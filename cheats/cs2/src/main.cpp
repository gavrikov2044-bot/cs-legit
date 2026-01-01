/*
 * EXTERNAL ESP v3.0 (Final Release)
 * - Architecture: Multi-threaded (Render/Memory Split)
 * - Security: Direct Syscalls via KnownDlls (Ring 3 Stealth)
 * - Features: UIAccess Overlay, Interpolation, Click-Through
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
#include <winternl.h>
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

// Global Run State
std::atomic<bool> g_running = true;

// Define esp_menu globals
namespace esp_menu {
    bool g_kernelModeActive = false;
}

// Logger
void Log(const std::string& msg) {
    std::cout << msg << std::endl;
    std::ofstream f("log.txt", std::ios::app);
    if(f) f << msg << std::endl;
}

// ============================================
// Offsets
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
    constexpr uintptr_t m_ArmorValue = 0x1518;
}

// ============================================
// Direct Syscalls (KnownDlls Implementation)
// ============================================
extern "C" DWORD NtReadVirtualMemory_SSN;
extern "C" NTSTATUS NtReadVirtualMemory_Direct(
    HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer, SIZE_T Size, PSIZE_T BytesRead
);

typedef NTSTATUS (NTAPI *NtOpenSection_t)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
typedef NTSTATUS (NTAPI *NtMapViewOfSection_t)(HANDLE, HANDLE, PVOID*, ULONG_PTR, SIZE_T, PLARGE_INTEGER, PSIZE_T, DWORD, ULONG, ULONG);

DWORD GetSyscallIdClean() {
    HMODULE hNtdllLocal = GetModuleHandleA("ntdll.dll");
    if (!hNtdllLocal) return 0;
    
    auto NtOpenSection = (NtOpenSection_t)GetProcAddress(hNtdllLocal, "NtOpenSection");
    auto NtMapViewOfSection = (NtMapViewOfSection_t)GetProcAddress(hNtdllLocal, "NtMapViewOfSection");
    
    if (!NtOpenSection || !NtMapViewOfSection) return 0;

    UNICODE_STRING name;
    OBJECT_ATTRIBUTES oa;
    HANDLE section = NULL;
    PVOID base = NULL;
    SIZE_T size = 0;
    
    wchar_t path[] = L"\\KnownDlls\\ntdll.dll";
    name.Buffer = path;
    name.Length = sizeof(path) - sizeof(wchar_t);
    name.MaximumLength = sizeof(path);
    
    InitializeObjectAttributes(&oa, &name, OBJ_CASE_INSENSITIVE, NULL, NULL);
    
    if (NtOpenSection(&section, SECTION_MAP_READ, &oa) != 0) return 0;
    
    // ViewUnmap is 2
    NTSTATUS status = NtMapViewOfSection(section, GetCurrentProcess(), &base, 0, 0, NULL, &size, 2, 0, PAGE_READONLY);
    CloseHandle(section);
    
    if (status != 0 || !base) return 0;
    
    uint8_t* clean = (uint8_t*)base;
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)clean;
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(clean + dos->e_lfanew);
    PIMAGE_EXPORT_DIRECTORY exp = (PIMAGE_EXPORT_DIRECTORY)(clean + nt->OptionalHeader.DataDirectory[0].VirtualAddress);
    
    DWORD* names = (DWORD*)(clean + exp->AddressOfNames);
    DWORD* funcs = (DWORD*)(clean + exp->AddressOfFunctions);
    WORD* ords = (WORD*)(clean + exp->AddressOfNameOrdinals);
    
    DWORD ssn = 0;
    
    for (DWORD i = 0; i < exp->NumberOfNames; i++) {
        const char* fn = (char*)(clean + names[i]);
        if (strcmp(fn, "NtReadVirtualMemory") == 0) {
            uint8_t* stub = clean + funcs[ords[i]];
            if (stub[0] == 0x4C && stub[1] == 0x8B && stub[2] == 0xD1 && stub[3] == 0xB8) {
                ssn = *(DWORD*)(stub + 4);
            }
            break;
        }
    }
    return ssn;
}

// ============================================
// Cheat Context
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
    
    // Memory Cache
    struct SmoothEntity {
        omath::Vector3<float> origin;
        int health;
        int armor;
        bool valid;
    };
    
    std::mutex cacheMutex;
    std::array<SmoothEntity, 65> entityCache{};
    struct { float m[4][4]; } viewMatrix{};
    
    bool attach() {
        // 1. Init Syscall
        DWORD cleanSSN = GetSyscallIdClean();
        if (cleanSSN != 0) {
            NtReadVirtualMemory_SSN = cleanSSN;
            Log("[+] Stealth: Clean SSN Found (" + std::to_string(cleanSSN) + ")");
        } else {
            NtReadVirtualMemory_SSN = 0;
            Log("[!] Stealth: KnownDlls Failed. Using WinAPI.");
        }
        
        // 2. Find Process
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
        
        // 3. Open Handle (Minimal Rights)
        processHandle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (!processHandle) return false;
        
        // 4. Find Module
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
        
        // Removed AttachThreadInput as it causes crashes/deadlocks on some systems in Fullscreen.
        // For menu interaction in Exclusive Fullscreen, users should use Borderless or Alt-Tab.
        
        return clientBase != 0;
    }
    
    bool readRaw(uintptr_t addr, void* buf, size_t size) {
        // Try Syscall
        if (NtReadVirtualMemory_SSN > 0) {
            SIZE_T bytesRead = 0;
            NTSTATUS status = NtReadVirtualMemory_Direct(processHandle, (PVOID)addr, buf, size, &bytesRead);
            if (status != 0) {
                // Fallback on error
                NtReadVirtualMemory_SSN = 0;
                return ReadProcessMemory(processHandle, (LPCVOID)addr, buf, size, nullptr);
            }
            return true;
        }
        // Fallback WinAPI
        return ReadProcessMemory(processHandle, (LPCVOID)addr, buf, size, nullptr);
    }
    
    template<typename T>
    T read(uintptr_t addr) {
        T val{};
        readRaw(addr, &val, sizeof(T));
        return val;
    }
    
    bool ok() const { return processHandle && clientBase; }
};

CheatContext g_ctx;

// ============================================
// Input Hook (Pro Level Interaction)
// ============================================
HHOOK g_mouseHook = nullptr;

LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && esp_menu::g_menuOpen) {
        MSLLHOOKSTRUCT* mouse = (MSLLHOOKSTRUCT*)lParam;
        ImGuiIO& io = ImGui::GetIO();
        
        // 1. Update ImGui Mouse
        io.MousePos = ImVec2((float)mouse->pt.x, (float)mouse->pt.y);
        
        if (wParam == WM_LBUTTONDOWN) io.MouseDown[0] = true;
        else if (wParam == WM_LBUTTONUP) io.MouseDown[0] = false;
        else if (wParam == WM_RBUTTONDOWN) io.MouseDown[1] = true;
        else if (wParam == WM_RBUTTONUP) io.MouseDown[1] = false;
        
        // 2. Block Input if Menu is hovered
        // We let ImGui handle the logic. If menu is open, we generally consume clicks
        // to prevent shooting while configuring.
        return 1; // Eat the input (Game won't see it)
    }
    return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
}

// ============================================
// Window Proc
// ============================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_NCHITTEST:
        if (!esp_menu::g_menuOpen) return HTTRANSPARENT;
        break;
        
    case WM_SIZE:
        {
            int w = LOWORD(lParam);
            int h = HIWORD(lParam);
            if (w > 0 && h > 0) g_ctx.gameBounds = {0, 0, w, h};
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ============================================
// Memory Thread
// ============================================
void MemoryThreadLogic() {
    using namespace omath;
    Log("[*] Memory Thread Started (8ms tick)"); // LOG CHANGED
    
    while (g_running) {
        if (!g_ctx.ok()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Relaxed check
            continue;
        }

        // ... reading logic ... (keep same)

        // Read Matrix
        auto vm = g_ctx.read<decltype(g_ctx.viewMatrix)>(g_ctx.clientBase + offsets::dwViewMatrix);
        
        // Read Entity List
        uintptr_t localController = g_ctx.read<uintptr_t>(g_ctx.clientBase + offsets::dwLocalPlayerController);
        uintptr_t localPawn = 0;
        int localTeam = 0;
        
        if (localController) {
            uint32_t handle = g_ctx.read<uint32_t>(localController + offsets::m_hPlayerPawn);
            uintptr_t list = g_ctx.read<uintptr_t>(g_ctx.clientBase + offsets::dwEntityList);
            if (list) {
                uintptr_t entry = g_ctx.read<uintptr_t>(list + 0x8 * ((handle & 0x7FFF) >> 9) + 16);
                localPawn = g_ctx.read<uintptr_t>(entry + 112 * (handle & 0x1FF));
                localTeam = g_ctx.read<int>(localPawn + offsets::m_iTeamNum);
            }
        }

        uintptr_t entityList = g_ctx.read<uintptr_t>(g_ctx.clientBase + offsets::dwEntityList);
        
        // Update Cache
        {
            std::lock_guard<std::mutex> lock(g_ctx.cacheMutex);
            g_ctx.viewMatrix = vm;
            
            if (entityList) {
                for (int i = 1; i <= 64; i++) {
                    auto& ent = g_ctx.entityCache[i];
                    
                    uintptr_t listEntry = g_ctx.read<uintptr_t>(entityList + (8 * (i & 0x7FFF) >> 9) + 16);
                    if (!listEntry) { ent.valid = false; continue; }
                    
                    uintptr_t controller = g_ctx.read<uintptr_t>(listEntry + 112 * (i & 0x1FF));
                    if (!controller) { ent.valid = false; continue; }
                    
                    uint32_t pawnHandle = g_ctx.read<uint32_t>(controller + offsets::m_hPlayerPawn);
                    if (!pawnHandle) { ent.valid = false; continue; }
                    
                    uintptr_t listEntry2 = g_ctx.read<uintptr_t>(entityList + 0x8 * ((pawnHandle & 0x7FFF) >> 9) + 16);
                    uintptr_t pawn = g_ctx.read<uintptr_t>(listEntry2 + 112 * (pawnHandle & 0x1FF));
                    
                    if (!pawn || pawn == localPawn) { ent.valid = false; continue; }
                    
                    int health = g_ctx.read<int>(pawn + offsets::m_iHealth);
                    if (health <= 0 || health > 100) { ent.valid = false; continue; }

                    int team = g_ctx.read<int>(pawn + offsets::m_iTeamNum);
                    if (team == localTeam) { ent.valid = false; continue; }

                    uintptr_t node = g_ctx.read<uintptr_t>(pawn + offsets::m_pGameSceneNode);
                    
                    ent.origin = g_ctx.read<Vector3<float>>(node + offsets::m_vecAbsOrigin);
                    ent.health = health;
                    ent.armor = g_ctx.read<int>(pawn + offsets::m_ArmorValue);
                    ent.valid = true;
                }
            }
        }
        
        // OPTIMIZATION: Increased sleep to 8ms (~120 updates/sec)
        // 1ms was killing CPU on laptops causing DWM lag.
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
}

// ============================================
// Render Loop
// ============================================
float Lerp(float a, float b, float t) { return a + (b - a) * t; }

void render() {
    using namespace omath;
    auto& s = esp_menu::g_settings;
    if (!s.boxESP && !s.healthBar && !s.armorBar) return;

    auto* draw = ImGui::GetBackgroundDrawList();
    float dt = ImGui::GetIO().DeltaTime;
    
    // Copy cache
    decltype(g_ctx.viewMatrix) matrix;
    std::array<CheatContext::SmoothEntity, 65> entities;
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

    for (int i = 1; i <= 64; i++) {
        auto& ent = entities[i];
        if (!ent.valid) continue;
        
        static std::array<Vector3<float>, 65> s_smoothPos{};
        Vector3<float>& smoothPos = s_smoothPos[i];
        
        if (smoothPos.x == 0 && smoothPos.y == 0) smoothPos = ent.origin;
        
        float distSq = (smoothPos.x - ent.origin.x)*(smoothPos.x - ent.origin.x) + 
                       (smoothPos.y - ent.origin.y)*(smoothPos.y - ent.origin.y) + 
                       (smoothPos.z - ent.origin.z)*(smoothPos.z - ent.origin.z);
                       
        // Teleport check
        if (distSq > 400.0f) {
            smoothPos = ent.origin;
        } else {
            // "Dead" ESP Logic:
            // High speed interpolation to snap quickly, but low enough to smooth out memory jitter.
            // 60.0f * dt is very fast (approx 2-3 frames to settle).
            float speed = 60.0f * dt;
            smoothPos.x = Lerp(smoothPos.x, ent.origin.x, speed);
            smoothPos.y = Lerp(smoothPos.y, ent.origin.y, speed);
            smoothPos.z = Lerp(smoothPos.z, ent.origin.z, speed);
            
            // Micro-optimization: If very close, snap to prevent micro-blur
            if (distSq < 1.0f) smoothPos = ent.origin;
        }
        
        Vector3<float> head = smoothPos; head.z += 72.0f;
        Vector3<float> s_orig, s_head;
        
        if (!w2s(smoothPos, s_orig) || !w2s(head, s_head)) continue;

        float h = s_orig.y - s_head.y;
        float w = h * 0.4f;
        float x = s_head.x - w / 2;
        
        // CORNER BOX IMPLEMENTATION (More "Legit/Pro" look)
        if (s.boxESP) {
             // Black Outline
             draw->AddRect(ImVec2(x-1, s_head.y-1), ImVec2(x+w+1, s_orig.y+1), 0xFF000000, 0, 0, 3.0f);
             // Color Box
             draw->AddRect(ImVec2(x, s_head.y), ImVec2(x+w, s_orig.y), ImGui::ColorConvertFloat4ToU32(ImVec4(1,0,0,1)), 0, 0, 1.5f);
        }

        if (s.healthBar) {
            float healthH = h * (ent.health / 100.0f);
            draw->AddRectFilled(ImVec2(x-6, s_head.y), ImVec2(x-2, s_orig.y), 0x80000000); 
            draw->AddRectFilled(ImVec2(x-5, s_orig.y - healthH), ImVec2(x-3, s_orig.y), 0xFF00FF00); 
            draw->AddRect(ImVec2(x-6, s_head.y), ImVec2(x-2, s_orig.y), 0xFF000000, 0, 0, 1.0f); 
        }

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
    AllocConsole();
    FILE* f; freopen_s(&f, "CONOUT$", "w", stdout); freopen_s(&f, "CONOUT$", "w", stderr);
    std::ofstream("log.txt", std::ios::trunc);
    
    Log("[INFO] ExternaCS2 v3.0 Final Starting...");

    // 1. UIAccess
    bool isUIAccessChild = (wcsstr(lpCmdLine, L"--uiaccess-child") != nullptr);
    if (!isUIAccessChild) {
        if (uiaccess::AcquireUIAccessToken()) return 0; 
    }

    // 2. Driver (Optional Mapping)
    Log("[*] Initializing Driver...");
    auto drv = DriverExtractor::Extract();
    if (drv.success) {
        esp_menu::g_kernelModeActive = true; 
    }
    atexit(CleanupExtractedDriver);

    // 3. Attach Logic
    Log("[*] Waiting for cs2.exe...");
    while (!g_ctx.attach()) {
        Sleep(1000);
        std::cout << "." << std::flush;
    }
    Log("[+] Attached!");
    
    // 4. Memory Thread (Auto-Join)
    std::jthread memoryThread(MemoryThreadLogic);

    // 5. Overlay Strategy (Hijack -> UIAccess -> Standard)
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ExternaOverlayClass";
    RegisterClassExW(&wc);
    
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    g_ctx.gameBounds = {0, 0, w, h};

    // A. Try Hijack (Discord/NVIDIA)
    HWND hijackTarget = FindWindowW(L"DiscordOverlayHost", nullptr);
    if (!hijackTarget) hijackTarget = FindWindowW(L"CEF-OSC-WIDGET", nullptr); // NVIDIA
    
    if (hijackTarget) {
        Log("[+] Hijacking Overlay: " + std::to_string((uintptr_t)hijackTarget));
        g_ctx.overlayHwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            wc.lpszClassName, L"Externa Overlay",
            WS_POPUP | WS_VISIBLE,
            0, 0, w, h,
            hijackTarget, // Parent = Hijack
            nullptr, hInstance, nullptr
        );
    }
    
    // B. UIAccess (Fallback 1)
    if (!g_ctx.overlayHwnd && isUIAccessChild) {
        Log("[*] Creating UIAccess Window (Band)...");
        g_ctx.overlayHwnd = uiaccess::CreateUIAccessWindow(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            wc.lpszClassName, L"Externa Overlay", WS_POPUP, 0, 0, w, h, nullptr, nullptr, hInstance, nullptr
        );
    } 
    
    // C. Standard (Fallback 2)
    if (!g_ctx.overlayHwnd) {
        Log("[*] Creating Standard Window...");
        g_ctx.overlayHwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 
            wc.lpszClassName, L"Externa Overlay", WS_POPUP, 0, 0, w, h, 0, 0, hInstance, 0
        );
    }

    if (!g_ctx.overlayHwnd) return 1;
    
    // Transparency
    MARGINS margins = {-1};
    DwmExtendFrameIntoClientArea(g_ctx.overlayHwnd, &margins);
    
    // DX11 Init
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
    sd1.BufferCount = 1;
    sd1.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED; 
    sd1.Scaling = DXGI_SCALING_STRETCH;
    
    factory->CreateSwapChainForHwnd(g_ctx.dev, g_ctx.overlayHwnd, &sd1, nullptr, nullptr, &g_ctx.swapChain);
    factory->Release(); adapter->Release(); dxgiDevice->Release();

    ID3D11Texture2D* backBuffer = nullptr;
    g_ctx.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    g_ctx.dev->CreateRenderTargetView(backBuffer, nullptr, &g_ctx.renderTarget);
    backBuffer->Release();

    ShowWindow(g_ctx.overlayHwnd, SW_SHOW);
    
    // ImGui Init
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(g_ctx.overlayHwnd);
    ImGui_ImplDX11_Init(g_ctx.dev, g_ctx.ctx);
    
    // Install Mouse Hook
    g_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, GetModuleHandle(nullptr), 0);
    Log("[+] Input Hook Installed");

    // Loop
    MSG msg{};
    while (g_running) {
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessage(&msg);
            if (msg.message == WM_QUIT) g_running = false;
        }
        
        // Input Toggle
        if (GetAsyncKeyState(VK_INSERT) & 1) esp_menu::g_menuOpen = !esp_menu::g_menuOpen;
        
        // REMOVED OLD FOCUS LOGIC (No longer needed with Hook)
        
        // Check Game
        if (g_ctx.processHandle) {
            DWORD exitCode = 0;
            if (GetExitCodeProcess(g_ctx.processHandle, &exitCode) && exitCode != STILL_ACTIVE) g_running = false;
        }
        
        // FOCUS CHECK: Only render if Game or Menu is active
        HWND fg = GetForegroundWindow();
        bool isGameActive = (fg == g_ctx.gameHwnd);
        bool isOverlayActive = (fg == g_ctx.overlayHwnd);
        
        static bool wasActive = true;
        
        // DEBOUNCE: Don't panic on micro-focus loss (e.g. during alt-tab transition)
        static int focusLossCounter = 0;
        
        if (!isGameActive && !isOverlayActive && !esp_menu::g_menuOpen) {
            focusLossCounter++;
            if (focusLossCounter > 60) { // Wait ~1 sec (60 frames) before sleeping
                if (wasActive) {
                    // Log("[-] Focus lost. Pausing..."); // Spam removed
                    wasActive = false;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                // Just clear and present once, then wait
                float clear[4] = {0,0,0,0};
                g_ctx.ctx->OMSetRenderTargets(1, &g_ctx.renderTarget, nullptr);
                g_ctx.ctx->ClearRenderTargetView(g_ctx.renderTarget, clear);
                g_ctx.swapChain->Present(1, 0);
                continue;
            }
        } else {
            focusLossCounter = 0;
            if (!wasActive) {
                // Log("[+] Focus regained."); // Spam removed
                wasActive = true;
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
    DestroyWindow(g_ctx.overlayHwnd);
    return 0;
}

