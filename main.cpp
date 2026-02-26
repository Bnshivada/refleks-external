// ================================================
//   REFLEKS EXTERNAL v3.0 — main.cpp
//   ImGui + DirectX11 overlay
//   External memory okuma/yazma
// ================================================

// ImGui başlıkları (imgui klasöründen gelir)
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

#include <d3d11.h>
#include <dwmapi.h>
#include <thread>
#include <atomic>
#include "Memory.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")

// ================================================
//   GLOBAl DEĞİŞKENLER
// ================================================

// DirectX
ID3D11Device*           g_pd3dDevice           = nullptr;
ID3D11DeviceContext*    g_pd3dDeviceContext     = nullptr;
IDXGISwapChain*         g_pSwapChain            = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView  = nullptr;
HWND                    g_hwnd                  = nullptr;

// Memory
MemoryManager g_mem;
GameData      g_gameData;

// Hile durumları
std::atomic<bool> g_rpmBoostActive  = false;
std::atomic<bool> g_noDamageActive  = false;
float             g_rpmTarget       = 1000.0f;   // Bar'dan ayarlanan hedef RPM
float             g_maxRpmSeen      = 0.0f;      // O oturumda görülen max RPM

// Pencere
bool g_menuOpen  = true;
int  g_winWidth  = 420;
int  g_winHeight = 320;

// Forward declarations
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);


// ================================================
//   OYUN VERİSİ OKUMA THREAD'İ
//   Ana UI thread'inden bağımsız çalışır
//   Her 16ms'de bir (60fps) memory okur
// ================================================
void GameDataThread() {
    while (true) {
        if (!g_mem.IsAttached()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        // --- RPM oku ---
        uintptr_t rpmAddr = g_mem.ResolvePointer(
            RPM_BASE,
            RPM_OFFSETS,
            sizeof(RPM_OFFSETS) / sizeof(DWORD)
        );

        // --- Hız oku ---
        uintptr_t spdAddr = g_mem.ResolvePointer(
            SPD_BASE,
            SPD_OFFSETS,
            sizeof(SPD_OFFSETS) / sizeof(DWORD)
        );

        // --- Hasar oku ---
        uintptr_t dmgAddr = g_mem.ResolvePointer(
            DMG_BASE,
            DMG_OFFSETS,
            sizeof(DMG_OFFSETS) / sizeof(DWORD)
        );

        // --- Yakıt oku ---
        uintptr_t fuelAddr = g_mem.ResolvePointer(
            FUEL_BASE,
            FUEL_OFFSETS,
            sizeof(FUEL_OFFSETS) / sizeof(DWORD)
        );

        if (rpmAddr && spdAddr && dmgAddr && fuelAddr) {
            g_gameData.rpm      = g_mem.ReadFloat(rpmAddr);
            g_gameData.speedKmh = g_mem.ReadFloat(spdAddr) * 3.6f;  // m/s → km/h
            g_gameData.damage   = g_mem.ReadFloat(dmgAddr);
            g_gameData.fuel     = g_mem.ReadFloat(fuelAddr);
            g_gameData.valid    = true;

            // Max RPM takibi
            if (g_gameData.rpm > g_maxRpmSeen)
                g_maxRpmSeen = g_gameData.rpm;

            // --- RPM BOOST YAZMA ---
            if (g_rpmBoostActive && rpmAddr)
                g_mem.WriteFloat(rpmAddr, g_rpmTarget);

            // --- NO DAMAGE YAZMA ---
            // Hasar değerini sürekli 0'a kilitle
            if (g_noDamageActive && dmgAddr)
                g_mem.WriteFloat(dmgAddr, 0.0f);
        }
        else {
            g_gameData.valid = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}


// ================================================
//   ImGui MENÜ ÇİZ
//   Her frame çağrılır
// ================================================
void RenderMenu() {

    // --- Pencere stili ---
    ImGui::SetNextWindowSize(ImVec2((float)g_winWidth, (float)g_winHeight), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("Refleks External v3.0 | ETS2 1.58", &g_menuOpen, flags);

    // ================================================
    //   BAĞLANTI DURUMU
    // ================================================
    if (g_mem.IsAttached()) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.4f, 1.0f), "[CONNECTED] eurotrucks2.exe");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[DISCONNECTED] ETS2 bulunamadi");
    }

    ImGui::Separator();

    // ================================================
    //   OYUN VERİLERİ BÖLÜMÜ
    // ================================================
    if (ImGui::CollapsingHeader("Oyun Verileri", ImGuiTreeNodeFlags_DefaultOpen)) {

        if (g_gameData.valid) {

            // Hız göstergesi
            ImGui::Text("Hiz     : %.1f km/h", g_gameData.speedKmh);

            // Güncel RPM
            ImGui::Text("RPM     : %.0f", g_gameData.rpm);

            // Yakıt
            ImGui::Text("Yakit   : %.1f L", g_gameData.fuel);

            // Hasar — renkli göster
            ImVec4 dmgColor = g_gameData.damage < 0.3f
                ? ImVec4(0.0f, 1.0f, 0.4f, 1.0f)   // yeşil
                : g_gameData.damage < 0.7f
                    ? ImVec4(1.0f, 0.8f, 0.0f, 1.0f)  // sarı
                    : ImVec4(1.0f, 0.2f, 0.2f, 1.0f); // kırmızı

            ImGui::TextColored(dmgColor,
                "Hasar   : %.0f%%", g_gameData.damage * 100.0f);

            // Max RPM
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.0f, 1.0f),
                "Max RPM : %.0f", g_maxRpmSeen);

        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                "Veri alinamiyor — Yukleme ekrani mi?");
        }
    }

    ImGui::Separator();

    // ================================================
    //   RPM BOOST BÖLÜMÜ
    //   Slider ile 0 → 10000 RPM arası ayar
    // ================================================
    if (ImGui::CollapsingHeader("RPM Boost", ImGuiTreeNodeFlags_DefaultOpen)) {

        // Checkbox — aktif/pasif
        bool rpmActive = g_rpmBoostActive.load();
        if (ImGui::Checkbox("RPM Boost Aktif", &rpmActive))
            g_rpmBoostActive = rpmActive;

        // RPM Slider — 0'dan 10000'e
        // Sağ tarafta "RPM: XXXX" yazısı
        ImGui::PushItemWidth(280.0f);
        ImGui::SliderFloat("##rpmslider", &g_rpmTarget, 0.0f, 10000.0f, "");
        ImGui::PopItemWidth();

        ImGui::SameLine();

        // Slider'ın sağında büyük RPM yazısı
        ImGui::TextColored(
            g_rpmBoostActive
                ? ImVec4(0.0f, 1.0f, 0.4f, 1.0f)
                : ImVec4(0.8f, 0.8f, 0.8f, 1.0f),
            "RPM: %.0f", g_rpmTarget
        );

        // Renkli progress bar (görsel)
        float rpmPercent = g_rpmTarget / 10000.0f;
        ImVec4 barColor = rpmPercent < 0.5f
            ? ImVec4(0.0f, 0.8f, 0.2f, 1.0f)   // yeşil (düşük rpm)
            : rpmPercent < 0.8f
                ? ImVec4(1.0f, 0.7f, 0.0f, 1.0f)  // sarı (orta rpm)
                : ImVec4(1.0f, 0.2f, 0.1f, 1.0f); // kırmızı (yüksek rpm)

        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
        ImGui::ProgressBar(rpmPercent, ImVec2(-1, 8), "");
        ImGui::PopStyleColor();

        // Hızlı preset butonları
        ImGui::Text("Hizli Sec:");
        ImGui::SameLine();
        if (ImGui::SmallButton("1K"))  g_rpmTarget = 1000.0f;
        ImGui::SameLine();
        if (ImGui::SmallButton("3K"))  g_rpmTarget = 3000.0f;
        ImGui::SameLine();
        if (ImGui::SmallButton("5K"))  g_rpmTarget = 5000.0f;
        ImGui::SameLine();
        if (ImGui::SmallButton("7.5K")) g_rpmTarget = 7500.0f;
        ImGui::SameLine();
        if (ImGui::SmallButton("10K")) g_rpmTarget = 10000.0f;
    }

    ImGui::Separator();

    // ================================================
    //   NO DAMAGE BÖLÜMÜ
    // ================================================
    if (ImGui::CollapsingHeader("No Damage", ImGuiTreeNodeFlags_DefaultOpen)) {

        bool dmgActive = g_noDamageActive.load();
        if (ImGui::Checkbox("No Damage Aktif", &dmgActive))
            g_noDamageActive = dmgActive;

        if (g_noDamageActive) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.4f, 1.0f),
                "Hasar sifirlanıyor — Tır yıkılmaz!");
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "Hasar normal sekilde isliyor.");
        }
    }

    ImGui::Separator();

    // Alt bilgi
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
        "INSERT = Menu ac/kapat | Sadece Singleplayer!");

    ImGui::End();
}


// ================================================
//   DirectX11 KURULUM
// ================================================
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = 0;
    sd.BufferDesc.Height                  = 0;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = hWnd;
    sd.SampleDesc.Count                   = 1;
    sd.SampleDesc.Quality                 = 0;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0
    };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        featureLevelArray, 2, D3D11_SDK_VERSION,
        &sd, &g_pSwapChain,
        &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext
    );

    if (FAILED(hr)) return false;

    // Render target oluştur
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
    return true;
}

void CleanupDeviceD3D() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
    if (g_pSwapChain)           { g_pSwapChain->Release();           g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext)    { g_pd3dDeviceContext->Release();    g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice)           { g_pd3dDevice->Release();           g_pd3dDevice = nullptr; }
}


// ================================================
//   PENCERE PROSEDÜRÜ
// ================================================
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice && wParam != SIZE_MINIMIZED) {
            if (g_mainRenderTargetView) {
                g_mainRenderTargetView->Release();
                g_mainRenderTargetView = nullptr;
            }
            g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam),
                DXGI_FORMAT_UNKNOWN, 0);
            ID3D11Texture2D* pBackBuffer = nullptr;
            g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
            if (pBackBuffer) {
                g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr,
                    &g_mainRenderTargetView);
                pBackBuffer->Release();
            }
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}


// ================================================
//   MAIN
// ================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {

    // --- ETS2'ye bağlan ---
    if (!g_mem.Attach(L"eurotrucks2.exe")) {
        MessageBoxW(nullptr,
            L"ETS2 bulunamadi!\n\nOnce oyunu ac, sonra Refleks External'i calistir.\nYonetici olarak calistirmayi unutma!",
            L"Refleks External", MB_OK | MB_ICONERROR);
        return 1;
    }

    // --- Veri okuma thread'ini başlat ---
    std::thread dataThread(GameDataThread);
    dataThread.detach();

    // --- Pencere sınıfı kaydet ---
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = L"RefleksExternal";
    RegisterClassExW(&wc);

    // --- Pencere oluştur (şeffaf arka plan) ---
    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED,  // Her zaman üstte + şeffaflık
        wc.lpszClassName,
        L"Refleks External v3.0",
        WS_POPUP,                         // Başlık çubuğu yok
        100, 100,
        g_winWidth, g_winHeight,
        nullptr, nullptr, hInstance, nullptr
    );

    // Şeffaf arka plan ayarı
    SetLayeredWindowAttributes(g_hwnd, RGB(0, 0, 0), 230, LWA_ALPHA);

    // DWM blur (opsiyonel — güzel görünüm)
    MARGINS margins{ -1 };
    DwmExtendFrameIntoClientArea(g_hwnd, &margins);

    // --- DirectX11 başlat ---
    if (!CreateDeviceD3D(g_hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, hInstance);
        return 1;
    }

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    // --- ImGui başlat ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Koyu tema
    ImGui::StyleColorsDark();

    // Renk özelleştirme
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 8.0f;
    style.FrameRounding     = 4.0f;
    style.GrabRounding      = 4.0f;
    style.Colors[ImGuiCol_WindowBg]      = ImVec4(0.08f, 0.08f, 0.10f, 0.95f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.55f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab]    = ImVec4(0.20f, 0.60f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_CheckMark]     = ImVec4(0.00f, 1.00f, 0.50f, 1.00f);

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // --- Ana döngü ---
    MSG msg{};
    while (msg.message != WM_QUIT) {

        if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        // INSERT tuşu → menü aç/kapat
        if (GetAsyncKeyState(VK_INSERT) & 1)
            g_menuOpen = !g_menuOpen;

        // F5 → Max RPM sıfırla
        if (GetAsyncKeyState(VK_F5) & 1)
            g_maxRpmSeen = 0.0f;

        // --- ImGui frame başlat ---
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Menü çiz
        if (g_menuOpen)
            RenderMenu();

        // --- Render ---
        ImGui::Render();
        const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);   // VSync
    }

    // --- Temizlik ---
    // Boost aktifken kapatılırsa hasar sıfırla
    g_rpmBoostActive = false;
    g_noDamageActive = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(g_hwnd);
    UnregisterClassW(wc.lpszClassName, hInstance);
    g_mem.Detach();

    return 0;
}
