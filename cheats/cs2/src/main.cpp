/*
 * EXTERNAL ESP v5.0 (STABLE)
 * - Architecture: Multi-threaded (Render/Memory Split)
 * - Memory: WinAPI ReadProcessMemory (Maximum Compatibility)
 * - Overlay: UIAccess + Simple Toggle (No Hooks, No Hijacking)
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

#include <array>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <iostream>
#include <fstream>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "esp_menu.hpp"
#include "uiaccess.hpp"

#include <omath/omath.hpp>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

std::atomic<bool> g_running = true;

namespace esp_menu {
    bool g_kernelModeActive = false;
}

void Log(const std::string& msg) {
    std::cout << msg << std::endl;
    std::ofstream f("log.txt", std::ios::app);
    if(f) f << msg << std::endl;
}

// ============================================
// Offsets (CS2 - Update if needed)
// ============================================
namespace offsets {
    constexpr uintptr_t dwEntityList = 0x1A6A8D0;
    constexpr uintptr_t dwLocalPlayerController = 0x1A4F178;
    constexpr uintptr_t dwViewMatrix = 0x1A8F690;
    
    constexpr uintptr_t m_iHealth = 0x344;
    constexpr uintptr_t m_iTeamNum = 0x3E3;
    constexpr uintptr_t m_pGameSceneNode = 0x328;
    constexpr uintptr_t m_vecAbsOrigin = 0xD0;
    constexpr uintptr_t m_hPlayerPawn = 0x80C;
    constexpr uintptr_t m_ArmorValue = 0x354;
}

// ============================================
// Cheat Context (Simple & Clean)
// ============================================
struct CheatContext {
    // Overlay
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    IDXGISwapChain1* swapChain = nullptr;
    ID3D11RenderTargetView* renderTarget = nullptr;
    HWND overlayHwnd = nullptr;
    RECT gameBounds{};
    
    // Game Memory
    HANDLE processHandle = nullptr;
    uintptr_t clientBase = 0;
    HWND gameHwnd = nullptr;
    
    // Entity Cache
    struct EntityData {
        omath::Vector3<float> origin;
        int health = 0;
        int armor = 0;
        bool valid = false;
    };
    
    std::mutex cacheMutex;
    std::array<EntityData, 65> entityCache{};
    struct { float m[4][4]; } viewMatrix{};
    
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
        
        processHandle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (!processHandle) return false;
        
        snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
        MODULEENTRY32W mod{sizeof(mod)};
        if (Module32FirstW(snap, &mod)) {
            do {
                if (_wcsicmp(mod.szModule, L"client.dll") == 0) {
                    clientBase = (uintptr_t)mod.modBaseAddr;
                    break;
                }
            } while (Module32NextW(snap, &mod));
        }
        CloseHandle(snap);
        
        gameHwnd = FindWindowW(nullptr, L"Counter-Strike 2");
        
        return clientBase != 0;
    }
    
    template<typename T>
    T read(uintptr_t addr) {
        T val{};
        ReadProcessMemory(processHandle, (LPCVOID)addr, &val, sizeof(T), nullptr);
        return val;
    }
    
    bool ok() const { return processHandle && clientBase; }
};

CheatContext g_ctx;

// ============================================
// Window Proc (Simple)
// ============================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_NCHITTEST:
        // Click-through when menu is closed
        if (!esp_menu::g_menuOpen) return HTTRANSPARENT;
        break;
        
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ============================================
// Memory Thread (2ms tick)
// ============================================
void MemoryThreadLogic() {
    using namespace omath;
    Log("[*] Memory Thread Started");
    
    while (g_running) {
        if (!g_ctx.ok()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        auto vm = g_ctx.read<decltype(g_ctx.viewMatrix)>(g_ctx.clientBase + offsets::dwViewMatrix);
        
        uintptr_t localController = g_ctx.read<uintptr_t>(g_ctx.clientBase + offsets::dwLocalPlayerController);
        uintptr_t localPawn = 0;
        int localTeam = 0;
        
        if (localController) {
            uint32_t handle = g_ctx.read<uint32_t>(localController + offsets::m_hPlayerPawn);
            uintptr_t list = g_ctx.read<uintptr_t>(g_ctx.clientBase + offsets::dwEntityList);
            if (list) {
                uintptr_t entry = g_ctx.read<uintptr_t>(list + 0x8 * ((handle & 0x7FFF) >> 9) + 16);
                localPawn = g_ctx.read<uintptr_t>(entry + 120 * (handle & 0x1FF));
                localTeam = g_ctx.read<int>(localPawn + offsets::m_iTeamNum);
            }
        }

        uintptr_t entityList = g_ctx.read<uintptr_t>(g_ctx.clientBase + offsets::dwEntityList);
        
        {
            std::lock_guard<std::mutex> lock(g_ctx.cacheMutex);
            g_ctx.viewMatrix = vm;
            
            if (entityList) {
                for (int i = 1; i <= 64; i++) {
                    auto& ent = g_ctx.entityCache[i];
                    ent.valid = false;
                    
                    uintptr_t listEntry = g_ctx.read<uintptr_t>(entityList + (8 * (i & 0x7FFF) >> 9) + 16);
                    if (!listEntry) continue;
                    
                    uintptr_t controller = g_ctx.read<uintptr_t>(listEntry + 120 * (i & 0x1FF));
                    if (!controller) continue;
                    
                    uint32_t pawnHandle = g_ctx.read<uint32_t>(controller + offsets::m_hPlayerPawn);
                    if (!pawnHandle) continue;
                    
                    uintptr_t listEntry2 = g_ctx.read<uintptr_t>(entityList + 0x8 * ((pawnHandle & 0x7FFF) >> 9) + 16);
                    uintptr_t pawn = g_ctx.read<uintptr_t>(listEntry2 + 120 * (pawnHandle & 0x1FF));
                    
                    if (!pawn || pawn == localPawn) continue;
                    
                    int health = g_ctx.read<int>(pawn + offsets::m_iHealth);
                    if (health <= 0 || health > 100) continue;

                    int team = g_ctx.read<int>(pawn + offsets::m_iTeamNum);
                    if (team == localTeam) continue;

                    uintptr_t node = g_ctx.read<uintptr_t>(pawn + offsets::m_pGameSceneNode);
                    
                    ent.origin = g_ctx.read<Vector3<float>>(node + offsets::m_vecAbsOrigin);
                    ent.health = health;
                    ent.armor = g_ctx.read<int>(pawn + offsets::m_ArmorValue);
                    ent.valid = true;
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

// ============================================
// Render (Interpolated ESP)
// ============================================
float Lerp(float a, float b, float t) { return a + (b - a) * (t > 1.0f ? 1.0f : t); }

void render() {
    using namespace omath;
    auto& s = esp_menu::g_settings;
    if (!s.boxESP && !s.healthBar && !s.armorBar) return;

    auto* draw = ImGui::GetBackgroundDrawList();
    float dt = ImGui::GetIO().DeltaTime;
    
    decltype(g_ctx.viewMatrix) matrix;
    std::array<CheatContext::EntityData, 65> entities;
    {
        std::lock_guard<std::mutex> lock(g_ctx.cacheMutex);
        matrix = g_ctx.viewMatrix;
        entities = g_ctx.entityCache;
    }
    
    auto w2s = [&](const Vector3<float>& v, Vector3<float>& out) -> bool {
        float w = matrix.m[3][0] * v.x + matrix.m[3][1] * v.y + matrix.m[3][2] * v.z + matrix.m[3][3];
        if (w < 0.001f) return false;
        float inv = 1.0f / w;
        out.x = (g_ctx.gameBounds.right / 2.0f) * (1.0f + (matrix.m[0][0] * v.x + matrix.m[0][1] * v.y + matrix.m[0][2] * v.z + matrix.m[0][3]) * inv);
        out.y = (g_ctx.gameBounds.bottom / 2.0f) * (1.0f - (matrix.m[1][0] * v.x + matrix.m[1][1] * v.y + matrix.m[1][2] * v.z + matrix.m[1][3]) * inv);
        return true;
    };

    static std::array<Vector3<float>, 65> s_smoothPos{};

    for (int i = 1; i <= 64; i++) {
        auto& ent = entities[i];
        if (!ent.valid) continue;
        
        Vector3<float>& smoothPos = s_smoothPos[i];
        
        if (smoothPos.x == 0 && smoothPos.y == 0) smoothPos = ent.origin;
        
        float dx = ent.origin.x - smoothPos.x;
        float dy = ent.origin.y - smoothPos.y;
        float dz = ent.origin.z - smoothPos.z;
        float distSq = dx*dx + dy*dy + dz*dz;
        
        if (distSq > 1000.0f) {
            smoothPos = ent.origin;
        } else {
            float speed = 50.0f * dt;
            smoothPos.x = Lerp(smoothPos.x, ent.origin.x, speed);
            smoothPos.y = Lerp(smoothPos.y, ent.origin.y, speed);
            smoothPos.z = Lerp(smoothPos.z, ent.origin.z, speed);
        }
        
        Vector3<float> head = smoothPos; head.z += 72.0f;
        Vector3<float> s_orig, s_head;
        
        if (!w2s(smoothPos, s_orig) || !w2s(head, s_head)) continue;

        float h = s_orig.y - s_head.y;
        float boxW = h * 0.4f;
        float x = s_head.x - boxW / 2;

        if (s.boxESP) {
             draw->AddRect(ImVec2(x-1, s_head.y-1), ImVec2(x+boxW+1, s_orig.y+1), 0xFF000000, 0, 0, 3.0f);
             draw->AddRect(ImVec2(x, s_head.y), ImVec2(x+boxW, s_orig.y), 0xFF0000FF, 0, 0, 1.5f);
        }

        if (s.healthBar) {
            float healthH = h * (ent.health / 100.0f);
            draw->AddRectFilled(ImVec2(x-6, s_head.y), ImVec2(x-2, s_orig.y), 0x80000000);
            draw->AddRectFilled(ImVec2(x-5, s_orig.y - healthH), ImVec2(x-3, s_orig.y), 0xFF00FF00);
            draw->AddRect(ImVec2(x-6, s_head.y), ImVec2(x-2, s_orig.y), 0xFF000000, 0, 0, 1.0f);
        }

        if (s.armorBar && ent.armor > 0) {
            float armorH = h * (ent.armor / 100.0f);
            draw->AddRectFilled(ImVec2(x+boxW+2, s_head.y), ImVec2(x+boxW+6, s_orig.y), 0x80000000);
            draw->AddRectFilled(ImVec2(x+boxW+3, s_orig.y - armorH), ImVec2(x+boxW+5, s_orig.y), 0xFFEDA100);
            draw->AddRect(ImVec2(x+boxW+2, s_head.y), ImVec2(x+boxW+6, s_orig.y), 0xFF000000, 0, 0, 1.0f);
        }
    }
}

// ============================================
// Main Entry
// ============================================
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int) {
    AllocConsole();
    FILE* f; freopen_s(&f, "CONOUT$", "w", stdout); freopen_s(&f, "CONOUT$", "w", stderr);
    std::ofstream("log.txt", std::ios::trunc);
    
    Log("[INFO] ExternaCS2 v5.0 STABLE Starting...");

    // UIAccess (for Borderless Fullscreen)
    bool isUIAccessChild = (wcsstr(lpCmdLine, L"--uiaccess-child") != nullptr);
    if (!isUIAccessChild) {
        if (uiaccess::AcquireUIAccessToken()) return 0;
    }

    // Attach
    Log("[*] Waiting for cs2.exe...");
    while (!g_ctx.attach()) {
        Sleep(1000);
        std::cout << "." << std::flush;
    }
    Log("[+] Attached to cs2.exe!");
    
    // Memory Thread
    std::jthread memoryThread(MemoryThreadLogic);

    // Overlay Window
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ExternaOverlayClass";
    RegisterClassExW(&wc);
    
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    g_ctx.gameBounds = {0, 0, w, h};

    if (isUIAccessChild) {
        Log("[*] Creating UIAccess Overlay...");
        g_ctx.overlayHwnd = uiaccess::CreateUIAccessWindow(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            wc.lpszClassName, L"", WS_POPUP, 0, 0, w, h, nullptr, nullptr, hInstance, nullptr
        );
    } else {
        Log("[*] Creating Standard Overlay...");
        g_ctx.overlayHwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            wc.lpszClassName, L"", WS_POPUP, 0, 0, w, h, 0, 0, hInstance, 0
        );
    }

    if (!g_ctx.overlayHwnd) { Log("[!] Failed to create overlay!"); return 1; }
    
    // Transparency
    MARGINS margins = {-1};
    DwmExtendFrameIntoClientArea(g_ctx.overlayHwnd, &margins);
    
    // DX11
    D3D_FEATURE_LEVEL fls[] = {D3D_FEATURE_LEVEL_11_0};
    D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, D3D11_CREATE_DEVICE_BGRA_SUPPORT, fls, 1, D3D11_SDK_VERSION, &g_ctx.dev, nullptr, &g_ctx.ctx);
    
    IDXGIDevice* dxgiDevice = nullptr; g_ctx.dev->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
    IDXGIAdapter* adapter = nullptr; dxgiDevice->GetAdapter(&adapter);
    IDXGIFactory2* factory = nullptr; adapter->GetParent(__uuidof(IDXGIFactory2), (void**)&factory);
    
    DXGI_SWAP_CHAIN_DESC1 sd1{};
    sd1.Width = w; sd1.Height = h;
    sd1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd1.SampleDesc.Count = 1;
    sd1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd1.BufferCount = 2;
    sd1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd1.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    sd1.Scaling = DXGI_SCALING_STRETCH;
    
    HRESULT hr = factory->CreateSwapChainForComposition(g_ctx.dev, &sd1, nullptr, &g_ctx.swapChain);
    if (FAILED(hr)) {
        // Fallback for older systems
        sd1.BufferCount = 1;
        sd1.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        sd1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        factory->CreateSwapChainForHwnd(g_ctx.dev, g_ctx.overlayHwnd, &sd1, nullptr, nullptr, &g_ctx.swapChain);
    } else {
        // Composition SwapChain - need DComp
        IDCompositionDevice* dcomp = nullptr;
        DCompositionCreateDevice(dxgiDevice, __uuidof(IDCompositionDevice), (void**)&dcomp);
        IDCompositionTarget* target = nullptr;
        dcomp->CreateTargetForHwnd(g_ctx.overlayHwnd, TRUE, &target);
        IDCompositionVisual* visual = nullptr;
        dcomp->CreateVisual(&visual);
        visual->SetContent(g_ctx.swapChain);
        target->SetRoot(visual);
        dcomp->Commit();
    }
    
    factory->Release(); adapter->Release(); dxgiDevice->Release();

    ID3D11Texture2D* backBuffer = nullptr;
    g_ctx.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    g_ctx.dev->CreateRenderTargetView(backBuffer, nullptr, &g_ctx.renderTarget);
    backBuffer->Release();

    ShowWindow(g_ctx.overlayHwnd, SW_SHOW);
    
    // ImGui
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(g_ctx.overlayHwnd);
    ImGui_ImplDX11_Init(g_ctx.dev, g_ctx.ctx);
    
    Log("[+] Overlay Ready! Press INSERT to open menu.");

    // Main Loop
    MSG msg{};
    while (g_running) {
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessage(&msg);
            if (msg.message == WM_QUIT) g_running = false;
        }
        
        // Menu Toggle
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            esp_menu::g_menuOpen = !esp_menu::g_menuOpen;
            
            LONG_PTR exStyle = GetWindowLongPtr(g_ctx.overlayHwnd, GWL_EXSTYLE);
            if (esp_menu::g_menuOpen) {
                exStyle &= ~WS_EX_TRANSPARENT;
                SetWindowLongPtr(g_ctx.overlayHwnd, GWL_EXSTYLE, exStyle);
                SetForegroundWindow(g_ctx.overlayHwnd);
            } else {
                exStyle |= WS_EX_TRANSPARENT;
                SetWindowLongPtr(g_ctx.overlayHwnd, GWL_EXSTYLE, exStyle);
            }
        }
        
        // Check if game closed
        if (g_ctx.processHandle) {
            DWORD exitCode = 0;
            if (GetExitCodeProcess(g_ctx.processHandle, &exitCode) && exitCode != STILL_ACTIVE) {
                Log("[!] Game closed. Exiting...");
                g_running = false;
            }
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        
        render();
        if (esp_menu::g_menuOpen) esp_menu::RenderMenu();
        
        ImGui::Render();
        float clear[4] = {0,0,0,0};
        g_ctx.ctx->OMSetRenderTargets(1, &g_ctx.renderTarget, nullptr);
        g_ctx.ctx->ClearRenderTargetView(g_ctx.renderTarget, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_ctx.swapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    if (g_ctx.renderTarget) g_ctx.renderTarget->Release();
    if (g_ctx.swapChain) g_ctx.swapChain->Release();
    if (g_ctx.ctx) g_ctx.ctx->Release();
    if (g_ctx.dev) g_ctx.dev->Release();
    DestroyWindow(g_ctx.overlayHwnd);
    Log("[*] Cleanup complete. Goodbye!");
    return 0;
}
