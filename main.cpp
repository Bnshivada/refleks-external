// ================================================================
//  Refleks External v3.0 — main.cpp
//  ETS2 1.58 | ImGui Overlay | Pattern Scanner
//  
//  DERLEME:

//  - Visual Studio 2022 x64
//  - DirectX 11 SDK
//  - imgui/ klasörüne ImGui dosyalarını koy
//  - Linker: d3d11.lib, dxgi.lib, dwmapi.lib
// ================================================================

#include <Windows.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <chrono>
#include <deque>

// ImGui
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

// Kendi headerlarımız
#include "Memory.h"
#include "PatternScanner.h"
#include "AutoScanner.h"
#include "AutoFinder.h"

// ================================================================
//  GLOBAL DEĞİŞKENLER
// ================================================================

// DirectX
static ID3D11Device*           g_pd3dDevice    = nullptr;
static ID3D11DeviceContext*     g_pd3dContext   = nullptr;
static IDXGISwapChain*          g_pSwapChain    = nullptr;
static ID3D11RenderTargetView*  g_mainRTV       = nullptr;

// Overlay penceresi
HWND  g_overlayHWND  = nullptr;
HWND  g_gameHWND     = nullptr;
int   g_screenW      = GetSystemMetrics(SM_CXSCREEN);
int   g_screenH      = GetSystemMetrics(SM_CYSCREEN);

// Oyun bağlantısı
AutoScanner  g_scanner;
AutoFinder*  g_finder   = nullptr;
bool         g_connected = false;

// UI state
bool g_menuOpen     = true;   // INSERT ile aç/kapat
bool g_noDamage     = false;
bool g_rpmBoost     = false;
bool g_speedHack    = false;
bool g_fuelFreeze   = false;
bool g_showESP      = false;  // Hız/RPM ekran üstü gösterge

// RPM Boost ayarları
float g_targetRPM      = 5000.f;
float g_rpmSmoothSpeed = 50.f;   // Her frame ne kadar artırılacak
float g_currentBoostRPM = 0.f;   // Smooth geçiş için

// Speed hack
float g_speedMultiplier = 1.5f;

// Grafik için geçmiş veri
std::deque<float> g_speedHistory;
std::deque<float> g_rpmHistory;
const int HISTORY_SIZE = 100;

// Anlık veri
struct TruckState {
    float speed     = 0.f;
    float rpm       = 0.f;
    float fuel      = 0.f;
    float fuelMax   = 400.f;
    float damage    = 0.f;
    bool  engineOn  = false;
    float posX      = 0.f;
    float posZ      = 0.f;
    // Hesaplanan
    float fuelPct   = 0.f;
    float damagePct = 0.f;
} g_truck;

// Tarama durumu
std::atomic<bool>  g_scanning(false);
std::atomic<bool>  g_scanDone(false);
std::string        g_scanStatus = "Bekleniyor...";

// FPS sayacı
float g_fps = 0.f;

// ================================================================
//  FORWARD DECLARATIONS
// ================================================================
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

bool  CreateOverlay();
void  DestroyOverlay();
bool  CreateD3D(HWND hwnd);
void  DestroyD3D();
void  CreateRenderTarget();
void  CleanupRenderTarget();

void  RenderFrame();
void  RenderMenu();
void  RenderESP();
void  RenderRPMBar();
void  RenderSpeedGraph();

void  GameLoop();
void  ScanThread();

// Yardımcı
void  PushHistory(std::deque<float>& dq, float val);
ImVec4 DamageColor(float pct);
ImVec4 FuelColor(float pct);

// ================================================================
//  MAIN
// ================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // Konsol penceresi (debug için)
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    SetConsoleTitleA("Refleks External - Debug");

    std::cout << "====================================\n";
    std::cout << "  Refleks External v3.0\n";
    std::cout << "  ETS2 1.58 | Pattern Scanner\n";
    std::cout << "====================================\n\n";

    // Overlay oluştur
    if (!CreateOverlay()) {
        std::cout << "[HATA] Overlay oluşturulamadı!\n";
        return 1;
    }

    // ImGui başlat
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    // Font — biraz büyük, overlay'de okunması için
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.f);

    // Tema
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 8.f;
    style.FrameRounding     = 4.f;
    style.ItemSpacing       = ImVec2(8.f, 6.f);
    style.WindowPadding     = ImVec2(12.f, 12.f);
    style.Colors[ImGuiCol_WindowBg]     = ImVec4(0.08f, 0.08f, 0.10f, 0.92f);
    style.Colors[ImGuiCol_TitleBg]      = ImVec4(0.15f, 0.15f, 0.20f, 1.f);
    style.Colors[ImGuiCol_TitleBgActive]= ImVec4(0.20f, 0.20f, 0.28f, 1.f);
    style.Colors[ImGuiCol_SliderGrab]   = ImVec4(0.26f, 0.59f, 0.98f, 1.f);
    style.Colors[ImGuiCol_CheckMark]    = ImVec4(0.26f, 0.98f, 0.52f, 1.f);
    style.Colors[ImGuiCol_Button]       = ImVec4(0.20f, 0.35f, 0.60f, 1.f);
    style.Colors[ImGuiCol_ButtonHovered]= ImVec4(0.26f, 0.59f, 0.98f, 1.f);

    ImGui_ImplWin32_Init(g_overlayHWND);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);

    // ETS2 bağlantı thread'i başlat
    std::thread gameThread(GameLoop);
    gameThread.detach();

    // ---- Ana Render Loop ----
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        // INSERT → menü toggle
        if (GetAsyncKeyState(VK_INSERT) & 1)
            g_menuOpen = !g_menuOpen;

        // END → çık
        if (GetAsyncKeyState(VK_END) & 1)
            break;

        // Overlay pencereyi oyunun üstüne hizala
        if (g_gameHWND && IsWindow(g_gameHWND)) {
            RECT r;
            GetWindowRect(g_gameHWND, &r);
            SetWindowPos(g_overlayHWND, HWND_TOPMOST,
                r.left, r.top,
                r.right - r.left,
                r.bottom - r.top,
                SWP_NOACTIVATE);
        }

        RenderFrame();
    }

    // Temizlik
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    DestroyD3D();
    DestroyOverlay();

    if (g_finder) delete g_finder;
    g_scanner.Disconnect();

    return 0;
}

// ================================================================
//  OYUN LOOP — Ayrı thread'de çalışır
//  Memory okuma/yazma işlemleri burada
// ================================================================
void GameLoop() {
    // 1. ETS2'ye bağlan
    g_scanStatus = "ETS2 bekleniyor...";
    g_scanner.WaitAndConnect();
    g_connected  = true;
    g_scanStatus = "Bagli! Tarama bekleniyor...";

    // 2. Finder oluştur
    g_finder = new AutoFinder(g_scanner.processHandle);

    // 3. Pattern tarama başlat
    g_scanning   = true;
    g_scanStatus = "Pattern taranıyor...";
    std::cout << "[GameLoop] Pattern tarama başlıyor...\n";

    bool ok = g_finder->ScanAll();

    g_scanning = false;
    g_scanDone = true;
    g_scanStatus = ok ? "Hazir! Tum adresler bulundu." : "HATA: Bazi adresler bulunamadi.";
    std::cout << "[GameLoop] Tarama tamamlandı: " << (ok ? "OK" : "FAIL") << "\n";

    // 4. Ana veri döngüsü
    while (g_scanner.IsStillRunning()) {

        if (!g_scanDone || !g_finder->isReady) {
            Sleep(100);
            continue;
        }

        // Veri oku
        g_truck.speed    = g_finder->ReadSpeed();
        g_truck.rpm      = g_finder->ReadRPM();
        g_truck.fuel     = g_finder->ReadFuel();
        g_truck.fuelMax  = g_finder->ReadFuelMax();
        g_truck.damage   = g_finder->ReadDamage();
        g_truck.engineOn = g_finder->ReadEngineOn();
        g_truck.fuelPct  = (g_truck.fuelMax > 0.f)
                           ? (g_truck.fuel / g_truck.fuelMax * 100.f)
                           : 0.f;
        g_truck.damagePct = g_truck.damage;

        // Grafik geçmişi güncelle
        PushHistory(g_speedHistory, g_truck.speed);
        PushHistory(g_rpmHistory,   g_truck.rpm);

        // ---- Özellikler Uygula ----

        // No Damage
        if (g_noDamage)
            g_finder->WriteNoDamage();

        // RPM Boost — smooth geçiş
        if (g_rpmBoost && g_truck.engineOn) {
            // Hedef RPM'e kademeli yaklaş
            if (g_currentBoostRPM < g_targetRPM)
                g_currentBoostRPM += g_rpmSmoothSpeed;
            else
                g_currentBoostRPM = g_targetRPM;

            g_finder->WriteRPM(g_currentBoostRPM);
        } else {
            g_currentBoostRPM = g_truck.rpm;  // Sıfırla
        }

        // Fuel Freeze — yakıt miktarını dondur
        if (g_fuelFreeze)
            g_finder->WriteFuel(g_truck.fuelMax * 0.99f);  // %99'da tut

        Sleep(16);  // ~60Hz
    }

    // Oyun kapandı
    g_connected = false;
    g_scanDone  = false;
    g_scanStatus = "ETS2 kapandi!";
    std::cout << "[GameLoop] ETS2 kapandı.\n";
}

// ================================================================
//  RENDER FRAME
// ================================================================
void RenderFrame() {
    // FPS hesapla
    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float>(now - lastTime).count();
    lastTime = now;
    g_fps = 1.f / (dt > 0.f ? dt : 0.001f);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (g_menuOpen) {
        RenderMenu();
    }

    // ESP her zaman göster (menü kapalıyken de)
    if (g_showESP || !g_menuOpen) {
        RenderESP();
    }

    ImGui::Render();

    const float clear[4] = { 0.f, 0.f, 0.f, 0.f };
    g_pd3dContext->OMSetRenderTargets(1, &g_mainRTV, nullptr);
    g_pd3dContext->ClearRenderTargetView(g_mainRTV, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_pSwapChain->Present(1, 0);
}

// ================================================================
//  ANA MENÜ — ImGui penceresi
// ================================================================
void RenderMenu() {
    ImGui::SetNextWindowSize(ImVec2(420.f, 580.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(20.f, 20.f), ImGuiCond_FirstUseEver);

    ImGui::Begin("Refleks External v3.0  |  ETS2 1.58", nullptr,
        ImGuiWindowFlags_NoCollapse);

    // ---- Bağlantı Durumu ----
    ImGui::PushStyleColor(ImGuiCol_Text,
        g_connected
            ? (g_scanDone ? ImVec4(0.2f,1.f,0.4f,1.f) : ImVec4(1.f,0.8f,0.f,1.f))
            : ImVec4(1.f,0.3f,0.3f,1.f));
    ImGui::Text("[%s]  %s",
        g_connected ? (g_scanDone ? "HAZIR" : "TARANIYOR") : "BAGLANTI YOK",
        g_scanStatus.c_str());
    ImGui::PopStyleColor();

    ImGui::Text("FPS: %.0f  |  [INSERT] Menu  [END] Cik", g_fps);
    ImGui::Separator();

    if (!g_connected || !g_scanDone) {
        if (g_scanning) {
            // Tarama animasyonu
            static float t = 0.f;
            t += ImGui::GetIO().DeltaTime * 2.f;
            int dots = (int)t % 4;
            std::string anim = "Taranıyor" + std::string(dots, '.');
            ImGui::Text("%s", anim.c_str());
        }
        ImGui::End();
        return;
    }

    // ============================
    //  BÖLÜM 1: ARAÇ VERİLERİ
    // ============================
    if (ImGui::CollapsingHeader("Arac Verileri", ImGuiTreeNodeFlags_DefaultOpen)) {

        // Hız göstergesi
        ImGui::Text("Hiz:");
        ImGui::SameLine(80.f);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
            g_truck.speed > 120.f ? ImVec4(1.f,0.3f,0.3f,1.f) : ImVec4(0.2f,0.7f,1.f,1.f));
        char speedBuf[32];
        snprintf(speedBuf, sizeof(speedBuf), "%.1f km/h", g_truck.speed);
        ImGui::ProgressBar(g_truck.speed / 160.f, ImVec2(-1.f, 14.f), speedBuf);
        ImGui::PopStyleColor();

        // RPM göstergesi
        ImGui::Text("RPM:");
        ImGui::SameLine(80.f);
        float rpmNorm = g_truck.rpm / 2500.f;  // ETS2 max RPM ~2500
        rpmNorm = rpmNorm > 1.f ? 1.f : rpmNorm;
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
            rpmNorm > 0.85f ? ImVec4(1.f,0.5f,0.f,1.f) : ImVec4(0.5f,0.9f,0.3f,1.f));
        char rpmBuf[32];
        snprintf(rpmBuf, sizeof(rpmBuf), "%.0f RPM", g_truck.rpm);
        ImGui::ProgressBar(rpmNorm, ImVec2(-1.f, 14.f), rpmBuf);
        ImGui::PopStyleColor();

        // Yakıt
        ImGui::Text("Yakıt:");
        ImGui::SameLine(80.f);
        ImVec4 fuelCol = FuelColor(g_truck.fuelPct);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, fuelCol);
        char fuelBuf[32];
        snprintf(fuelBuf, sizeof(fuelBuf), "%.0f / %.0f L (%.0f%%)",
            g_truck.fuel, g_truck.fuelMax, g_truck.fuelPct);
        ImGui::ProgressBar(g_truck.fuelPct / 100.f, ImVec2(-1.f, 14.f), fuelBuf);
        ImGui::PopStyleColor();

        // Hasar
        ImGui::Text("Hasar:");
        ImGui::SameLine(80.f);
        ImVec4 dmgCol = DamageColor(g_truck.damagePct);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, dmgCol);
        char dmgBuf[32];
        snprintf(dmgBuf, sizeof(dmgBuf), "%.1f%%", g_truck.damagePct);
        ImGui::ProgressBar(g_truck.damagePct / 100.f, ImVec2(-1.f, 14.f), dmgBuf);
        ImGui::PopStyleColor();

        // Motor durumu
        ImGui::Text("Motor: ");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text,
            g_truck.engineOn ? ImVec4(0.2f,1.f,0.4f,1.f) : ImVec4(1.f,0.3f,0.3f,1.f));
        ImGui::Text(g_truck.engineOn ? "ACIK" : "KAPALI");
        ImGui::PopStyleColor();
    }

    // ============================
    //  BÖLÜM 2: ÖZELLİKLER
    // ============================
    if (ImGui::CollapsingHeader("Ozellikler", ImGuiTreeNodeFlags_DefaultOpen)) {

        // --- No Damage ---
        ImGui::PushStyleColor(ImGuiCol_Text,
            g_noDamage ? ImVec4(0.2f,1.f,0.4f,1.f) : ImVec4(0.7f,0.7f,0.7f,1.f));
        ImGui::Checkbox("[F6] No Damage", &g_noDamage);
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Hasar sifirlanir. Her frame 0.0 yazilir.");

        ImGui::Spacing();

        // --- RPM Boost ---
        ImGui::PushStyleColor(ImGuiCol_Text,
            g_rpmBoost ? ImVec4(1.f,0.7f,0.2f,1.f) : ImVec4(0.7f,0.7f,0.7f,1.f));
        ImGui::Checkbox("[F7] RPM Boost", &g_rpmBoost);
        ImGui::PopStyleColor();

        if (g_rpmBoost) {
            ImGui::Indent(16.f);

            // Hedef RPM slider
            ImGui::Text("Hedef RPM:");
            ImGui::SliderFloat("##rpm", &g_targetRPM, 500.f, 10000.f, "%.0f RPM");

            // Hızlı seçim butonları
            ImGui::Text("Hizli sec:");
            ImGui::SameLine();
            if (ImGui::SmallButton("1K"))  g_targetRPM = 1000.f;
            ImGui::SameLine();
            if (ImGui::SmallButton("2.5K")) g_targetRPM = 2500.f;
            ImGui::SameLine();
            if (ImGui::SmallButton("5K"))  g_targetRPM = 5000.f;
            ImGui::SameLine();
            if (ImGui::SmallButton("7.5K")) g_targetRPM = 7500.f;
            ImGui::SameLine();
            if (ImGui::SmallButton("10K")) g_targetRPM = 10000.f;

            // Smooth hız
            ImGui::Text("Gecis hizi:");
            ImGui::SliderFloat("##smooth", &g_rpmSmoothSpeed, 10.f, 500.f, "%.0f/frame");

            // Şu anki boost RPM göster
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f,0.7f,0.2f,1.f));
            ImGui::Text("Simdi yazilan: %.0f RPM", g_currentBoostRPM);
            ImGui::PopStyleColor();

            // RPM Bar (0-10K)
            RenderRPMBar();

            ImGui::Unindent(16.f);
        }

        ImGui::Spacing();

        // --- Fuel Freeze ---
        ImGui::PushStyleColor(ImGuiCol_Text,
            g_fuelFreeze ? ImVec4(0.3f,0.8f,1.f,1.f) : ImVec4(0.7f,0.7f,0.7f,1.f));
        ImGui::Checkbox("[F8] Fuel Freeze", &g_fuelFreeze);
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Yakıt %%99'da sabit kalır.");

        ImGui::Spacing();

        // --- Speed Hack ---
        ImGui::PushStyleColor(ImGuiCol_Text,
            g_speedHack ? ImVec4(1.f,0.4f,0.8f,1.f) : ImVec4(0.7f,0.7f,0.7f,1.f));
        ImGui::Checkbox("[F9] Speed Multiplier", &g_speedHack);
        ImGui::PopStyleColor();

        if (g_speedHack) {
            ImGui::Indent(16.f);
            ImGui::SliderFloat("Carpan##sp", &g_speedMultiplier, 1.f, 5.f, "x%.1f");
            ImGui::Unindent(16.f);
        }

        ImGui::Spacing();

        // --- ESP toggle ---
        ImGui::Checkbox("ESP Gosterge (hiz/rpm ekranda)", &g_showESP);
    }

    // ============================
    //  BÖLÜM 3: GRAFİK
    // ============================
    if (ImGui::CollapsingHeader("Grafik")) {
        RenderSpeedGraph();
    }

    // ============================
    //  BÖLÜM 4: YENIDEN TARA
    // ============================
    ImGui::Separator();
    if (ImGui::Button("Yeniden Tara (Oyun guncellendiyse)")) {
        if (!g_scanning) {
            g_scanDone = false;
            g_scanning = true;
            std::thread([]() {
                bool ok = g_finder->Rescan();
                g_scanning = false;
                g_scanDone = true;
                g_scanStatus = ok ? "Yeniden tarama OK!" : "Yeniden tarama HATA!";
            }).detach();
        }
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f,0.3f,0.3f,1.f));
    ImGui::TextWrapped("UYARI: TruckersMP / Multiplayer'da kullanmak BAN ile sonuclanir!");
    ImGui::PopStyleColor();

    ImGui::End();
}

// ================================================================
//  RPM ÇUBUK GÖSTERGESİ
//  0'dan hedef RPM'e kadar renkli bir bar
// ================================================================
void RenderRPMBar() {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = 20.f;

    // Arka plan
    draw->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h),
        IM_COL32(30,30,30,200), 4.f);

    // Dolu kısım — renk RPM'e göre değişir
    float pct = g_currentBoostRPM / 10000.f;
    pct = pct > 1.f ? 1.f : pct;

    // Kırmızı → sarı → yeşil renk geçişi
    ImU32 barColor;
    if (pct < 0.5f)
        barColor = IM_COL32(
            (int)(pct * 2 * 255), 200, 50, 255);
    else
        barColor = IM_COL32(255,
            (int)((1.f - pct) * 2 * 200), 50, 255);

    if (pct > 0.f) {
        draw->AddRectFilled(
            pos,
            ImVec2(pos.x + w * pct, pos.y + h),
            barColor, 4.f);
    }

    // Çerçeve
    draw->AddRect(pos, ImVec2(pos.x + w, pos.y + h),
        IM_COL32(100,100,100,200), 4.f);

    // Yazı
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f / 10000 RPM", g_currentBoostRPM);
    draw->AddText(
        ImVec2(pos.x + w/2 - 50.f, pos.y + 3.f),
        IM_COL32(255,255,255,255), buf);

    ImGui::Dummy(ImVec2(w, h + 4.f));
}

// ================================================================
//  HIZ GRAFİĞİ
// ================================================================
void RenderSpeedGraph() {
    // Speed history → float array
    static float speedArr[HISTORY_SIZE] = {};
    static float rpmArr[HISTORY_SIZE]   = {};

    int i = 0;
    for (float v : g_speedHistory) speedArr[i++] = v;
    i = 0;
    for (float v : g_rpmHistory)   rpmArr[i++]   = v;

    ImGui::Text("Hız Grafiği (son 100 frame)");
    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.2f,0.7f,1.f,1.f));
    ImGui::PlotLines("##speed", speedArr, HISTORY_SIZE, 0,
        nullptr, 0.f, 160.f, ImVec2(-1.f, 60.f));
    ImGui::PopStyleColor();

    ImGui::Text("RPM Grafiği");
    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.f,0.7f,0.2f,1.f));
    ImGui::PlotLines("##rpm", rpmArr, HISTORY_SIZE, 0,
        nullptr, 0.f, 3000.f, ImVec2(-1.f, 60.f));
    ImGui::PopStyleColor();
}

// ================================================================
//  ESP — Ekranın köşesinde küçük HUD
//  Menü kapalıyken bile görünür
// ================================================================
void RenderESP() {
    ImGui::SetNextWindowPos(ImVec2(g_screenW - 220.f, 20.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(200.f, 130.f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.6f);

    ImGui::Begin("##esp", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize   |
        ImGuiWindowFlags_NoMove     |
        ImGuiWindowFlags_NoInputs);

    ImGui::Text("HIZ  : %.1f km/h", g_truck.speed);
    ImGui::Text("RPM  : %.0f",      g_truck.rpm);
    ImGui::Text("YAKIT: %.0f L",    g_truck.fuel);

    ImGui::PushStyleColor(ImGuiCol_Text, DamageColor(g_truck.damagePct));
    ImGui::Text("HASAR: %.1f%%",    g_truck.damagePct);
    ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f,0.5f,0.5f,1.f));
    ImGui::Text("[INSERT] Menu");
    ImGui::PopStyleColor();

    ImGui::End();
}

// ================================================================
//  YARDIMCI FONKSİYONLAR
// ================================================================

void PushHistory(std::deque<float>& dq, float val) {
    dq.push_back(val);
    if ((int)dq.size() > HISTORY_SIZE)
        dq.pop_front();
}

ImVec4 DamageColor(float pct) {
    if (pct < 20.f) return ImVec4(0.2f, 1.f, 0.4f, 1.f);  // Yeşil
    if (pct < 50.f) return ImVec4(1.f, 0.8f, 0.f, 1.f);   // Sarı
    return ImVec4(1.f, 0.2f, 0.2f, 1.f);                   // Kırmızı
}

ImVec4 FuelColor(float pct) {
    if (pct > 40.f) return ImVec4(0.2f, 0.7f, 1.f, 1.f);  // Mavi
    if (pct > 15.f) return ImVec4(1.f, 0.8f, 0.f, 1.f);   // Sarı
    return ImVec4(1.f, 0.3f, 0.3f, 1.f);                   // Kırmızı (az yakıt)
}

// ================================================================
//  DIRECTX + PENCERE KURULUMU
// ================================================================

bool CreateOverlay() {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.lpszClassName = L"RefleksOverlay";
    RegisterClassExW(&wc);

    g_overlayHWND = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
        L"RefleksOverlay", L"Refleks External",
        WS_POPUP,
        0, 0, g_screenW, g_screenH,
        nullptr, nullptr,
        GetModuleHandle(nullptr), nullptr);

    if (!g_overlayHWND) return false;

    // Arka plan tamamen şeffaf
    SetLayeredWindowAttributes(g_overlayHWND, RGB(0,0,0), 0, LWA_COLORKEY);

    // ETS2 penceresi bul
    g_gameHWND = FindWindowA(nullptr, "Euro Truck Simulator 2");

    if (!CreateD3D(g_overlayHWND)) return false;

    ShowWindow(g_overlayHWND, SW_SHOW);
    UpdateWindow(g_overlayHWND);
    return true;
}

void DestroyOverlay() {
    DestroyWindow(g_overlayHWND);
    UnregisterClassW(L"RefleksOverlay", GetModuleHandle(nullptr));
}

bool CreateD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount          = 2;
    sd.BufferDesc.Width     = g_screenW;
    sd.BufferDesc.Height    = g_screenH;
    sd.BufferDesc.Format    = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage          = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow         = hwnd;
    sd.SampleDesc.Count     = 1;
    sd.Windowed             = TRUE;
    sd.SwapEffect           = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        0, levels, 1, D3D11_SDK_VERSION,
        &sd, &g_pSwapChain,
        &g_pd3dDevice, &featureLevel,
        &g_pd3dContext);

    if (FAILED(hr)) return false;

    CreateRenderTarget();
    return true;
}

void CreateRenderTarget() {
    ID3D11Texture2D* back = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&back));
    if (back) {
        g_pd3dDevice->CreateRenderTargetView(back, nullptr, &g_mainRTV);
        back->Release();
    }
}

void CleanupRenderTarget() {
    if (g_mainRTV) { g_mainRTV->Release(); g_mainRTV = nullptr; }
}

void DestroyD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dContext){ g_pd3dContext->Release(); g_pd3dContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release();  g_pd3dDevice = nullptr; }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice && wp != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, LOWORD(lp), HIWORD(lp),
                DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
            g_screenW = LOWORD(lp);
            g_screenH = HIWORD(lp);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
