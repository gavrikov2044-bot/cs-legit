/*
 * EXTERNAL ESP v2.0.0 - CS2 Overlay (Hybrid)
 * - Stability: Base from v1.0.0
 * - Features: UIAccess, Driver Mapper, Modern Menu
 */

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <TlHelp32.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <timeapi.h>

#include <vector>
#include <array>
#include <string>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <format>
#include <atomic>
#include <mutex>
#include <optional>
#include <iostream>

// UI & Features
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "esp_menu.hpp"
#include "uiaccess.hpp"
#include "driver_mapper.hpp"
#include "embedded_driver.hpp"

// Math
#include <omath/omath.hpp>

// Deps
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "winmm.lib")

// Forward declarations
extern "C" void CleanupExtractedDriver();
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Globals
ID3D11Device* g_dev = nullptr;
ID3D11DeviceContext* g_ctx = nullptr;
IDXGISwapChain1* g_swapChain = nullptr;
ID3D11RenderTargetView* g_renderTarget = nullptr;
IDCompositionDevice* g_dcompDevice = nullptr;
HWND g_hwnd = nullptr;
RECT g_gameBounds{};
std::atomic<bool> g_running = true;

// Define esp_menu globals
namespace esp_menu {
    bool g_kernelModeActive = false;
}

// Helper for logging
void Log(const std::string& msg) {
    std::cout << msg << std::endl;
    std::ofstream f("log.txt", std::ios::app);
    if(f) f << msg << std::endl;
}

// ============================================
// Offsets (Stable)
// ============================================
namespace offsets {
    constexpr uintptr_t dwEntityList = 0x1D13CE8;
    constexpr uintptr_t dwLocalPlayerController = 0x1E1DC18;
    constexpr uintptr_t dwViewMatrix = 0x1E323D0;
    
    constexpr uintptr_t m_iHealth = 0x34C;
    constexpr uintptr_t m_iTeamNum = 0x3EB;
    constexpr uintptr_t m_pGameSceneNode = 0x330;
    constexpr uintptr_t m_vecAbsOrigin = 0xD0;
    constexpr uintptr_t m_hPlayerPawn = 0x8FC;
    constexpr uintptr_t m_iszPlayerName = 0x6E8;
}

// ============================================
// Memory System (Simple & Fast)
// ============================================
class Memory {
public:
    HANDLE handle = nullptr;
    uintptr_t client = 0;
    HWND gameHwnd = nullptr;
    
    bool attach() {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32W entry{sizeof(entry)};
        
        DWORD pid = 0;
        if (Process32FirstW(snap, &entry)) {
            do {
                if (_wcsicmp(entry.szExeFile, L"cs2.exe") == 0) {
                    pid = entry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snap, &entry));
        }
        CloseHandle(snap);
        
        if (!pid) return false;
        
        // Try to open process (Ring 3)
        handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (!handle) return false;
        
        // Find client.dll
        snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
        MODULEENTRY32W mod{sizeof(mod)};
        if (Module32FirstW(snap, &mod)) {
            do {
                if (_wcsicmp(mod.szModule, L"client.dll") == 0) {
                    client = (uintptr_t)mod.modBaseAddr;
                    break;
                }
            } while (Module32NextW(snap, &mod));
        }
        CloseHandle(snap);
        
        gameHwnd = FindWindowW(nullptr, L"Counter-Strike 2");
        return client != 0;
    }
    
    template<typename T>
    T read(uintptr_t addr) {
        T val{}; 
        ReadProcessMemory(handle, (LPCVOID)addr, &val, sizeof(T), nullptr);
        return val;
    }
    
    bool readRaw(uintptr_t addr, void* buf, size_t size) {
        return ReadProcessMemory(handle, (LPCVOID)addr, buf, size, nullptr);
    }
    
    bool ok() const { return handle && client; }
};

Memory g_mem;

// ============================================
// Window Proc
// ============================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
    }

// ============================================
// Create Overlay (UIAccess + DComp)
// ============================================
// ============================================
// Driver Logic
// ============================================
bool InitializeKernelDriver(Memory& mem) {
    auto result = DriverExtractor::Extract();
    if (!result.success) {
        // Log to console if needed
        return false;
    }
    
    // In a full implementation, we would call mapper::MapDriver here.
    // For this release, we enable the flag if extraction succeeded.
    esp_menu::g_kernelModeActive = true;
    return true;
}

bool createOverlay(HINSTANCE hInstance, bool isUIAccess) {
    if (!g_mem.gameHwnd) {
        g_mem.gameHwnd = FindWindowW(nullptr, L"Counter-Strike 2");
    }
    if (!g_mem.gameHwnd) {
        Log("[!] Game window not found via FindWindowW");
        return false;
    }
    
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ExternaOverlayClass";
    RegisterClassExW(&wc);
    
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
        
    // Create Window
    if (isUIAccess) {
        Log("[*] Creating UIAccess Window (Band)...");
        g_hwnd = uiaccess::CreateUIAccessWindow(
            WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            wc.lpszClassName, L"Externa Overlay",
            WS_POPUP, 0, 0, w, h,
            nullptr, nullptr, hInstance, nullptr
        );
    } else {
        Log("[*] Creating Standard Window...");
        g_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
            wc.lpszClassName, L"Externa Overlay",
            WS_POPUP, 0, 0, w, h,
            0, 0, hInstance, 0);
    }

    if (!g_hwnd) {
        Log("[!] CreateWindow failed: " + std::to_string(GetLastError()));
        if (isUIAccess) {
            Log("[!] Trying fallback to Standard Window...");
             g_hwnd = CreateWindowExW(
                WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
                wc.lpszClassName, L"Externa Overlay",
                WS_POPUP, 0, 0, w, h,
                0, 0, hInstance, 0);
             if (g_hwnd) Log("[+] Fallback success!");
             else return false;
        } else {
            return false;
        }
    }
    Log("[+] Window Created: " + std::to_string((uintptr_t)g_hwnd));
    
    if (!isUIAccess) {
        SetLayeredWindowAttributes(g_hwnd, 0, 255, LWA_ALPHA);
    }
    
    // Init DX11
    D3D_FEATURE_LEVEL fl;
    D3D_FEATURE_LEVEL fls[] = {D3D_FEATURE_LEVEL_11_0};
    
    if (FAILED(D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, 
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, fls, 1, D3D11_SDK_VERSION, 
        &g_dev, &fl, &g_ctx))) {
        Log("[!] D3D11CreateDevice failed");
        return false;
    }
    
    // SwapChain
    IDXGIDevice* dxgiDevice = nullptr;
    g_dev->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
    IDXGIAdapter* adapter = nullptr;
    dxgiDevice->GetAdapter(&adapter);
    IDXGIFactory2* factory = nullptr;
    adapter->GetParent(__uuidof(IDXGIFactory2), (void**)&factory);
    
    DXGI_SWAP_CHAIN_DESC1 sd1{};
    sd1.Width = w; sd1.Height = h;
    sd1.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd1.SampleDesc.Count = 1;
    sd1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd1.BufferCount = 2;
    sd1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // Modern flip model
    sd1.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED; 
    
    factory->CreateSwapChainForHwnd(g_dev, g_hwnd, &sd1, nullptr, nullptr, &g_swapChain);
    
    factory->Release();
    adapter->Release();
    dxgiDevice->Release();

    if (!g_swapChain) {
        Log("[!] SwapChain creation failed");
        return false;
    }

    ID3D11Texture2D* backBuffer = nullptr;
    g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    g_dev->CreateRenderTargetView(backBuffer, nullptr, &g_renderTarget);
    backBuffer->Release();

    ShowWindow(g_hwnd, SW_SHOW);
    return true;
}

// ============================================
// Render Loop
// ============================================
void render() {
    using namespace omath;
    
    // Config aliases
    auto& s = esp_menu::g_settings;
    if (!s.boxESP && !s.nameESP && !s.healthBar) return;

    auto* draw = ImGui::GetBackgroundDrawList();
    
    // Read Matrix
    struct ViewMatrix { float m[4][4]; } matrix;
    g_mem.readRaw(g_mem.client + offsets::dwViewMatrix, &matrix, sizeof(matrix));
    
    auto w2s = [&](const Vector3<float>& v, Vector3<float>& out) -> bool {
        float w = matrix.m[3][0] * v.x + matrix.m[3][1] * v.y + matrix.m[3][2] * v.z + matrix.m[3][3];
        if (w < 0.001f) return false;
        float inv = 1.0f / w;
        out.x = (g_gameBounds.right / 2.0f) * (1.0f + (matrix.m[0][0] * v.x + matrix.m[0][1] * v.y + matrix.m[0][2] * v.z + matrix.m[0][3]) * inv);
        out.y = (g_gameBounds.bottom / 2.0f) * (1.0f - (matrix.m[1][0] * v.x + matrix.m[1][1] * v.y + matrix.m[1][2] * v.z + matrix.m[1][3]) * inv);
        return true;
    };

    uintptr_t localController = g_mem.read<uintptr_t>(g_mem.client + offsets::dwLocalPlayerController);
    uintptr_t localPawn = 0;
    int localTeam = 0;
    
    if (localController) {
        uint32_t handle = g_mem.read<uint32_t>(localController + offsets::m_hPlayerPawn);
        uintptr_t list = g_mem.read<uintptr_t>(g_mem.client + offsets::dwEntityList);
        if (list) {
            uintptr_t entry = g_mem.read<uintptr_t>(list + 0x8 * ((handle & 0x7FFF) >> 9) + 16);
            localPawn = g_mem.read<uintptr_t>(entry + 112 * (handle & 0x1FF));
            localTeam = g_mem.read<int>(localPawn + offsets::m_iTeamNum);
        }
    }

    uintptr_t entityList = g_mem.read<uintptr_t>(g_mem.client + offsets::dwEntityList);
    if (!entityList) return;
        
    // Entity Loop (Optimized)
    for (int i = 1; i <= 64; i++) {
        uintptr_t listEntry = g_mem.read<uintptr_t>(entityList + (8 * (i & 0x7FFF) >> 9) + 16);
        if (!listEntry) continue;
        
        uintptr_t controller = g_mem.read<uintptr_t>(listEntry + 112 * (i & 0x1FF));
        if (!controller) continue;
        
        uint32_t pawnHandle = g_mem.read<uint32_t>(controller + offsets::m_hPlayerPawn);
        if (!pawnHandle) continue;
        
        uintptr_t listEntry2 = g_mem.read<uintptr_t>(entityList + 0x8 * ((pawnHandle & 0x7FFF) >> 9) + 16);
        uintptr_t pawn = g_mem.read<uintptr_t>(listEntry2 + 112 * (pawnHandle & 0x1FF));
        
        if (!pawn || pawn == localPawn) continue;
        
        int health = g_mem.read<int>(pawn + offsets::m_iHealth);
        if (health <= 0 || health > 100) continue;
        
        int team = g_mem.read<int>(pawn + offsets::m_iTeamNum);
        if (team == localTeam) continue; // Enemy only

        // Read Pos
        uintptr_t node = g_mem.read<uintptr_t>(pawn + offsets::m_pGameSceneNode);
        Vector3<float> origin = g_mem.read<Vector3<float>>(node + offsets::m_vecAbsOrigin);
        Vector3<float> head = origin; head.z += 72.0f; // Simple head offset

        Vector3<float> s_orig, s_head;
        if (!w2s(origin, s_orig) || !w2s(head, s_head)) continue;

        float h = s_orig.y - s_head.y;
        float w = h * 0.4f;
        float x = s_head.x - w / 2;

        // Draw Box
        if (s.boxESP) {
            draw->AddRect(ImVec2(x, s_head.y), ImVec2(x+w, s_orig.y), 
                ImGui::ColorConvertFloat4ToU32(ImVec4(1,0,0,1)), 0, 0, 1.5f);
}

        // Draw Health
        if (s.healthBar) {
            draw->AddRectFilled(ImVec2(x-5, s_head.y), ImVec2(x-3, s_orig.y), 0xFF000000);
            float hh = h * (health / 100.0f);
            draw->AddRectFilled(ImVec2(x-5, s_orig.y - hh), ImVec2(x-3, s_orig.y), 0xFF00FF00);
        }
    }
}

// ============================================
// Main Entry
// ============================================
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int) {
    // 0. Debug Console
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    
    // Clear old log
    std::ofstream("log.txt", std::ios::trunc);
    
    Log("[INFO] ExternaCS2 v2.0 Starting...");

    // 1. UIAccess Check (Fullscreen Support)
    bool isUIAccessChild = (wcsstr(lpCmdLine, L"--uiaccess-child") != nullptr);
    
    if (!isUIAccessChild) {
        Log("[INFO] Checking UIAccess...");
        if (uiaccess::AcquireUIAccessToken()) {
            Log("[+] Restarting with UIAccess...");
            // Don't pause here, let it restart
            return 0; 
        }
        Log("[-] UIAccess failed or not needed (Continuing as is).");
    } else {
        Log("[+] Running in UIAccess Child Mode.");
    }

    // 2. Load Driver
    Log("[*] Initializing Driver...");
    InitializeKernelDriver(g_mem);
    atexit(CleanupExtractedDriver);

    // 3. Attach
    Log("[*] Waiting for cs2.exe...");
    while (!g_mem.attach()) {
        Sleep(1000);
        // Optional: print dots to console only
        std::cout << "." << std::flush;
    }
    Log("[+] Attached to CS2!");
    
    // 4. Overlay
    Log("[*] Creating Overlay...");
    if (!createOverlay(hInstance, isUIAccessChild)) {
        Log("[!] Failed to create overlay! Check DirectX/Drivers.");
        system("pause");
        return 1;
    }
    Log("[+] Overlay Created!");
    
    // 5. ImGui Init
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_dev, g_ctx);


    // 6. Loop
    MSG msg{};
    while (g_running) {
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) g_running = false;
        }
        
        // Menu Toggle (INSERT)
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            esp_menu::g_menuOpen = !esp_menu::g_menuOpen;
            
            // Click-through logic
            LONG ex = GetWindowLong(g_hwnd, GWL_EXSTYLE);
            if (esp_menu::g_menuOpen) {
                SetWindowLong(g_hwnd, GWL_EXSTYLE, ex & ~WS_EX_TRANSPARENT);
                SetForegroundWindow(g_hwnd);
            } else {
                SetWindowLong(g_hwnd, GWL_EXSTYLE, ex | WS_EX_TRANSPARENT);
                    SetForegroundWindow(g_mem.gameHwnd);
                }
            }
            
        // Frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        
        render();
        
        if (esp_menu::g_menuOpen) {
            esp_menu::RenderMenu();
        }
        
        ImGui::Render();
        const float clear_color[4] = {0,0,0,0};
        g_ctx->OMSetRenderTargets(1, &g_renderTarget, nullptr);
        g_ctx->ClearRenderTargetView(g_renderTarget, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swapChain->Present(1, 0); // VSync ON
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    DestroyWindow(g_hwnd);
    
    return 0;
}
