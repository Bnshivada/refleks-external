// ================================================================
//  Refleks External v3.0 — main.cpp
//  ETS2 1.58 | ImGui Overlay | SCS Telemetry SDK
//
//  DERLEME:
//  - Visual Studio 2022 x64
//  - DirectX 11 SDK
//  - Linker: d3d11.lib dxgi.lib dwmapi.lib
//
//  KURULUM:
//  - scs-telemetry.dll → ETS2/bin/win_x64/plugins/
//  - ETS2'yi aç → haritaya gir → bu exe'yi yönetici olarak çalıştır
// ================================================================

#include <Windows.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <iostream>
#include <string>
#include <deque>
#include <thread>
#include <atomic>
#include <chrono>
#include <iomanip>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

#include "Memory.h"           // WriteProcessMemory wrapper (RPM/Damage yazma için)
#include "AutoScanner.h"      // Process bağlantısı (yazma için hala lazım)
#include "TelemetryReader.h"  // SCS SDK okuma — pattern yok, offset yok

// ================================================================
//  GLOBAL DEĞİŞKENLER
// ================================================================

// DirectX
static ID3D11Device*           g_pd3dDevice  = nullptr;
static ID3D11DeviceContext*     g_pd3dContext = nullptr;
static IDXGISwapChain*          g_pSwapChain  = nullptr;
static ID3D11RenderTargetView*  g_mainRTV     = nullptr;

// Pencere
HWND  g_overlayHWND = nullptr;
HWND  g_gameHWND    = nullptr;
int   g_screenW     = GetSystemMetrics(SM_CXSCREEN);
int   g_screenH     = GetSystemMetrics(SM_CYSCREEN);

// Bağlantı
AutoScanner     g_scanner;
TelemetryReader g_telemetry;
MemoryManager   g_mem;
bool            g_connected  = false;
bool            g_telOK      = false;
std::string     g_statusMsg  = "Baslatiliyor...";

// UI
bool  g_menuOpen    = true;
bool  g_noDamage    = false;
bool  g_rpmBoost    = false;
bool  g_fuelFreeze  = false;
bool  g_showESP     = true;

// RPM Boost
float g_targetRPM       = 2000.f;
float g_currentBoostRPM = 0.f;
float g_smoothSpeed     = 30.f;

// Grafik geçmişi
std::deque<float> g_speedHist;
std::deque<float> g_rpmHist;
const int HIST = 120;

// Araç verisi
struct TruckState {
    float speed      = 0.f;
    float rpm        = 0.f;
    float rpmMax     = 2500.f;
    float fuel       = 0.f;
    float fuelMax    = 400.f;
    float fuelPct    = 0.f;
    float damage     = 0.f;
    float throttle   = 0.f;
    float brake      = 0.f;
    int   gear       = 0;
    bool  engineOn   = false;
    std::string cargo    = "";
    std::string destCity = "";
    float cargoMass  = 0.f;
} g_truck;

float g_fps = 0.f;

// ================================================================
//  FORWARD DECLARATIONS
// ================================================================
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

bool CreateOverlay();
void DestroyOverlay();
bool CreateD3D(HWND);
void DestroyD3D();
void CreateRenderTarget();
void CleanupRenderTarget();

void RenderFrame();
void RenderMenu();
void RenderESP();
void RenderRPMBar(float current, float max);
void RenderGraphs();

void GameLoop();
void PushHist(std::deque<float>& dq, float v);
ImVec4 DmgColor(float p);
ImVec4 FuelColor(float p);

// ================================================================
//  WINMAIN
// ================================================================
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    SetConsoleTitleA("Refleks Debug");

    std::cout << "====================================\n";
    std::cout << "  Refleks External v3.0\n";
    std::cout << "  ETS2 1.58 | SCS Telemetry\n";
    std::cout << "====================================\n\n";

    if (!CreateOverlay()) return 1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 15.f);

    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding     = 8.f;
    s.FrameRounding      = 4.f;
    s.ItemSpacing        = ImVec2(8.f, 5.f);
    s.WindowPadding      = ImVec2(12.f, 10.f);
    s.Colors[ImGuiCol_WindowBg]      = ImVec4(0.07f, 0.07f, 0.09f, 0.93f);
    s.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.18f, 0.28f, 1.f);
    s.Colors[ImGuiCol_SliderGrab]    = ImVec4(0.26f, 0.59f, 0.98f, 1.f);
    s.Colors[ImGuiCol_CheckMark]     = ImVec4(0.2f,  0.95f, 0.5f,  1.f);
    s.Colors[ImGuiCol_Button]        = ImVec4(0.18f, 0.32f, 0.55f, 1.f);
    s.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.f);

    ImGui_ImplWin32_Init(g_overlayHWND);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);

    // GameLoop ayrı thread
    std::thread(GameLoop).detach();

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        if (GetAsyncKeyState(VK_INSERT) & 1) g_menuOpen  = !g_menuOpen;
        if (GetAsyncKeyState(VK_F6)     & 1) g_noDamage  = !g_noDamage;
        if (GetAsyncKeyState(VK_F7)     & 1) g_rpmBoost  = !g_rpmBoost;
        if (GetAsyncKeyState(VK_F8)     & 1) g_fuelFreeze= !g_fuelFreeze;
        if (GetAsyncKeyState(VK_END)    & 1) break;

        // Overlay'i ETS2 penceresinin üstüne kilitle
        if (g_gameHWND && IsWindow(g_gameHWND)) {
            RECT r;
            GetWindowRect(g_gameHWND, &r);
            SetWindowPos(g_overlayHWND, HWND_TOPMOST,
                r.left, r.top,
                r.right - r.left, r.bottom - r.top,
                SWP_NOACTIVATE);
        }

        RenderFrame();
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    DestroyD3D();
    DestroyOverlay();
    g_telemetry.Disconnect();
    g_scanner.Disconnect();
    return 0;
}

// ================================================================
//  GAME LOOP — ayrı thread
//  Telemetry okuma + memory yazma (RPM/Damage)
// ================================================================
void GameLoop() {

    // 1. Telemetry'e bağlan (plugin yüklüyse direkt olur)
    g_statusMsg = "Telemetry bekleniyor...";

    while (!g_telemetry.Connect()) {
        std::cout << "[GameLoop] Telemetry baglanti bekleniyor...\n";
        Sleep(2000);
    }

    g_telOK     = true;
    g_statusMsg = "Telemetry OK!";

    // 2. RPM/Damage yazmak için process'e de bağlan
    //    (Telemetry read-only, yazma için ReadProcessMemory lazım)
    g_statusMsg = "ETS2 process bekleniyor...";
    g_scanner.WaitAndConnect();
    g_mem.hProcess    = g_scanner.processHandle;
    g_mem.baseAddress = g_scanner.baseAddress;
    g_connected = true;
    g_statusMsg = "Hazir!";

    std::cout << "[GameLoop] Her sey hazir, dongu basliyor.\n";

    auto lastFrame = std::chrono::steady_clock::now();

    while (true) {

        // 60Hz sabit — CPU'yu boşa harcama
        auto now = std::chrono::steady_clock::now();
        float ms = std::chrono::duration<float, std::milli>(now - lastFrame).count();
        if (ms < 16.f) { Sleep(1); continue; }
        lastFrame = now;

        // Telemetry koptu mu?
        if (!g_telemetry.IsGameActive()) {
            g_statusMsg = "Oyun pause/kapali...";
            Sleep(500);
            continue;
        }

        // ---- Veri oku (shared memory — çok hızlı, syscall yok) ----
        g_truck.speed    = g_telemetry.GetSpeed();
        g_truck.rpm      = g_telemetry.GetRPM();
        g_truck.rpmMax   = g_telemetry.GetRPMMax();
        g_truck.fuel     = g_telemetry.GetFuel();
        g_truck.fuelMax  = g_telemetry.GetFuelMax();
        g_truck.fuelPct  = g_truck.fuelMax > 0.f
                           ? (g_truck.fuel / g_truck.fuelMax * 100.f) : 0.f;
        g_truck.damage   = g_telemetry.GetDamage();
        g_truck.throttle = g_telemetry.GetThrottle();
        g_truck.brake    = g_telemetry.GetBrake();
        g_truck.gear     = g_telemetry.GetGear();
        g_truck.engineOn = g_telemetry.GetEngineOn();
        g_truck.cargo    = g_telemetry.GetCargoName();
        g_truck.destCity = g_telemetry.GetDestCity();
        g_truck.cargoMass= g_telemetry.GetCargoMass();

        PushHist(g_speedHist, g_truck.speed);
        PushHist(g_rpmHist,   g_truck.rpm);

        // ---- Özellikler ----

        // No Damage — Memory.h ile hasar adresine 0 yaz
        if (g_noDamage && g_connected) {
            // Pointer zincirini çöz ve yaz
            uintptr_t dmgAddr = g_mem.ResolvePointer(
                DMG_BASE,
                DMG_OFFSETS,
                sizeof(DMG_OFFSETS) / sizeof(DWORD)
            );
            if (dmgAddr) g_mem.WriteFloat(dmgAddr, 0.f);
        }

        // RPM Boost
        if (g_rpmBoost && g_truck.engineOn && g_connected) {
            if (g_currentBoostRPM < g_targetRPM)
                g_currentBoostRPM += g_smoothSpeed;
            else
                g_currentBoostRPM = g_targetRPM;

            uintptr_t rpmAddr = g_mem.ResolvePointer(
                RPM_BASE,
                RPM_OFFSETS,
                sizeof(RPM_OFFSETS) / sizeof(DWORD)
            );
            if (rpmAddr) g_mem.WriteFloat(rpmAddr, g_currentBoostRPM);
        } else {
            g_currentBoostRPM = g_truck.rpm;
        }

        // Fuel Freeze
        if (g_fuelFreeze && g_connected) {
            uintptr_t fuelAddr = g_mem.ResolvePointer(
                FUEL_BASE,
                FUEL_OFFSETS,
                sizeof(FUEL_OFFSETS) / sizeof(DWORD)
            );
            if (fuelAddr) g_mem.WriteFloat(fuelAddr, g_truck.fuelMax * 0.99f);
        }
    }
}

// ================================================================
//  RENDER FRAME
// ================================================================
void RenderFrame() {
    static auto lt = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float>(now - lt).count();
    lt = now;
    g_fps = 1.f / (dt > 0.f ? dt : 0.001f);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (g_menuOpen) RenderMenu();
    if (g_showESP)  RenderESP();

    ImGui::Render();
    const float clr[4] = {};
    g_pd3dContext->OMSetRenderTargets(1, &g_mainRTV, nullptr);
    g_pd3dContext->ClearRenderTargetView(g_mainRTV, clr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_pSwapChain->Present(1, 0);
}

// ================================================================
//  ANA MENÜ
// ================================================================
void RenderMenu() {
    ImGui::SetNextWindowSize(ImVec2(430.f, 600.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(20.f, 20.f),    ImGuiCond_FirstUseEver);

    ImGui::Begin("Refleks External v3.0  |  ETS2 1.58", nullptr,
        ImGuiWindowFlags_NoCollapse);

    // Durum satırı
    ImVec4 stCol = g_connected
        ? ImVec4(0.2f,1.f,0.4f,1.f)
        : ImVec4(1.f,0.7f,0.f,1.f);
    ImGui::PushStyleColor(ImGuiCol_Text, stCol);
    ImGui::Text("[%s]  %s", g_connected ? "HAZIR" : "BEKLIYOR", g_statusMsg.c_str());
    ImGui::PopStyleColor();
    ImGui::Text("FPS: %.0f  |  INSERT:Menu  END:Cik", g_fps);
    ImGui::Separator();

    // ==========================
    //  ARAÇ VERİLERİ
    // ==========================
    if (ImGui::CollapsingHeader("Arac Verileri", ImGuiTreeNodeFlags_DefaultOpen)) {

        // Hız bar
        ImGui::Text("Hiz  :");
        ImGui::SameLine(70.f);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
            g_truck.speed > 110.f ? ImVec4(1.f,0.3f,0.3f,1.f) : ImVec4(0.2f,0.65f,1.f,1.f));
        char sb[32]; snprintf(sb, 32, "%.1f km/h", g_truck.speed);
        ImGui::ProgressBar(g_truck.speed / 150.f, ImVec2(-1.f, 16.f), sb);
        ImGui::PopStyleColor();

        // RPM bar
        ImGui::Text("RPM  :");
        ImGui::SameLine(70.f);
        float rNorm = g_truck.rpmMax > 0.f ? g_truck.rpm / g_truck.rpmMax : 0.f;
        rNorm = rNorm > 1.f ? 1.f : rNorm;
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
            rNorm > 0.85f ? ImVec4(1.f,0.4f,0.f,1.f) : ImVec4(0.4f,0.85f,0.3f,1.f));
        char rb[32]; snprintf(rb, 32, "%.0f / %.0f", g_truck.rpm, g_truck.rpmMax);
        ImGui::ProgressBar(rNorm, ImVec2(-1.f, 16.f), rb);
        ImGui::PopStyleColor();

        // Yakıt bar
        ImGui::Text("Yakit:");
        ImGui::SameLine(70.f);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, FuelColor(g_truck.fuelPct));
        char fb[48]; snprintf(fb, 48, "%.0f / %.0f L  (%.0f%%)",
            g_truck.fuel, g_truck.fuelMax, g_truck.fuelPct);
        ImGui::ProgressBar(g_truck.fuelPct / 100.f, ImVec2(-1.f, 16.f), fb);
        ImGui::PopStyleColor();

        // Hasar bar
        ImGui::Text("Hasar:");
        ImGui::SameLine(70.f);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, DmgColor(g_truck.damage));
        char db[32]; snprintf(db, 32, "%.1f%%", g_truck.damage);
        ImGui::ProgressBar(g_truck.damage / 100.f, ImVec2(-1.f, 16.f), db);
        ImGui::PopStyleColor();

        // Gaz / Fren / Debriyaj
        ImGui::Spacing();
        ImGui::Text("Gaz  : "); ImGui::SameLine(70.f);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f,0.8f,0.3f,1.f));
        ImGui::ProgressBar(g_truck.throttle, ImVec2(-1.f, 10.f), "");
        ImGui::PopStyleColor();

        ImGui::Text("Fren : "); ImGui::SameLine(70.f);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.9f,0.2f,0.2f,1.f));
        ImGui::ProgressBar(g_truck.brake, ImVec2(-1.f, 10.f), "");
        ImGui::PopStyleColor();

        // Vites + Motor
        ImGui::Spacing();
        ImGui::Text("Vites: ");
        ImGui::SameLine(70.f);
        if      (g_truck.gear == 0)  ImGui::TextColored(ImVec4(1.f,1.f,0.f,1.f), "Notr");
        else if (g_truck.gear < 0)   ImGui::TextColored(ImVec4(1.f,0.4f,0.4f,1.f), "Geri");
        else                         ImGui::TextColored(ImVec4(0.4f,1.f,0.4f,1.f), "%d. Vites", g_truck.gear);

        ImGui::SameLine(160.f);
        ImGui::Text("Motor: ");
        ImGui::SameLine();
        ImGui::TextColored(
            g_truck.engineOn ? ImVec4(0.2f,1.f,0.4f,1.f) : ImVec4(1.f,0.3f,0.3f,1.f),
            g_truck.engineOn ? "ACIK" : "KAPALI");

        // Kargo bilgisi
        if (!g_truck.cargo.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.7f,0.7f,1.f,1.f),
                "Kargo : %s  (%.0f kg)", g_truck.cargo.c_str(), g_truck.cargoMass);
            ImGui::TextColored(ImVec4(0.7f,0.7f,1.f,1.f),
                "Hedef : %s", g_truck.destCity.c_str());
        }
    }

    // ==========================
    //  ÖZELLİKLER
    // ==========================
    if (ImGui::CollapsingHeader("Ozellikler", ImGuiTreeNodeFlags_DefaultOpen)) {

        // No Damage
        ImGui::PushStyleColor(ImGuiCol_Text,
            g_noDamage ? ImVec4(0.2f,1.f,0.4f,1.f) : ImVec4(0.6f,0.6f,0.6f,1.f));
        ImGui::Checkbox("[F6] No Damage", &g_noDamage);
        ImGui::PopStyleColor();
        ImGui::SameLine(180.f);
        ImGui::TextDisabled("Hasar sifirlanir");

        ImGui::Spacing();

        // RPM Boost
        ImGui::PushStyleColor(ImGuiCol_Text,
            g_rpmBoost ? ImVec4(1.f,0.7f,0.2f,1.f) : ImVec4(0.6f,0.6f,0.6f,1.f));
        ImGui::Checkbox("[F7] RPM Boost", &g_rpmBoost);
        ImGui::PopStyleColor();

        if (g_rpmBoost) {
            ImGui::Indent(14.f);
            ImGui::SliderFloat("Hedef RPM##r", &g_targetRPM, 500.f, 3000.f, "%.0f");
            ImGui::Text("Hizli: ");
            ImGui::SameLine();
            if (ImGui::SmallButton("800"))  g_targetRPM = 800.f;  ImGui::SameLine();
            if (ImGui::SmallButton("1200")) g_targetRPM = 1200.f; ImGui::SameLine();
            if (ImGui::SmallButton("1800")) g_targetRPM = 1800.f; ImGui::SameLine();
            if (ImGui::SmallButton("2200")) g_targetRPM = 2200.f; ImGui::SameLine();
            if (ImGui::SmallButton("2500")) g_targetRPM = 2500.f;
            ImGui::SliderFloat("Gecis hizi##sm", &g_smoothSpeed, 5.f, 200.f, "%.0f/frame");

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f,0.7f,0.2f,1.f));
            ImGui::Text("Simdi: %.0f RPM", g_currentBoostRPM);
            ImGui::PopStyleColor();

            RenderRPMBar(g_currentBoostRPM, g_truck.rpmMax > 0.f ? g_truck.rpmMax : 2500.f);
            ImGui::Unindent(14.f);
        }

        ImGui::Spacing();

        // Fuel Freeze
        ImGui::PushStyleColor(ImGuiCol_Text,
            g_fuelFreeze ? ImVec4(0.3f,0.8f,1.f,1.f) : ImVec4(0.6f,0.6f,0.6f,1.f));
        ImGui::Checkbox("[F8] Fuel Freeze", &g_fuelFreeze);
        ImGui::PopStyleColor();
        ImGui::SameLine(180.f);
        ImGui::TextDisabled("Yakit %%99 sabit");

        ImGui::Spacing();
        ImGui::Checkbox("ESP HUD (kose gosterge)", &g_showESP);
    }

    // ==========================
    //  GRAFİK
    // ==========================
    if (ImGui::CollapsingHeader("Grafik")) {
        RenderGraphs();
    }

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f,0.25f,0.25f,1.f));
    ImGui::TextWrapped("TruckersMP'de kullanmak KESİN BAN!");
    ImGui::PopStyleColor();

    ImGui::End();
}

// ================================================================
//  RPM BAR
// ================================================================
void RenderRPMBar(float current, float max) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = 18.f;

    dl->AddRectFilled(p, ImVec2(p.x+w, p.y+h), IM_COL32(25,25,25,220), 4.f);

    float pct = max > 0.f ? current/max : 0.f;
    pct = pct > 1.f ? 1.f : pct;

    if (pct > 0.f) {
        ImU32 col;
        if      (pct < 0.6f) col = IM_COL32(60,  200, 60,  255);
        else if (pct < 0.85f)col = IM_COL32(220, 180, 40,  255);
        else                  col = IM_COL32(220, 60,  40,  255);
        dl->AddRectFilled(p, ImVec2(p.x+w*pct, p.y+h), col, 4.f);
    }

    dl->AddRect(p, ImVec2(p.x+w, p.y+h), IM_COL32(80,80,80,200), 4.f);

    char buf[32];
    snprintf(buf, 32, "%.0f / %.0f RPM", current, max);
    ImVec2 ts = ImGui::CalcTextSize(buf);
    dl->AddText(ImVec2(p.x + w/2.f - ts.x/2.f, p.y+2.f), IM_COL32(255,255,255,255), buf);

    ImGui::Dummy(ImVec2(w, h+4.f));
}

// ================================================================
//  GRAFİKLER
// ================================================================
void RenderGraphs() {
    static float sa[120] = {}, ra[120] = {};
    int i = 0;
    for (float v : g_speedHist) sa[i++] = v;
    i = 0;
    for (float v : g_rpmHist)   ra[i++] = v;

    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.2f,0.65f,1.f,1.f));
    ImGui::PlotLines("Hiz##g", sa, HIST, 0, nullptr, 0.f, 160.f, ImVec2(-1.f, 55.f));
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.f,0.6f,0.2f,1.f));
    ImGui::PlotLines("RPM##g", ra, HIST, 0, nullptr, 0.f, 3000.f, ImVec2(-1.f, 55.f));
    ImGui::PopStyleColor();
}

// ================================================================
//  ESP — sağ köşe mini HUD
// ================================================================
void RenderESP() {
    ImGui::SetNextWindowPos(ImVec2(g_screenW - 210.f, 15.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(195.f, 155.f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55f);

    ImGui::Begin("##esp", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoScrollbar);

    ImGui::TextColored(ImVec4(0.2f,0.65f,1.f,1.f), "HIZ   %.1f km/h", g_truck.speed);
    ImGui::TextColored(ImVec4(0.4f,0.85f,0.3f,1.f),"RPM   %.0f",       g_truck.rpm);
    ImGui::TextColored(ImVec4(0.3f,0.8f,1.f,1.f),  "YAKIT %.0f L",     g_truck.fuel);
    ImGui::TextColored(DmgColor(g_truck.damage),    "HASAR %.1f%%",     g_truck.damage);

    ImGui::Separator();
    // Aktif özellikler
    if (g_noDamage)  ImGui::TextColored(ImVec4(0.2f,1.f,0.4f,1.f), "[F6] NoDmg");
    if (g_rpmBoost)  ImGui::TextColored(ImVec4(1.f,0.7f,0.2f,1.f), "[F7] RPM");
    if (g_fuelFreeze)ImGui::TextColored(ImVec4(0.3f,0.8f,1.f,1.f), "[F8] Fuel");

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.45f,0.45f,0.45f,1.f), "INSERT = Menu");
    ImGui::End();
}

// ================================================================
//  YARDIMCI
// ================================================================
void PushHist(std::deque<float>& dq, float v) {
    dq.push_back(v);
    if ((int)dq.size() > HIST) dq.pop_front();
}

ImVec4 DmgColor(float p) {
    if (p < 20.f) return ImVec4(0.2f,1.f,0.4f,1.f);
    if (p < 50.f) return ImVec4(1.f,0.75f,0.f,1.f);
    return ImVec4(1.f,0.2f,0.2f,1.f);
}

ImVec4 FuelColor(float p) {
    if (p > 40.f) return ImVec4(0.2f,0.65f,1.f,1.f);
    if (p > 15.f) return ImVec4(1.f,0.75f,0.f,1.f);
    return ImVec4(1.f,0.25f,0.25f,1.f);
}

// ================================================================
//  DIRECTX + PENCERE
// ================================================================
bool CreateOverlay() {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.lpszClassName = L"RefleksOverlay";
    RegisterClassExW(&wc);

    g_overlayHWND = CreateWindowExW(
        WS_EX_TOPMOST|WS_EX_TRANSPARENT|WS_EX_LAYERED,
        L"RefleksOverlay", L"Refleks External",
        WS_POPUP, 0, 0, g_screenW, g_screenH,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr);

    if (!g_overlayHWND) return false;
    SetLayeredWindowAttributes(g_overlayHWND, RGB(0,0,0), 0, LWA_COLORKEY);

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
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = g_screenW;
    sd.BufferDesc.Height                  = g_screenH;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = hwnd;
    sd.SampleDesc.Count                   = 1;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL fl;
    const D3D_FEATURE_LEVEL fls[] = { D3D_FEATURE_LEVEL_11_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        0, fls, 1, D3D11_SDK_VERSION,
        &sd, &g_pSwapChain, &g_pd3dDevice, &fl, &g_pd3dContext);

    if (FAILED(hr)) return false;
    CreateRenderTarget();
    return true;
}

void CreateRenderTarget() {
    ID3D11Texture2D* back = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&back));
    if (back) { g_pd3dDevice->CreateRenderTargetView(back, nullptr, &g_mainRTV); back->Release(); }
}

void CleanupRenderTarget() {
    if (g_mainRTV) { g_mainRTV->Release(); g_mainRTV = nullptr; }
}

void DestroyD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain)  { g_pSwapChain->Release();  g_pSwapChain  = nullptr; }
    if (g_pd3dContext) { g_pd3dContext->Release();  g_pd3dContext = nullptr; }
    if (g_pd3dDevice)  { g_pd3dDevice->Release();   g_pd3dDevice  = nullptr; }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return true;
    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice && wp != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, LOWORD(lp), HIWORD(lp), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
            g_screenW = LOWORD(lp); g_screenH = HIWORD(lp);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
