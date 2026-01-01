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
#include <map>
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
    constexpr uintptr_t m_ArmorValue = 0x1518; // Standard Armor Offset
}

// ============================================
// Direct Syscalls (Ring 3 Stealth)
// ============================================
extern "C" NTSTATUS DirectSyscall(
    DWORD syscallNumber,
    HANDLE ProcessHandle,
    PVOID BaseAddress,
    PVOID Buffer,
    SIZE_T Size,
    PSIZE_T BytesRead
);

DWORD GetNtReadVirtualMemorySyscall() {
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) return 0; 
    
    FARPROC func = GetProcAddress(hNtdll, "NtReadVirtualMemory");
    if (!func) return 0;
    
    BYTE* p = (BYTE*)func;
    
    // Professional SSN Extraction
    // Expected:
    // 4C 8B D1          mov r10, rcx
    // B8 xx xx xx xx    mov eax, imm32
    // 0F 05             syscall
    
    if (p[0] == 0x4C && p[1] == 0x8B && p[2] == 0xD1 && p[3] == 0xB8) {
         return *(DWORD*)(p + 4);
    }
    
    // Check for JMP hook (E9) - Anti-Cheat might be present
    if (p[0] == 0xE9) {
        // We could follow the jump, but for safety/stealth, 
        // falling back to WinAPI is often smarter than analyzing the hook.
        // However, we can check neighbor functions or use Halos Gate here.
        // For this version: Fallback.
        return 0;
    }
    
    return 0; 
}

DWORD g_syscallSSN = 0;

// ============================================
// Memory System (Simple & Fast)
// ============================================
class Memory {
public:
    HANDLE handle = nullptr;
    uintptr_t client = 0;
    HWND gameHwnd = nullptr;
    
    bool attach() {
        // Init SSN
        // Re-enabled with robust scanner
        g_syscallSSN = GetNtReadVirtualMemorySyscall();
        
        if (g_syscallSSN != 0) {
            Log("[+] Stealth Mode: Dynamic SSN Found (" + std::to_string(g_syscallSSN) + ")");
        } else {
            Log("[!] Stealth Mode: SSN Lookup Failed. Using WinAPI.");
        }
        
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
        readRaw(addr, &val, sizeof(T));
        return val;
    }
    
    bool readRaw(uintptr_t addr, void* buf, size_t size) {
        // Priority 1: Kernel Driver (Ring 0) - TODO: Implement mapping check
        if (esp_menu::g_kernelModeActive) {
            // In a real driver setup, we would use IOCTL here.
            // Since this version maps the driver but the communication is not fully set up in this snippet,
            // we fall back to Ring 3 methods if driver is not explicitly handling it.
            // For now, let's assume if g_kernelModeActive is true, we want to use Syscall or Driver.
            // We'll stick to Syscall for this "Stealth Mode" if Driver IOCTL isn't ready.
        }

        // Priority 2: Direct Syscall (Ring 3 Stealth)
        if (g_syscallSSN > 0) {
            SIZE_T bytesRead = 0;
            NTSTATUS status = DirectSyscall(g_syscallSSN, handle, (PVOID)addr, buf, size, &bytesRead);
            if (status != 0) {
                // Log once and DISABLE syscalls to fallback immediately
                static bool logged = false;
                if (!logged) {
                    Log("[!] Syscall Read Failed: Status " + std::to_string(status) + ". Reverting to WinAPI.");
                    logged = true;
                }
                g_syscallSSN = 0; // Force fallback for next calls
                // Try WinAPI immediately this time
                return ReadProcessMemory(handle, (LPCVOID)addr, buf, size, nullptr);
            }
            return true; 
        }
        
        // Priority 3: WinAPI (Standard)
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
    case WM_NCHITTEST:
        // CRITICAL FIX: Allow clicks to pass through when menu is closed
        // This works even without WS_EX_LAYERED
        if (!esp_menu::g_menuOpen) {
            return HTTRANSPARENT;
        }
        break;
        
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
    Log("[*] Screen Size: " + std::to_string(w) + "x" + std::to_string(h));
        
    // Create Window
    if (isUIAccess) {
        Log("[*] Creating UIAccess Window (Band)...");
        // WS_EX_NOACTIVATE prevents focus stealing
        g_hwnd = uiaccess::CreateUIAccessWindow(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            wc.lpszClassName, L"Externa Overlay",
            WS_POPUP, 0, 0, w, h,
            nullptr, nullptr, hInstance, nullptr
        );
    } else {
        Log("[*] Creating Standard Window...");
        g_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 
            wc.lpszClassName, L"Externa Overlay",
            WS_POPUP, 0, 0, w, h,
            0, 0, hInstance, 0);
    }

    if (!g_hwnd) {
        Log("[!] CreateWindow failed: " + std::to_string(GetLastError()));
         return false;
    }
    Log("[+] Window Created: " + std::to_string((uintptr_t)g_hwnd));
    
    // TRANSPARENCY FIX:
    // 1. Remove SetLayeredWindowAttributes (It causes black screen with DWM)
    // 2. Extend DWM Frame into client area (This enables per-pixel alpha)
    MARGINS margins = {-1};
    DwmExtendFrameIntoClientArea(g_hwnd, &margins);
    
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
    sd1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd1.SampleDesc.Count = 1;
    sd1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd1.BufferCount = 1;
    sd1.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; // Safest for overlay
    sd1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED; // DWM handles alpha
    sd1.Scaling = DXGI_SCALING_STRETCH;
    
    HRESULT hr = factory->CreateSwapChainForHwnd(g_dev, g_hwnd, &sd1, nullptr, nullptr, &g_swapChain);
    
    if (FAILED(hr)) {
         Log("[!] SwapChain creation failed: " + std::to_string(hr));
         return false;
    }
    
    factory->Release();
    adapter->Release();
    dxgiDevice->Release();

    if (FAILED(hr) || !g_swapChain) {
        Log("[!] SwapChain creation failed: " + std::to_string(hr));
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
// Optimization & Smoothness (Multi-threaded)
// ============================================
struct SmoothEntity {
    omath::Vector3<float> currentPos;
    omath::Vector3<float> targetPos; // Not used in simple lerp, but good for future
    omath::Vector3<float> origin;    // Raw read
    int health;
    int armor;
    bool valid;
};
// Thread-safe data
std::array<SmoothEntity, 65> g_entityCache;
std::mutex g_cacheMutex;
struct ViewMatrix { float m[4][4]; } g_viewMatrix;

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// ============================================
// Memory Thread (Background Reader)
// ============================================
void MemoryThread() {
    using namespace omath;
    Log("[*] Memory Thread Started");
    
    while (g_running) {
        if (!g_mem.ok()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // 1. Read Globals
        ViewMatrix vm;
        g_mem.readRaw(g_mem.client + offsets::dwViewMatrix, &vm, sizeof(vm));
        
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
        
        // 2. Read Entities
        {
            std::lock_guard<std::mutex> lock(g_cacheMutex);
            g_viewMatrix = vm; // Update matrix
            
            if (entityList) {
                for (int i = 1; i <= 64; i++) {
                    auto& ent = g_entityCache[i];
                    ent.valid = false;
                    
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
                    if (team == localTeam) continue;

                    uintptr_t node = g_mem.read<uintptr_t>(pawn + offsets::m_pGameSceneNode);
                    Vector3<float> origin = g_mem.read<Vector3<float>>(node + offsets::m_vecAbsOrigin);
                    
                    int armor = g_mem.read<int>(pawn + offsets::m_ArmorValue);
                    
                    // Fill Cache
                    ent.health = health;
                    ent.armor = armor;
                    ent.origin = origin;
                    ent.valid = true;
                    
                    // Init smooth pos if first time
                    if (ent.currentPos.x == 0 && ent.currentPos.y == 0) {
                         ent.currentPos = origin;
                    }
                }
            }
        }
        
        // Sleep to save CPU (1ms is enough for >500hz reading)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// ============================================
// Render Loop
// ============================================
void render() {
    using namespace omath;
    
    // Config aliases
    auto& s = esp_menu::g_settings;
    if (!s.boxESP && !s.healthBar && !s.armorBar) return;

    auto* draw = ImGui::GetBackgroundDrawList();
    float dt = ImGui::GetIO().DeltaTime;
    
    // Lock and Copy Data
    // We copy to stack to release mutex ASAP, avoiding render lag
    ViewMatrix matrix;
    std::array<SmoothEntity, 65> entities;
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        matrix = g_viewMatrix;
        entities = g_entityCache;
    }
    
    auto w2s = [&](const Vector3<float>& v, Vector3<float>& out) -> bool {
        float w = matrix.m[3][0] * v.x + matrix.m[3][1] * v.y + matrix.m[3][2] * v.z + matrix.m[3][3];
        if (w < 0.001f) return false;
        float inv = 1.0f / w;
        out.x = (g_gameBounds.right / 2.0f) * (1.0f + (matrix.m[0][0] * v.x + matrix.m[0][1] * v.y + matrix.m[0][2] * v.z + matrix.m[0][3]) * inv);
        out.y = (g_gameBounds.bottom / 2.0f) * (1.0f - (matrix.m[1][0] * v.x + matrix.m[1][1] * v.y + matrix.m[1][2] * v.z + matrix.m[1][3]) * inv);
        return true;
    };

    // Entity Loop (Draw Only)
    for (int i = 1; i <= 64; i++) {
        auto& ent = entities[i];
        if (!ent.valid) continue;
        
        // --- SMOOTHING LOGIC ---
        // Interpolate position to remove jitter
        // Since we copied the cache, we need to maintain state locally for smoothing
        static std::map<int, Vector3<float>> s_smoothPos;
        Vector3<float>& smoothPos = s_smoothPos[i];
        
        if (smoothPos.x == 0 && smoothPos.y == 0) smoothPos = ent.origin;
        
        // Lerp factor
        float lerpSpeed = 25.0f * dt;
        
        // TELEPORT CHECK: If distance is too big (> 2.0m), snap immediately (Respawn/Teleport)
        float distSq = (smoothPos.x - ent.origin.x)*(smoothPos.x - ent.origin.x) + 
                       (smoothPos.y - ent.origin.y)*(smoothPos.y - ent.origin.y) + 
                       (smoothPos.z - ent.origin.z)*(smoothPos.z - ent.origin.z);
                       
        if (distSq > 400.0f) { // 20m^2
            smoothPos = ent.origin;
        } else {
            smoothPos.x = Lerp(smoothPos.x, ent.origin.x, lerpSpeed);
            smoothPos.y = Lerp(smoothPos.y, ent.origin.y, lerpSpeed);
            smoothPos.z = Lerp(smoothPos.z, ent.origin.z, lerpSpeed);
        }
        
        Vector3<float> smoothHead = smoothPos;
        smoothHead.z += 72.0f;
        // -----------------------

        Vector3<float> s_orig, s_head;
        if (!w2s(smoothPos, s_orig) || !w2s(smoothHead, s_head)) continue;

        float h = s_orig.y - s_head.y;
        float w = h * 0.4f;
        float x = s_head.x - w / 2;

        // Draw Box
        if (s.boxESP) {
             draw->AddRect(ImVec2(x-1, s_head.y-1), ImVec2(x+w+1, s_orig.y+1), 0xFF000000, 0, 0, 3.0f);
             draw->AddRect(ImVec2(x, s_head.y), ImVec2(x+w, s_orig.y), 
                ImGui::ColorConvertFloat4ToU32(ImVec4(1,0,0,1)), 0, 0, 1.5f);
        }

        // Draw Health
        if (s.healthBar) {
            float healthH = h * (ent.health / 100.0f);
            draw->AddRectFilled(ImVec2(x-6, s_head.y), ImVec2(x-2, s_orig.y), 0x80000000); 
            draw->AddRectFilled(ImVec2(x-5, s_orig.y - healthH), ImVec2(x-3, s_orig.y), 0xFF00FF00); 
            draw->AddRect(ImVec2(x-6, s_head.y), ImVec2(x-2, s_orig.y), 0xFF000000, 0, 0, 1.0f); 
        }

        // Draw Armor
        if (s.armorBar && ent.armor > 0) {
            float armorH = h * (ent.armor / 100.0f);
            draw->AddRectFilled(ImVec2(x+w+2, s_head.y), ImVec2(x+w+6, s_orig.y), 0x80000000); 
            draw->AddRectFilled(ImVec2(x+w+3, s_orig.y - armorH), ImVec2(x+w+5, s_orig.y), 0xFFEDA100); 
            draw->AddRect(ImVec2(x+w+2, s_head.y), ImVec2(x+w+6, s_orig.y), 0xFF000000, 0, 0, 1.0f); 
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
    
    // Start Memory Thread
    std::thread memoryThread(MemoryThread);
    memoryThread.detach();

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
        }

        // CLICK-THROUGH LOGIC (ROBUST)
        static bool lastMenuState = !esp_menu::g_menuOpen; // Force update on start
        if (esp_menu::g_menuOpen != lastMenuState) {
            lastMenuState = esp_menu::g_menuOpen;
            
            LONG_PTR exStyle = GetWindowLongPtr(g_hwnd, GWL_EXSTYLE);
            
            if (esp_menu::g_menuOpen) {
                // OPEN: Interactive
                exStyle &= ~WS_EX_TRANSPARENT;
                exStyle &= ~WS_EX_LAYERED; // Remove Layered to use DWM fully
                SetWindowLongPtr(g_hwnd, GWL_EXSTYLE, exStyle);
                SetForegroundWindow(g_hwnd);
            } else {
                // CLOSED: Click-through
                exStyle |= WS_EX_TRANSPARENT;
                exStyle |= WS_EX_LAYERED; // Layered needed for input transparency
                SetWindowLongPtr(g_hwnd, GWL_EXSTYLE, exStyle);
                // Fix black screen potential by setting full opacity
                SetLayeredWindowAttributes(g_hwnd, 0, 255, LWA_ALPHA);
            }
        }
            
        // Frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        
        // AUTO-EXIT CHECK
        if (g_mem.handle) {
            DWORD exitCode = 0;
            if (GetExitCodeProcess(g_mem.handle, &exitCode)) {
                if (exitCode != STILL_ACTIVE) {
                    Log("[!] Game closed. Exiting...");
                    g_running = false;
                }
            }
        }
        
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
