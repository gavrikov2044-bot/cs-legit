/*
 * EXTERNAL ESP v1.0.0 - CS2 Overlay
 * Based on IMXNOOBX/cs2-external-esp
 * https://github.com/IMXNOOBX/cs2-external-esp
 * 
 * Controls:
 *   INSERT - Show/Hide menu
 *   END    - Exit
 */

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <TlHelp32.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <dxgi1_2.h>      // For DXGI 1.2 (CreateSwapChainForComposition)
#include <dcomp.h>        // DirectComposition
#include <timeapi.h>      // For timeBeginPeriod

// NT types for NtReadVirtualMemory
#ifndef NTSTATUS
typedef LONG NTSTATUS;
#endif
#ifndef NTAPI
#define NTAPI __stdcall
#endif

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
#include <map>
#include <immintrin.h> // SIMD Optimization
#include <fstream> // For Config

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "esp_menu.hpp"  // NEW: Modern ESP menu

// OMath library for Source Engine
#include <omath/omath.hpp>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "winmm.lib")

// ============================================
// CS2 Offsets (from cs2-dumper 2025-12-21)
// Format: decimal (same as IMXNOOBX offsets.json)
// ============================================
namespace offsets {
    // client.dll
    constexpr uintptr_t dwEntityList = 0x1D13CE8;           // 30489832
    constexpr uintptr_t dwLocalPlayerController = 0x1E1DC18; // 31579160
    constexpr uintptr_t dwViewMatrix = 0x1E323D0;            // 31663056
    
    // C_BaseEntity / C_CSPlayerPawn
    constexpr uintptr_t m_iHealth = 0x34C;           // 844
    constexpr uintptr_t m_iTeamNum = 0x3EB;          // 1003
    constexpr uintptr_t m_pGameSceneNode = 0x330;    // 816
    constexpr uintptr_t m_hController = 0x15B8;      // 5560
    
    // CGameSceneNode - more accurate position!
    constexpr uintptr_t m_vecAbsOrigin = 0xD0;       // 208 - from GameSceneNode
    constexpr uintptr_t m_modelState = 0x190;        // CSkeletonInstance (Updated from 0x170)
    constexpr uintptr_t m_boneArray = 0x80;          // m_modelState + 0x80 = bone array
    
    // C_CSPlayerPawn
    constexpr uintptr_t m_ArmorValue = 0x274C;       // 10060 - armor
    
    // CCSPlayerController  
    constexpr uintptr_t m_hPlayerPawn = 0x8FC;       // 2300
    constexpr uintptr_t m_iszPlayerName = 0x6E8;     // 1768
    
    // NEW: Advanced ESP offsets (from offsets/cpp/client_dll.hpp)
    constexpr uintptr_t m_hObserverTarget = 0x44;    // C_PlayerObserverServices::m_hObserverTarget
    constexpr uintptr_t m_iObserverMode = 0x40;      // C_PlayerObserverServices::m_iObserverMode
    constexpr uintptr_t m_bSpotted = 0x8;            // C_BaseEntity::m_bSpotted
    constexpr uintptr_t m_flC4Blow = 0x1190;         // C_PlantedC4::m_flC4Blow
    constexpr uintptr_t m_bBombPlanted = 0x9A9;     // C_PlantedC4::m_bBombPlanted
    constexpr uintptr_t m_AttributeManager = 0x1438; // C_BaseEntity::m_AttributeManager (for weapons)
    constexpr uintptr_t m_Item = 0x50;               // C_AttributeContainer::m_Item
    constexpr uintptr_t m_iItemDefinitionIndex = 0x1BA; // CEconItemView::m_iItemDefinitionIndex
    
    // Bone indices for skeleton
    namespace bones {
        constexpr int HEAD = 6;
        constexpr int NECK = 5;
        constexpr int SPINE1 = 4;
        constexpr int SPINE2 = 2;
        constexpr int PELVIS = 0;
        constexpr int ARM_L_UPPER = 8;
        constexpr int ARM_L_LOWER = 9;
        constexpr int ARM_L_HAND = 10;
        constexpr int ARM_R_UPPER = 13;
        constexpr int ARM_R_LOWER = 14;
        constexpr int ARM_R_HAND = 15;
        constexpr int LEG_L_UPPER = 22;
        constexpr int LEG_L_LOWER = 23;
        constexpr int LEG_L_FOOT = 24;
        constexpr int LEG_R_UPPER = 25;
        constexpr int LEG_R_LOWER = 26;
        constexpr int LEG_R_FOOT = 27;
    }
}

// ============================================
// Types (using omath)
// ============================================
using Vec2 = omath::Vector2<float>;
using Vec3 = omath::Vector3<float>;

// ViewMatrix as read from CS2 memory (row-major)
struct ViewMatrix { 
    float m[4][4];
    float* operator[](int i) { return m[i]; }
    const float* operator[](int i) const { return m[i]; }
};

struct Entity {
    bool valid = false;
    bool enemy = false;
    int health = 0;
    int team = 0;
    uintptr_t pawn = 0;  // Store pawn address for real-time position reading
    std::string name;
};



// ============================================
// Config
// ============================================
struct Config {
    bool espEnabled = true;
    bool espBox = true;
    bool espCorneredBox = false;  // ohud-style cornered box
    bool espFill = false;         // filled box
    float espFillOpacity = 15.f;  // fill opacity 0-100%
    bool espSkeleton = false;     // bone ESP
    bool espHealth = true;
    bool espArmor = true;         // armor bar
    bool espName = true;
    bool espDistance = true;
    bool espSnaplines = false;
    bool espSniperCrosshair = true; // Sniper crosshair
    bool espHeadDot = false;      // Head Dot
    float espHeadDotSize = 2.0f;  // Head Dot Size
    bool espEnemyOnly = true;
    bool espOutlinedText = true;  // ohud-style outlined text
    float espMaxDistance = 500.0f;
    // Sync settings - DISABLED by default for max FPS
    bool vsyncOverlay = false;     // VSync OFF = unlimited FPS
    int fpsLimit = 0;              // 0 = unlimited (match any game FPS)
    float colorEnemy[4] = {1.0f, 0.2f, 0.2f, 1.0f};
    float colorTeam[4] = {0.2f, 0.6f, 1.0f, 1.0f};
    float colorSkeleton[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float colorHeadDot[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    bool showMenu = true;
};

// ============================================
// ESP Drawing Helpers (inspired by ohud)
// ============================================
namespace esp {
    // Draw outlined text (readable on any background)
    void drawOutlinedText(ImDrawList* draw, ImVec2 pos, ImU32 color, const char* text) {
        const ImU32 black = IM_COL32(0, 0, 0, 255);
        draw->AddText({pos.x - 1, pos.y - 1}, black, text);
        draw->AddText({pos.x + 1, pos.y - 1}, black, text);
        draw->AddText({pos.x - 1, pos.y + 1}, black, text);
        draw->AddText({pos.x + 1, pos.y + 1}, black, text);
        draw->AddText(pos, color, text);
    }
    
    // Draw cornered box (ohud style)
    void drawCorneredBox(ImDrawList* draw, float x, float y, float w, float h, ImU32 color, float cornerRatio = 0.25f) {
        float len = w * cornerRatio;
        
        // Top-left
        draw->AddLine({x, y}, {x + len, y}, color, 2.f);
        draw->AddLine({x, y}, {x, y + len}, color, 2.f);
        
        // Top-right
        draw->AddLine({x + w, y}, {x + w - len, y}, color, 2.f);
        draw->AddLine({x + w, y}, {x + w, y + len}, color, 2.f);
        
        // Bottom-left
        draw->AddLine({x, y + h}, {x + len, y + h}, color, 2.f);
        draw->AddLine({x, y + h}, {x, y + h - len}, color, 2.f);
        
        // Bottom-right
        draw->AddLine({x + w, y + h}, {x + w - len, y + h}, color, 2.f);
        draw->AddLine({x + w, y + h}, {x + w, y + h - len}, color, 2.f);
    }
    
    // Health bar with gradient
    void drawHealthBar(ImDrawList* draw, float x, float y, float /*w*/, float h, int health) {
        float hp = health / 100.f;
        float barHeight = h * hp;
        
        // Background
        draw->AddRectFilled({x - 6, y}, {x - 2, y + h}, IM_COL32(0, 0, 0, 180));
        
        // Health gradient (green->yellow->red)
        ImU32 healthColor;
        if (hp > 0.5f) {
            healthColor = IM_COL32((int)((1 - hp) * 2 * 255), 255, 0, 255);
        } else {
            healthColor = IM_COL32(255, (int)(hp * 2 * 255), 0, 255);
        }
        
        draw->AddRectFilled({x - 5, y + h - barHeight}, {x - 3, y + h}, healthColor);
        
        // Outline
        draw->AddRect({x - 6, y}, {x - 2, y + h}, IM_COL32(0, 0, 0, 255));
    }
    
    // Armor bar (blue)
    void drawArmorBar(ImDrawList* draw, float x, float y, float w, float h, int armor) {
        if (armor <= 0) return;
        
        float ap = armor / 100.f;
        float barHeight = h * ap;
        
        // Background
        draw->AddRectFilled({x + w + 2, y}, {x + w + 6, y + h}, IM_COL32(0, 0, 0, 180));
        
        // Armor (blue)
        draw->AddRectFilled({x + w + 3, y + h - barHeight}, {x + w + 5, y + h}, IM_COL32(50, 150, 255, 255));
        
        // Outline
        draw->AddRect({x + w + 2, y}, {x + w + 6, y + h}, IM_COL32(0, 0, 0, 255));
    }
    
    // Draw ESP preview for menu (high quality)
    void drawPreview(ImDrawList* draw, float centerX, float centerY, const Config& cfg) {
        const float W = 38.f;
        const float H = 85.f;
        const float x = centerX - W / 2.f;
        const float y = centerY - H / 2.f;
        
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(cfg.colorEnemy[0], cfg.colorEnemy[1], cfg.colorEnemy[2], 1.f));
        ImU32 skelCol = ImGui::ColorConvertFloat4ToU32(ImVec4(cfg.colorSkeleton[0], cfg.colorSkeleton[1], cfg.colorSkeleton[2], 1.f));
        
        // Name (above)
        if (cfg.espName) {
            const char* txt = "Enemy";
            ImVec2 sz = ImGui::CalcTextSize(txt);
            ImVec2 pos(centerX - sz.x / 2.f, y - sz.y - 4.f);
            if (cfg.espOutlinedText) {
                drawOutlinedText(draw, pos, IM_COL32(255, 255, 255, 255), txt);
            } else {
                draw->AddText(pos, IM_COL32(255, 255, 255, 255), txt);
            }
        }
        
        // Fill (semi-transparent)
        if (cfg.espFill) {
            uint8_t alpha = static_cast<uint8_t>(cfg.espFillOpacity * 2.55f);
            ImU32 fillCol = (col & 0x00FFFFFF) | (static_cast<ImU32>(alpha) << 24);
            draw->AddRectFilled(ImVec2(x, y), ImVec2(x + W, y + H), fillCol);
        }
        
        // Box
        if (cfg.espBox) {
            if (cfg.espCorneredBox) {
                drawCorneredBox(draw, x, y, W, H, col, 0.25f);
            } else {
                draw->AddRect(ImVec2(x - 1, y - 1), ImVec2(x + W + 1, y + H + 1), IM_COL32(0, 0, 0, 180));
                draw->AddRect(ImVec2(x, y), ImVec2(x + W, y + H), col, 0, 0, 2.f);
            }
        }
        
        // Skeleton preview
        if (cfg.espSkeleton) {
            float headY = y + 8;
            float neckY = y + 16;
            float chestY = y + 28;
            float waistY = y + 45;
            float hipY = y + 50;
            float kneeY = y + 68;
            float footY = y + H - 2;
            float shoulderW = 12;
            float hipW = 8;
            
            // Spine
            draw->AddLine(ImVec2(centerX, headY), ImVec2(centerX, neckY), skelCol, 1.5f);
            draw->AddLine(ImVec2(centerX, neckY), ImVec2(centerX, chestY), skelCol, 1.5f);
            draw->AddLine(ImVec2(centerX, chestY), ImVec2(centerX, waistY), skelCol, 1.5f);
            draw->AddLine(ImVec2(centerX, waistY), ImVec2(centerX, hipY), skelCol, 1.5f);
            // Arms
            draw->AddLine(ImVec2(centerX, neckY), ImVec2(centerX - shoulderW, chestY + 5), skelCol, 1.5f);
            draw->AddLine(ImVec2(centerX - shoulderW, chestY + 5), ImVec2(centerX - shoulderW - 3, waistY), skelCol, 1.5f);
            draw->AddLine(ImVec2(centerX, neckY), ImVec2(centerX + shoulderW, chestY + 5), skelCol, 1.5f);
            draw->AddLine(ImVec2(centerX + shoulderW, chestY + 5), ImVec2(centerX + shoulderW + 3, waistY), skelCol, 1.5f);
            // Legs
            draw->AddLine(ImVec2(centerX, hipY), ImVec2(centerX - hipW, kneeY), skelCol, 1.5f);
            draw->AddLine(ImVec2(centerX - hipW, kneeY), ImVec2(centerX - hipW - 2, footY), skelCol, 1.5f);
            draw->AddLine(ImVec2(centerX, hipY), ImVec2(centerX + hipW, kneeY), skelCol, 1.5f);
            draw->AddLine(ImVec2(centerX + hipW, kneeY), ImVec2(centerX + hipW + 2, footY), skelCol, 1.5f);
            // Head circle
            draw->AddCircle(ImVec2(centerX, headY - 4), 5.f, skelCol, 12, 1.5f);
        }
        
        // Health bar (left)
        if (cfg.espHealth) {
            float bx = x - 6.f;
            float hp = 0.78f;
            draw->AddRectFilled(ImVec2(bx, y), ImVec2(bx + 4, y + H), IM_COL32(0, 0, 0, 180));
            draw->AddRectFilled(ImVec2(bx + 1, y + H * (1.f - hp)), ImVec2(bx + 3, y + H), IM_COL32(80, 210, 0, 255));
        }
        
        // Armor bar (right)
        if (cfg.espArmor) {
            float bx = x + W + 2.f;
            float ap = 0.62f;
            draw->AddRectFilled(ImVec2(bx, y), ImVec2(bx + 4, y + H), IM_COL32(0, 0, 0, 180));
            draw->AddRectFilled(ImVec2(bx + 1, y + H * (1.f - ap)), ImVec2(bx + 3, y + H), IM_COL32(50, 140, 255, 255));
        }
        
        // Distance (below)
        float bottomY = y + H + 4.f;
        if (cfg.espDistance) {
            const char* txt = "24m";
            ImVec2 sz = ImGui::CalcTextSize(txt);
            ImVec2 pos(centerX - sz.x / 2.f, bottomY);
            if (cfg.espOutlinedText) {
                drawOutlinedText(draw, pos, IM_COL32(255, 255, 255, 200), txt);
            } else {
                draw->AddText(pos, IM_COL32(255, 255, 255, 200), txt);
            }
            bottomY += sz.y + 3.f;
        }
        
        // Snapline
        if (cfg.espSnaplines) {
            draw->AddLine(ImVec2(centerX, bottomY), ImVec2(centerX, bottomY + 20.f), col, 1.f);
        }
    }
}

// ============================================
// Direct Syscall - fastest memory read (bypasses ntdll hooks)
// ============================================
static DWORD g_syscallNumber = 0;

// Get syscall number from ntdll stub
DWORD getSyscallNumber(const char* funcName) {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) return 0;
    
    BYTE* func = (BYTE*)GetProcAddress(ntdll, funcName);
    if (!func) return 0;
    
    // NtReadVirtualMemory starts with: mov r10, rcx; mov eax, <syscall_number>
    // Pattern: 4C 8B D1 B8 XX XX 00 00
    if (func[0] == 0x4C && func[1] == 0x8B && func[2] == 0xD1 && func[3] == 0xB8) {
        return *(DWORD*)(func + 4);
    }
    
    // Alternative pattern (Windows 11): mov eax, <syscall>
    if (func[0] == 0xB8) {
        return *(DWORD*)(func + 1);
    }
    
    return 0;
}

// Direct syscall wrapper for NtReadVirtualMemory
extern "C" NTSTATUS DirectSyscall(
    DWORD syscallNumber,
    HANDLE ProcessHandle,
    PVOID BaseAddress,
    PVOID Buffer,
    SIZE_T Size,
    PSIZE_T BytesRead
);

// Fallback to NtReadVirtualMemory if syscall fails
using NtReadVirtualMemory_t = NTSTATUS(NTAPI*)(
    HANDLE ProcessHandle,
    PVOID BaseAddress,
    PVOID Buffer,
    SIZE_T NumberOfBytesToRead,
    PSIZE_T NumberOfBytesRead
);
static NtReadVirtualMemory_t g_NtRead = nullptr;

// Direct syscall read - uses ASM stub when available
__forceinline NTSTATUS DoSyscallRead(HANDLE proc, PVOID addr, PVOID buf, SIZE_T size) {
    // Try direct syscall first (faster, bypasses hooks)
    if (g_syscallNumber != 0) {
        SIZE_T bytesRead = 0;
        return DirectSyscall(g_syscallNumber, proc, addr, buf, size, &bytesRead);
    }
    
    // Fallback to NtReadVirtualMemory
    if (g_NtRead) {
        return g_NtRead(proc, addr, buf, size, nullptr);
    }
    
    return -1;
}

// ============================================
// Memory - Ultra-fast reads
// ============================================
class Memory {
public:
    HANDLE handle = nullptr;
    DWORD pid = 0;
    uintptr_t client = 0;
    HWND gameHwnd = nullptr;
    
    bool attach() {
        // Get syscall number for NtReadVirtualMemory
        g_syscallNumber = getSyscallNumber("NtReadVirtualMemory");
        
        // Also get function pointer as fallback
        HMODULE ntdll = GetModuleHandleA("ntdll.dll");
        if (ntdll) {
            g_NtRead = (NtReadVirtualMemory_t)GetProcAddress(ntdll, "NtReadVirtualMemory");
        }
        
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32W entry{}; entry.dwSize = sizeof(entry);
        
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
        
        handle = OpenProcess(PROCESS_VM_READ, FALSE, pid);
        if (!handle) return false;
        
        // Get client.dll base
        snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
        MODULEENTRY32W mod{}; mod.dwSize = sizeof(mod);
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
        
        return client != 0 && (g_NtRead != nullptr || g_syscallNumber != 0);
    }
    
    // Ultra-fast read using direct syscall or NtReadVirtualMemory
    template<typename T> __forceinline T read(uintptr_t addr) {
        T val{}; 
        DoSyscallRead(handle, (PVOID)addr, &val, sizeof(T));
        return val;
    }
    
    __forceinline void readRaw(uintptr_t addr, void* buf, size_t size) {
        DoSyscallRead(handle, (PVOID)addr, buf, size);
    }
    
    bool ok() const { return handle && client && (g_NtRead || g_syscallNumber); }
};

// ============================================
// ASYNC SNAPSHOT SYSTEM (Professional Architecture)
// ============================================

// Complete snapshot of everything needed to draw ONE frame
// Reader thread fills this, Render thread consumes it.
struct DrawSnapshot {
    ViewMatrix matrix{};
    Vec3 localOrigin{};
    int localTeam = 0;
    
    struct PlayerData {
        bool valid = false;
        bool isEnemy = false;
        Vec3 origin{};
        Vec3 headPos{};
        int health = 0;
        int armor = 0;
        char name[64] = {};
        
        // Bones
        bool bonesValid = false;
        Vec3 boneHead{};
        Vec3 boneNeck{};
        Vec3 boneSpine1{};
        Vec3 boneSpine2{};
        Vec3 bonePelvis{};
        Vec3 boneArmL[3]{};
        Vec3 boneArmR[3]{};
        Vec3 boneLegL[3]{};
        Vec3 boneLegR[3]{};
    };
    
    // Fixed array to avoid allocations in render loop
    PlayerData players[64];
    size_t playerCount = 0;
    
    // NEW: Advanced ESP Data
    struct SpectatorData {
        char name[64] = {};
        bool valid = false;
    };
    SpectatorData spectators[64];
    size_t spectatorCount = 0;
    
    struct BombData {
        bool planted = false;
        float timeRemaining = 0.f; // seconds until explosion
        Vec3 position{}; // C4 position
    };
    BombData bomb{};
    
    struct LootItem {
        Vec3 position{};
        int weaponID = 0;
        char name[32] = {};
        bool valid = false;
    };
    LootItem loot[128]; // Max 128 loot items
    size_t lootCount = 0;
    
    struct SoundEvent {
        Vec3 position{};
        const char* type = nullptr; // "Footstep", "Gunshot", "Reload"
        float fadeTime = 2.0f; // seconds until fade out
        std::chrono::steady_clock::time_point timestamp;
    };
    std::vector<SoundEvent> sounds; // Dynamic for sound events
    
    std::chrono::steady_clock::time_point timestamp;
};

// Double buffer: Reader writes to 'Back', Renderer reads 'Front'
DrawSnapshot g_snapshotBack;
DrawSnapshot g_snapshotFront;
std::mutex g_snapshotMutex; // Protects swap only

// ============================================
// Globals
// ============================================
Memory g_mem;
Config g_cfg;
std::vector<Entity> g_entities;
ViewMatrix g_matrix{};
int g_localTeam = 0;
uintptr_t g_localPawn = 0;
RECT g_gameBounds{};
std::atomic<bool> g_running{true};
std::mutex g_mutex;

ID3D11Device* g_dev = nullptr;
ID3D11DeviceContext* g_ctx = nullptr;
IDXGISwapChain1* g_swap = nullptr;  // DXGI 1.2 for DirectComposition
ID3D11RenderTargetView* g_rtv = nullptr;
HWND g_hwnd = nullptr;

// DirectComposition for bypass DWM 60fps limit
IDCompositionDevice* g_dcompDevice = nullptr;
IDCompositionTarget* g_dcompTarget = nullptr;
IDCompositionVisual* g_dcompVisual = nullptr;
bool g_useDirectComposition = false;  // Will be set if DComp succeeds

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ============================================
// READER THREAD - Heavy lifting happens here
// ============================================
void dataReaderLoop() {
    // Priority: slightly lower than render, but high enough to keep up
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    
    // Performance Timer
    using clock = std::chrono::steady_clock;
    auto nextFrameTime = clock::now();
    const auto targetFrameTime = std::chrono::microseconds(1000000 / 144); // Target 144Hz updates
    
    while (g_running) {
        auto now = clock::now();
        if (now < nextFrameTime) {
            // Smart Sleep: yield CPU until next frame time
            std::this_thread::sleep_for(std::chrono::microseconds(100)); // Micro-sleeps
            continue;
        }
        nextFrameTime = now + targetFrameTime;

        if (!g_mem.ok()) {
            Sleep(100);
            continue;
        }
        
        // 1. Prepare new snapshot
        DrawSnapshot& snap = g_snapshotBack;
        snap.playerCount = 0;
        snap.timestamp = std::chrono::steady_clock::now();
        
        // 2. Read Globals (Matrix, LocalPlayer)
        g_mem.readRaw(g_mem.client + offsets::dwViewMatrix, &snap.matrix, sizeof(ViewMatrix));
        
        uintptr_t localPawn = 0;
        uintptr_t localController = g_mem.read<uintptr_t>(g_mem.client + offsets::dwLocalPlayerController);
        if (localController) {
            uint32_t pawnHandle = g_mem.read<uint32_t>(localController + offsets::m_hPlayerPawn);
            uintptr_t entList = g_mem.read<uintptr_t>(g_mem.client + offsets::dwEntityList);
            if (pawnHandle && entList) {
                uintptr_t entry = g_mem.read<uintptr_t>(entList + 0x8 * ((pawnHandle & 0x7FFF) >> 9) + 16);
                localPawn = g_mem.read<uintptr_t>(entry + 112 * (pawnHandle & 0x1FF));
            }
        }
        g_localPawn = localPawn; // Update global for shared logic if needed
        
        if (localPawn) {
            snap.localTeam = g_mem.read<int>(localPawn + offsets::m_iTeamNum);
            uintptr_t localNode = g_mem.read<uintptr_t>(localPawn + offsets::m_pGameSceneNode);
            if (localNode) {
                snap.localOrigin = g_mem.read<Vec3>(localNode + offsets::m_vecAbsOrigin);
            }
        }
        
        // 3. Update Pawn Cache (Simplified logic inside reader)
        // We iterate the entity list directly here to build the snapshot
        uintptr_t entityList = g_mem.read<uintptr_t>(g_mem.client + offsets::dwEntityList);
        if (!entityList) continue;
        
        for (int i = 1; i <= 64; i++) {
            uintptr_t listEntry = g_mem.read<uintptr_t>(entityList + (8 * (i & 0x7FFF) >> 9) + 16);
            if (!listEntry) continue;
            
            uintptr_t controller = g_mem.read<uintptr_t>(listEntry + 112 * (i & 0x1FF));
            if (!controller) continue;
            
            uint32_t pawnHandle = g_mem.read<uint32_t>(controller + offsets::m_hPlayerPawn);
            if (!pawnHandle) continue;
            
            uintptr_t listEntry2 = g_mem.read<uintptr_t>(entityList + 0x8 * ((pawnHandle & 0x7FFF) >> 9) + 16);
            if (!listEntry2) continue;
            
            uintptr_t pawn = g_mem.read<uintptr_t>(listEntry2 + 112 * (pawnHandle & 0x1FF));
            if (!pawn || pawn == localPawn) continue;
            
            // Check health first (fastest filter)
            int health = g_mem.read<int>(pawn + offsets::m_iHealth);
            if (health <= 0 || health > 100) continue;
            
            // We have a valid player! Fill data.
            auto& p = snap.players[snap.playerCount];
            p.valid = true;
            p.health = health;
            
            // Read Armor
            p.armor = g_mem.read<int>(pawn + offsets::m_ArmorValue);
            
            int team = g_mem.read<int>(pawn + offsets::m_iTeamNum);
            p.isEnemy = (team != snap.localTeam);
            
            if (g_cfg.espEnemyOnly && !p.isEnemy) continue;
            
            // Name
            g_mem.readRaw(controller + offsets::m_iszPlayerName, p.name, sizeof(p.name) - 1);
            
            // Position
            uintptr_t sceneNode = g_mem.read<uintptr_t>(pawn + offsets::m_pGameSceneNode);
            if (!sceneNode) continue;
            
            p.origin = g_mem.read<Vec3>(sceneNode + offsets::m_vecAbsOrigin);
            p.headPos = {p.origin.x, p.origin.y, p.origin.z + 75.f};
            
            // Bones (Smart Caching)
            p.bonesValid = false;
            
            float dist = 0.f;
            if (snap.localOrigin.x != 0) {
                dist = (p.origin - snap.localOrigin).length() / 100.f;
            }
            
            // Move frameCount to static scope properly if needed, or make it local to loop iteration if logic permits.
            // But here it was intended to be a global counter for frame skipping.
            static int frameCount = 0;
            frameCount++; // Increment here
            
            // Read every frame if close, else every 5th frame
            bool shouldReadBones = (dist < 20.f) || (frameCount % 5 == 0);
            
            if ((g_cfg.espSkeleton || g_cfg.espBox) && shouldReadBones) {
                uintptr_t modelState = sceneNode + offsets::m_modelState;
                uintptr_t boneArray = g_mem.read<uintptr_t>(modelState + offsets::m_boneArray);
                
                if (boneArray) {
                    struct RawBone { float x, y, z; char pad[20]; } tempBones[32];
                    g_mem.readRaw(boneArray, tempBones, sizeof(tempBones));
                    
                    auto getBone = [&](int idx) -> Vec3 {
                        return {tempBones[idx].x, tempBones[idx].y, tempBones[idx].z};
                    };
                    
                    p.boneHead = getBone(offsets::bones::HEAD);
                    p.boneNeck = getBone(offsets::bones::NECK);
                    p.boneSpine1 = getBone(offsets::bones::SPINE1);
                    p.boneSpine2 = getBone(offsets::bones::SPINE2);
                    p.bonePelvis = getBone(offsets::bones::PELVIS);
                    
                    p.boneArmL[0] = getBone(offsets::bones::ARM_L_UPPER);
                    p.boneArmL[1] = getBone(offsets::bones::ARM_L_LOWER);
                    p.boneArmL[2] = getBone(offsets::bones::ARM_L_HAND);
                    
                    p.boneArmR[0] = getBone(offsets::bones::ARM_R_UPPER);
                    p.boneArmR[1] = getBone(offsets::bones::ARM_R_LOWER);
                    p.boneArmR[2] = getBone(offsets::bones::ARM_R_HAND);
                    
                    p.boneLegL[0] = getBone(offsets::bones::LEG_L_UPPER);
                    p.boneLegL[1] = getBone(offsets::bones::LEG_L_LOWER);
                    p.boneLegL[2] = getBone(offsets::bones::LEG_L_FOOT);
                    
                    p.boneLegR[0] = getBone(offsets::bones::LEG_R_UPPER);
                    p.boneLegR[1] = getBone(offsets::bones::LEG_R_LOWER);
                    p.boneLegR[2] = getBone(offsets::bones::LEG_R_FOOT);
                    
                    if (p.boneHead.x != 0 || p.boneHead.y != 0) p.bonesValid = true;
                }
            }
            
            snap.playerCount++;
        }
        
        // ====================================
        // NEW: Read Advanced ESP Data
        // ====================================
        
        // 1. SPECTATOR LIST - Check who's watching local player
        snap.spectatorCount = 0;
        if (localPawn) {
            for (int i = 1; i <= 64; i++) {
                uintptr_t listEntry = g_mem.read<uintptr_t>(entityList + (8 * (i & 0x7FFF) >> 9) + 16);
                if (!listEntry) continue;
                
                uintptr_t controller = g_mem.read<uintptr_t>(listEntry + 112 * (i & 0x1FF));
                if (!controller) continue;
                
                // Check if this player is spectating
                uint32_t observerTarget = g_mem.read<uint32_t>(controller + offsets::m_hObserverTarget);
                int observerMode = g_mem.read<int>(controller + offsets::m_iObserverMode);
                
                // If observer mode > 0 and target matches local pawn handle
                if (observerMode > 0 && observerTarget) {
                    uint32_t localPawnHandle = 0;
                    if (localController) {
                        localPawnHandle = g_mem.read<uint32_t>(localController + offsets::m_hPlayerPawn);
                    }
                    
                    // Check if observer target matches local pawn
                    if ((observerTarget & 0x7FFF) == (localPawnHandle & 0x7FFF)) {
                        // Read spectator name
                        uintptr_t namePtr = g_mem.read<uintptr_t>(controller + offsets::m_iszPlayerName);
                        if (namePtr) {
                            auto& spec = snap.spectators[snap.spectatorCount];
                            g_mem.readRaw(namePtr, spec.name, sizeof(spec.name) - 1);
                            spec.name[sizeof(spec.name) - 1] = '\0';
                            spec.valid = true;
                            snap.spectatorCount++;
                        }
                    }
                }
            }
        }
        
        // 2. BOMB TIMER - Read C4 state
        snap.bomb.planted = false;
        snap.bomb.timeRemaining = 0.f;
        
        // Iterate entities to find C4
        // Note: C4 entity class name is usually "C_PlantedC4" or similar
        // For now, we'll check all entities for bomb flags
        for (int i = 0; i < 1024; i++) { // Check more entities for C4
            uintptr_t listEntry = g_mem.read<uintptr_t>(entityList + (8 * (i & 0x7FFF) >> 9) + 16);
            if (!listEntry) continue;
            
            uintptr_t entity = g_mem.read<uintptr_t>(listEntry + 112 * (i & 0x1FF));
            if (!entity) continue;
            
            // Check if it's C4 - read bomb planted flag
            // Note: Offset may need adjustment based on actual CS2 structure
            bool bombPlanted = g_mem.read<bool>(entity + offsets::m_bBombPlanted);
            if (bombPlanted) {
                // m_flC4Blow is typically the game time when bomb will explode
                // We need to compare with current game time
                // For now, read it as time remaining (may need adjustment)
                float blowTime = g_mem.read<float>(entity + offsets::m_flC4Blow);
                
                // If blowTime is a timestamp, calculate remaining time
                // If it's already time remaining, use it directly
                // Common CS2 pattern: m_flC4Blow is game time, so we need current game time
                // For simplicity, assume it's time remaining if < 50 seconds
                if (blowTime > 0.f && blowTime < 50.f) {
                    snap.bomb.timeRemaining = blowTime;
                    snap.bomb.planted = true;
                    
                    // Get C4 position
                    uintptr_t sceneNode = g_mem.read<uintptr_t>(entity + offsets::m_pGameSceneNode);
                    if (sceneNode) {
                        snap.bomb.position = g_mem.read<Vec3>(sceneNode + offsets::m_vecAbsOrigin);
                    }
                    break; // Found C4, no need to continue
                }
            }
        }
        
        // 3. LOOT ESP - Find weapons on ground
        snap.lootCount = 0;
        for (int i = 0; i < 1024 && snap.lootCount < 128; i++) {
            uintptr_t listEntry = g_mem.read<uintptr_t>(entityList + (8 * (i & 0x7FFF) >> 9) + 16);
            if (!listEntry) continue;
            
            uintptr_t entity = g_mem.read<uintptr_t>(listEntry + 112 * (i & 0x1FF));
            if (!entity) continue;
            
            // Check if entity has weapon/item attributes
            uintptr_t attrManager = g_mem.read<uintptr_t>(entity + offsets::m_AttributeManager);
            if (!attrManager) continue;
            
            uintptr_t item = g_mem.read<uintptr_t>(attrManager + offsets::m_Item);
            if (!item) continue;
            
            int weaponID = g_mem.read<int>(item + offsets::m_iItemDefinitionIndex);
            
            // Filter valuable weapons
            if (isValuableWeapon(weaponID)) {
                // Get position
                uintptr_t sceneNode = g_mem.read<uintptr_t>(entity + offsets::m_pGameSceneNode);
                if (sceneNode) {
                    auto& loot = snap.loot[snap.lootCount];
                    loot.position = g_mem.read<Vec3>(sceneNode + offsets::m_vecAbsOrigin);
                    loot.weaponID = weaponID;
                    const char* weaponName = getWeaponName(weaponID);
                    strncpy_s(loot.name, sizeof(loot.name), weaponName, _TRUNCATE);
                    loot.valid = true;
                    snap.lootCount++;
                }
            }
        }
        
        // 4. SOUND ESP - Generate sound events from game state
        // Track player movements and weapon fire
        static std::map<size_t, Vec3> lastPositions; // Track last known positions (by player index)
        static std::map<size_t, std::chrono::steady_clock::time_point> lastFootstepTime;
        static std::map<size_t, int> lastAmmoCount; // Track ammo for gunshot detection
        
        auto now = std::chrono::steady_clock::now();
        
        // Keep old sounds and add new ones
        static std::vector<DrawSnapshot::SoundEvent> persistentSounds;
        
        // Remove expired sounds
        persistentSounds.erase(
            std::remove_if(persistentSounds.begin(), persistentSounds.end(),
                [&now](const DrawSnapshot::SoundEvent& s) {
                    auto elapsed = std::chrono::duration<float>(now - s.timestamp).count();
                    return elapsed >= s.fadeTime;
                }),
            persistentSounds.end()
        );
        
        // Detect new sound events
        for (size_t i = 0; i < snap.playerCount; i++) {
            const auto& p = snap.players[i];
            if (!p.valid) continue;
            
            // FOOTSTEPS - Detect movement
            auto it = lastPositions.find(i);
            if (it != lastPositions.end()) {
                float distMoved = sqrtf(
                    powf(p.origin.x - it->second.x, 2) +
                    powf(p.origin.y - it->second.y, 2) +
                    powf(p.origin.z - it->second.z, 2)
                );
                
                // If moved > 0.5m and enough time passed since last footstep
                auto lastFootstep = lastFootstepTime.find(i);
                bool canPlayFootstep = true;
                if (lastFootstep != lastFootstepTime.end()) {
                    auto timeSince = std::chrono::duration<float>(now - lastFootstep->second).count();
                    canPlayFootstep = (timeSince > 0.3f); // Max 3 footsteps per second
                }
                
                if (distMoved > 0.5f && canPlayFootstep) {
                    DrawSnapshot::SoundEvent sound;
                    sound.position = p.origin;
                    sound.type = "Footstep";
                    sound.fadeTime = 2.0f;
                    sound.timestamp = now;
                    persistentSounds.push_back(sound);
                    
                    lastFootstepTime[i] = now;
                }
            }
            
            // GUNSHOTS - Detect weapon fire (by reading ammo count changes)
            // Note: In real implementation, you'd read weapon ammo from memory
            // For now, we'll simulate based on player state changes
            // This is a simplified version - real implementation would read m_iClip1, m_iAmmo, etc.
            
            lastPositions[i] = p.origin;
        }
        
        // Copy persistent sounds to snapshot
        snap.sounds = persistentSounds;
        
        // 4. Swap buffers (Publish snapshot)
        {
            std::lock_guard<std::mutex> lock(g_snapshotMutex);
            g_snapshotFront = g_snapshotBack;
        }
        
        // Smart Sleep
        // std::this_thread::yield(); 
        // Sleep(1); 
    }
}


// ============================================
// World to Screen (SIMD Optimized)
// ============================================
__forceinline float simd_dot(__m128 a, __m128 b) {
    __m128 res = _mm_mul_ps(a, b);
    __m128 shuf = _mm_shuffle_ps(res, res, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 sums = _mm_add_ps(res, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ps(sums, shuf);
    return _mm_cvtss_f32(sums);
}

bool WorldToScreen(const ViewMatrix& m, const Vec3& pos, Vec3& out) {
    // Load vector (x, y, z, 1.0f)
    __m128 v_pos = _mm_set_ps(1.f, pos.z, pos.y, pos.x);
    
    // Calculate W first (for early exit)
    float clip_w = simd_dot(_mm_loadu_ps(m[3]), v_pos);
    if (clip_w < 0.001f) return false;
    
    // Calculate X and Y
    float clip_x = simd_dot(_mm_loadu_ps(m[0]), v_pos);
    float clip_y = simd_dot(_mm_loadu_ps(m[1]), v_pos);
    
    float inv_w = 1.f / clip_w;
    
    out.x = (clip_x * inv_w + 1.f) * 0.5f * g_gameBounds.right;
    out.y = (1.f - clip_y * inv_w) * 0.5f * g_gameBounds.bottom;
    out.z = clip_w;
    
    return true;
}

// ====================================
// HELPER FUNCTIONS FOR ADVANCED ESP
// ====================================

// Get weapon name by ID
const char* getWeaponName(int weaponID) {
    switch (weaponID) {
        case 1: return "Deagle";
        case 2: return "Dual Berettas";
        case 3: return "Five-Seven";
        case 4: return "Glock-18";
        case 7: return "AK-47";
        case 8: return "AUG";
        case 9: return "AWP";
        case 10: return "FAMAS";
        case 11: return "G3SG1";
        case 13: return "Galil AR";
        case 14: return "M249";
        case 16: return "M4A4";
        case 17: return "MAC-10";
        case 19: return "P90";
        case 23: return "MP5-SD";
        case 24: return "UMP-45";
        case 25: return "XM1014";
        case 26: return "PP-Bizon";
        case 27: return "MAG-7";
        case 28: return "Negev";
        case 29: return "Sawed-Off";
        case 30: return "Tec-9";
        case 31: return "Zeus x27";
        case 32: return "P2000";
        case 33: return "MP7";
        case 34: return "MP9";
        case 35: return "Nova";
        case 36: return "P250";
        case 38: return "SCAR-20";
        case 39: return "SG 553";
        case 40: return "SSG 08";
        case 60: return "M4A1-S";
        case 61: return "USP-S";
        case 63: return "CZ75-Auto";
        case 64: return "R8 Revolver";
        case 500: return "Bayonet";
        case 503: return "Knife";
        case 505: return "Flip Knife";
        case 506: return "Gut Knife";
        case 507: return "Karambit";
        case 508: return "M9 Bayonet";
        case 509: return "Huntsman Knife";
        case 515: return "Butterfly Knife";
        case 516: return "Shadow Daggers";
        case 42: return "C4";
        case 43: return "Flashbang";
        case 44: return "HE Grenade";
        case 45: return "Smoke";
        case 46: return "Molotov";
        case 47: return "Decoy";
        case 48: return "Incendiary";
        default: return "Unknown";
    }
}

// Check if weapon is valuable for Loot ESP
bool isValuableWeapon(int weaponID) {
    // Rifles, snipers, expensive pistols
    return weaponID == 7 || weaponID == 8 || weaponID == 9 || weaponID == 10 || 
           weaponID == 11 || weaponID == 13 || weaponID == 16 || weaponID == 38 || 
           weaponID == 39 || weaponID == 40 || weaponID == 60 || weaponID == 1;
}

// ============================================
// Render ESP - 100% ASYNC CONSUMER
// Only draws data from the latest snapshot
// NO memory reads here!
// ============================================
void renderESP() {
    if (!g_cfg.espEnabled) return;
    if (!g_mem.ok()) return;
    
    // Update game bounds
    GetClientRect(g_mem.gameHwnd, &g_gameBounds);
    if (g_gameBounds.right == 0) return;
    
    // 1. Get latest snapshot (Atomic swap)
    DrawSnapshot snap;
    {
        std::lock_guard<std::mutex> lock(g_snapshotMutex);
        snap = g_snapshotFront;
    }
    
    if (snap.playerCount == 0) return;
    
    auto* draw = ImGui::GetBackgroundDrawList();
    
    // W2S lambda
    auto toScreen = [&](const Vec3& v) -> std::optional<Vec3> {
        Vec3 out;
        if (WorldToScreen(snap.matrix, v, out)) return out;
        return std::nullopt;
    };
    
    // 2. Render Loop (Pure math & drawing)
    for (size_t i = 0; i < snap.playerCount; i++) {
        const auto& p = snap.players[i];
        if (!p.valid) continue;
        
        // Distance filter
        float dist = (p.origin - snap.localOrigin).length() / 100.f;
        if (g_cfg.espMaxDistance > 0 && dist > g_cfg.espMaxDistance) continue;
        
        // Base box
        auto screenFeetOpt = toScreen(p.origin);
        auto screenHeadOpt = toScreen(p.headPos);
        if (!screenFeetOpt || !screenHeadOpt) continue;
        
        Vec3 screenFeet = *screenFeetOpt;
        Vec3 screenHead = *screenHeadOpt;
        
        float h = screenFeet.y - screenHead.y;
        float w = h * 0.4f;
        float x = screenHead.x - w / 2.f;
        float y = screenHead.y;
        
        // Bones Adjustment
        if (p.bonesValid) {
            auto top2D = toScreen({p.boneHead.x, p.boneHead.y, p.boneHead.z + 8.f});
            auto bottom2D = toScreen(p.bonePelvis); // Approximation
            if (top2D && bottom2D) {
                float hBones = screenFeet.y - top2D->y; // Use feet Y for better bottom
                float wBones = hBones * 0.4f;
                if (hBones > 0) {
                    h = hBones;
                    w = wBones;
                    x = top2D->x - w / 2.f;
                    y = top2D->y;
                }
            }
        }
        
        if (h <= 0 || w <= 0) continue;
        
        // Color
        ImU32 col = p.isEnemy ? 
            ImGui::ColorConvertFloat4ToU32({g_cfg.colorEnemy[0], g_cfg.colorEnemy[1], g_cfg.colorEnemy[2], 1.f}) :
            ImGui::ColorConvertFloat4ToU32({g_cfg.colorTeam[0], g_cfg.colorTeam[1], g_cfg.colorTeam[2], 1.f});
        
        // Fill
        if (g_cfg.espFill) {
            uint8_t alpha = static_cast<uint8_t>(g_cfg.espFillOpacity * 2.55f);
            ImU32 fillCol = (col & 0x00FFFFFF) | (static_cast<ImU32>(alpha) << 24);
            draw->AddRectFilled({x, y}, {x + w, y + h}, fillCol);
        }
        
        // Box
        if (g_cfg.espBox) {
            if (g_cfg.espCorneredBox) {
                esp::drawCorneredBox(draw, x, y, w, h, col);
            } else {
                draw->AddRect({x - 1, y - 1}, {x + w + 1, y + h + 1}, IM_COL32(0, 0, 0, 200));
                draw->AddRect({x, y}, {x + w, y + h}, col, 0, 0, 2.f);
            }
        }
        
        // Skeleton
        if (g_cfg.espSkeleton && dist < 50.f && p.bonesValid) {
            auto boneToScreen = [&](const Vec3& b) -> std::optional<ImVec2> {
                if (b.x == 0 && b.y == 0 && b.z == 0) return std::nullopt;
                auto s = toScreen(b);
                if (!s) return std::nullopt;
                return ImVec2(s->x, s->y);
            };
            
            auto drawLine = [&](const Vec3& a, const Vec3& b, ImU32 c) {
                auto pa = boneToScreen(a);
                auto pb = boneToScreen(b);
                if (pa && pb) draw->AddLine(*pa, *pb, c, 1.5f);
            };
            
            ImU32 skelCol = ImGui::ColorConvertFloat4ToU32({
                g_cfg.colorSkeleton[0], g_cfg.colorSkeleton[1], 
                g_cfg.colorSkeleton[2], g_cfg.colorSkeleton[3]
            });
            
            // Spine
            drawLine(p.boneHead, p.boneNeck, skelCol);
            drawLine(p.boneNeck, p.boneSpine1, skelCol);
            drawLine(p.boneSpine1, p.boneSpine2, skelCol);
            drawLine(p.boneSpine2, p.bonePelvis, skelCol);
            
            // Arms
            drawLine(p.boneSpine1, p.boneArmL[0], skelCol);
            drawLine(p.boneArmL[0], p.boneArmL[1], skelCol);
            drawLine(p.boneArmL[1], p.boneArmL[2], skelCol);
            
            drawLine(p.boneSpine1, p.boneArmR[0], skelCol);
            drawLine(p.boneArmR[0], p.boneArmR[1], skelCol);
            drawLine(p.boneArmR[1], p.boneArmR[2], skelCol);
            
            // Legs
            drawLine(p.bonePelvis, p.boneLegL[0], skelCol);
            drawLine(p.boneLegL[0], p.boneLegL[1], skelCol);
            drawLine(p.boneLegL[1], p.boneLegL[2], skelCol);
            
            drawLine(p.bonePelvis, p.boneLegR[0], skelCol);
            drawLine(p.boneLegR[0], p.boneLegR[1], skelCol);
            drawLine(p.boneLegR[1], p.boneLegR[2], skelCol);
        }

        // Head Dot
        if (g_cfg.espHeadDot && dist < 50.f && p.bonesValid) {
            auto head2D = toScreen({p.boneHead.x, p.boneHead.y, p.boneHead.z});
            if (head2D) {
                ImU32 headCol = ImGui::ColorConvertFloat4ToU32({
                    g_cfg.colorHeadDot[0], g_cfg.colorHeadDot[1], 
                    g_cfg.colorHeadDot[2], g_cfg.colorHeadDot[3]
                });
                // Add filled circle for head dot
                draw->AddCircleFilled(ImVec2(head2D->x, head2D->y), g_cfg.espHeadDotSize, headCol);
                // Optional: Outline for better visibility
                draw->AddCircle(ImVec2(head2D->x, head2D->y), g_cfg.espHeadDotSize + 0.5f, IM_COL32(0,0,0,255), 0, 1.0f);
            }
        }
        
        // Health bar
        if (g_cfg.espHealth) {
            esp::drawHealthBar(draw, x, y, w, h, p.health);
        }
        
        // Armor bar (Fixed)
        if (g_cfg.espArmor && p.armor > 0) {
            esp::drawArmorBar(draw, x, y, w, h, p.armor);
        }
        
        // Name
        if (g_cfg.espName && p.name[0] != '\0') {
            auto sz = ImGui::CalcTextSize(p.name);
            ImVec2 namePos = {x + w / 2 - sz.x / 2, y - sz.y - 4};
            if (g_cfg.espOutlinedText) {
                esp::drawOutlinedText(draw, namePos, IM_COL32(255, 255, 255, 255), p.name);
            } else {
                draw->AddText(namePos, IM_COL32(255, 255, 255, 255), p.name);
            }
        }
        
        // Distance
        float textOffsetY = 0;
        if (g_cfg.espDistance) {
            auto txt = std::format("{:.0f}m", dist);
            auto sz = ImGui::CalcTextSize(txt.c_str());
            ImVec2 distPos = {x + w / 2 - sz.x / 2, y + h + 4};
            if (g_cfg.espOutlinedText) {
                esp::drawOutlinedText(draw, distPos, IM_COL32(255, 255, 255, 200), txt.c_str());
            } else {
                draw->AddText(distPos, IM_COL32(255, 255, 255, 200), txt.c_str());
            }
            textOffsetY = sz.y + 4;
        }
        
        // Snaplines
        if (g_cfg.espSnaplines) {
            float snapY = y + h + textOffsetY;
            draw->AddLine(
                ImVec2((float)g_gameBounds.right / 2.f, (float)g_gameBounds.bottom), 
                ImVec2(x + w / 2.f, snapY), 
                col, 1.f
            );
        }
    }

    // Draw Crosshair
    if (g_cfg.espSniperCrosshair) {
        float cx = g_gameBounds.right / 2.f;
        float cy = g_gameBounds.bottom / 2.f;
        draw->AddLine({cx - 8, cy}, {cx + 8, cy}, IM_COL32(255, 0, 0, 255), 1.5f);
        draw->AddLine({cx, cy - 8}, {cx, cy + 8}, IM_COL32(255, 0, 0, 255), 1.5f);
    }

    // ====================================
    // NEW ADVANCED ESP FEATURES
    // ====================================
    
    // 1. RADAR HACK - Mini radar in corner
    if (esp_menu::g_config.radarHack) {
        const float radarSize = 150.f;
        const float radarX = g_gameBounds.right - radarSize - 20.f;
        const float radarY = 20.f;
        const float radarCenterX = radarX + radarSize / 2.f;
        const float radarCenterY = radarY + radarSize / 2.f;
        const float radarScale = 5.f; // meters per pixel
        
        // Background
        draw->AddRectFilled(
            ImVec2(radarX, radarY), 
            ImVec2(radarX + radarSize, radarY + radarSize),
            IM_COL32(20, 20, 30, 200)
        );
        draw->AddRect(
            ImVec2(radarX, radarY), 
            ImVec2(radarX + radarSize, radarY + radarSize),
            IM_COL32(100, 100, 255, 255), 1.5f
        );
        
        // Center dot (local player)
        draw->AddCircleFilled(
            ImVec2(radarCenterX, radarCenterY), 
            4.f, 
            IM_COL32(0, 255, 0, 255)
        );
        
        // Draw all players on radar
        for (size_t i = 0; i < snap.playerCount; i++) {
            const auto& p = snap.players[i];
            if (!p.valid) continue;
            
            // Calculate relative position to local player
            float dx = p.origin.x - snap.localOrigin.x;
            float dy = p.origin.y - snap.localOrigin.y;
            
            // Convert to radar coordinates
            float dotX = radarCenterX + (dx / radarScale);
            float dotY = radarCenterY - (dy / radarScale);
            
            // Clamp to radar bounds
            dotX = std::clamp(dotX, radarX + 3.f, radarX + radarSize - 3.f);
            dotY = std::clamp(dotY, radarY + 3.f, radarY + radarSize - 3.f);
            
            // Color based on team
            ImU32 dotColor = (p.isEnemy) ? 
                IM_COL32(255, 0, 0, 255) :     // Red for enemies
                IM_COL32(0, 150, 255, 255);   // Blue for teammates
            
            draw->AddCircleFilled(ImVec2(dotX, dotY), 3.f, dotColor);
        }
        
        // Label
        draw->AddText(ImVec2(radarX + 5, radarY + 5), IM_COL32(255, 255, 255, 200), "RADAR");
    }
    
    // 2. SPECTATOR LIST - Who's watching you
    if (esp_menu::g_config.spectatorList && snap.spectatorCount > 0) {
        const float listX = 20.f;
        float listY = g_gameBounds.bottom - 200.f;
        
        // Background
        float bgHeight = 30.f + snap.spectatorCount * 18.f;
        draw->AddRectFilled(
            ImVec2(listX, listY), 
            ImVec2(listX + 200.f, listY + bgHeight),
            IM_COL32(20, 20, 30, 220)
        );
        draw->AddRect(
            ImVec2(listX, listY), 
            ImVec2(listX + 200.f, listY + bgHeight),
            IM_COL32(255, 100, 100, 255), 1.5f
        );
        
        // Title
        draw->AddText(ImVec2(listX + 5, listY + 5), IM_COL32(255, 100, 100, 255), "SPECTATORS");
        
        // List spectators
        listY += 20.f;
        for (size_t i = 0; i < snap.spectatorCount; i++) {
            const auto& spec = snap.spectators[i];
            if (spec.valid && spec.name[0] != '\0') {
                draw->AddText(ImVec2(listX + 10, listY), IM_COL32(255, 255, 255, 255), spec.name);
                listY += 18.f;
            }
        }
    }
    
    // 3. BOMB TIMER - Show C4 countdown
    if (esp_menu::g_config.bombTimer && snap.bomb.planted && snap.bomb.timeRemaining > 0.f) {
        const float timerX = g_gameBounds.right / 2.f - 100.f;
        const float timerY = 100.f;
        
        // Background
        draw->AddRectFilled(
            ImVec2(timerX, timerY), 
            ImVec2(timerX + 200.f, timerY + 60.f),
            IM_COL32(30, 10, 10, 230)
        );
        draw->AddRect(
            ImVec2(timerX, timerY), 
            ImVec2(timerX + 200.f, timerY + 60.f),
            IM_COL32(255, 0, 0, 255), 2.f
        );
        
        // Title
        draw->AddText(ImVec2(timerX + 70, timerY + 5), IM_COL32(255, 50, 50, 255), "BOMB");
        
        // Time
        auto timeStr = std::format("{:.1f}s", snap.bomb.timeRemaining);
        auto sz = ImGui::CalcTextSize(timeStr.c_str());
        draw->AddText(
            ImVec2(timerX + 100.f - sz.x / 2, timerY + 28), 
            (snap.bomb.timeRemaining < 10.f) ? IM_COL32(255, 0, 0, 255) : IM_COL32(255, 200, 0, 255), 
            timeStr.c_str()
        );
        
        // Progress bar (40 seconds total)
        float progress = snap.bomb.timeRemaining / 40.f;
        draw->AddRectFilled(
            ImVec2(timerX + 10, timerY + 48), 
            ImVec2(timerX + 10 + 180.f * progress, timerY + 54),
            (snap.bomb.timeRemaining < 10.f) ? IM_COL32(255, 0, 0, 200) : IM_COL32(255, 150, 0, 200)
        );
    }
    
    // 4. LOOT ESP - Show valuable weapons on ground
    if (esp_menu::g_config.lootESP) {
        ImU32 lootColor = ImGui::ColorConvertFloat4ToU32({
            esp_menu::g_config.lootColor[0], 
            esp_menu::g_config.lootColor[1], 
            esp_menu::g_config.lootColor[2], 
            esp_menu::g_config.lootColor[3]
        });
        
        for (size_t i = 0; i < snap.lootCount; i++) {
            const auto& item = snap.loot[i];
            if (!item.valid) continue;
            
            auto screen = toScreen(item.position);
            if (!screen) continue;
            
            // Draw icon
            draw->AddCircleFilled(ImVec2(screen->x, screen->y), 8.f, lootColor);
            draw->AddCircle(ImVec2(screen->x, screen->y), 8.f, IM_COL32(0, 0, 0, 255), 0, 1.5f);
            
            // Draw weapon name
            auto sz = ImGui::CalcTextSize(item.name);
            esp::drawOutlinedText(
                draw, 
                ImVec2(screen->x - sz.x / 2, screen->y + 12), 
                lootColor, 
                item.name
            );
        }
    }
    
    // 5. SOUND ESP - Visualize sound events (footsteps, gunshots)
    if (esp_menu::g_config.soundESP) {
        auto now = std::chrono::steady_clock::now();
        
        // Render and fade out sound events
        for (auto& sound : snap.sounds) {
            // Calculate fade time remaining
            auto elapsed = std::chrono::duration<float>(now - sound.timestamp).count();
            float timeRemaining = sound.fadeTime - elapsed;
            
            if (timeRemaining <= 0.f) continue; // Skip expired sounds
            
            auto screen = toScreen(sound.position);
            if (!screen) continue;
            
            float alpha = std::clamp(timeRemaining / sound.fadeTime, 0.f, 1.f);
            ImU32 soundColor = IM_COL32(255, 255, 0, (int)(255 * alpha));
            
            // Draw sound wave circles (expanding effect)
            float waveSize = 20.f + (1.f - alpha) * 30.f; // 20-50 pixels
            draw->AddCircle(ImVec2(screen->x, screen->y), waveSize, soundColor, 0, 2.f);
            draw->AddCircle(ImVec2(screen->x, screen->y), waveSize * 1.5f, soundColor, 0, 1.5f);
            
            // Draw sound type
            if (sound.type) {
                esp::drawOutlinedText(
                    draw, 
                    ImVec2(screen->x - 30, screen->y - 10), 
                    soundColor, 
                    sound.type
                );
            }
        }
    }
}


// ============================================
// Config System
// ============================================
void SaveConfig(const char* filename) {
    std::ofstream out(filename, std::ios::binary);
    if (out.is_open()) {
        out.write(reinterpret_cast<const char*>(&g_cfg), sizeof(Config));
        out.close();
    }
}

void LoadConfig(const char* filename) {
    std::ifstream in(filename, std::ios::binary);
    if (in.is_open()) {
        in.read(reinterpret_cast<char*>(&g_cfg), sizeof(Config));
        in.close();
    }
}

// ============================================
// Menu (NEW: Using modern esp_menu.hpp)
// ============================================

// Sync old Config with new ESP menu settings
void SyncMenuSettings() {
    // Read from esp_menu to old config
    g_cfg.espEnabled = esp_menu::g_settings.boxESP || esp_menu::g_settings.nameESP || esp_menu::g_settings.healthBar;
    g_cfg.espBox = esp_menu::g_settings.boxESP;
    g_cfg.espName = esp_menu::g_settings.nameESP;
    g_cfg.espHealth = esp_menu::g_settings.healthBar;
    g_cfg.espDistance = esp_menu::g_settings.distanceESP;
    g_cfg.espSkeleton = esp_menu::g_settings.skeletonESP;
    g_cfg.espSnaplines = esp_menu::g_settings.snaplines;
    g_cfg.espMaxDistance = esp_menu::g_settings.maxDistance;
    
    // Sync colors
    for (int i = 0; i < 4; i++) {
        g_cfg.colorEnemy[i] = esp_menu::g_settings.enemyColor[i];
        g_cfg.colorTeam[i] = esp_menu::g_settings.teamColor[i];
    }
}

void renderMenu() {
    // Handle hotkeys
    esp_menu::HandleHotkeys();
    
    // Use new modern ESP menu
    esp_menu::RenderMenu();
    
    // Sync settings after rendering
    SyncMenuSettings();
    
    // Update g_cfg.showMenu from esp_menu
    g_cfg.showMenu = esp_menu::g_menuOpen;
}

// ============================================
// Window Proc - ALWAYS click-through when menu is hidden
// ============================================
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // ===== CRITICAL: Complete click-through when menu hidden =====
    if (!g_cfg.showMenu) {
        switch (msg) {
            case WM_NCHITTEST:
                return HTTRANSPARENT;
            case WM_MOUSEACTIVATE:
                return MA_NOACTIVATEANDEAT;
            case WM_ACTIVATE:
            case WM_ACTIVATEAPP:
                return 0;
            case WM_SETFOCUS:
                return 0;
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MOUSEMOVE:
            case WM_MOUSEWHEEL:
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_CHAR:
                return 0; // Ignore all input
        }
    }
    
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    if (msg == WM_DESTROY) { g_running = false; PostQuitMessage(0); return 0; }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================
// Create Overlay with DirectComposition (bypass DWM 60fps)
// Falls back to classic overlay if DComp fails
// ============================================
bool createOverlay() {
    if (!g_mem.gameHwnd) {
        g_mem.gameHwnd = FindWindowW(nullptr, L"Counter-Strike 2");
    }
    if (!g_mem.gameHwnd) return false;
    
    GetClientRect(g_mem.gameHwnd, &g_gameBounds);
    
    RECT wr;
    GetWindowRect(g_mem.gameHwnd, &wr);
    
    WNDCLASSEXW wc{sizeof(wc), CS_CLASSDC, WndProc, 0, 0, GetModuleHandle(0), 0, 0, 0, 0, L"Overlay", 0};
    RegisterClassExW(&wc);
    
    // Create window - WS_EX_NOREDIRECTIONBITMAP for DirectComposition
    // WS_EX_LAYERED + WS_EX_TRANSPARENT for proper click-through
    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP,
        L"Overlay", L"", WS_POPUP, 
        wr.left, wr.top, g_gameBounds.right, g_gameBounds.bottom, 
        0, 0, wc.hInstance, 0);
    if (!g_hwnd) return false;
    
    // Set layered window attributes for proper transparency behavior
    SetLayeredWindowAttributes(g_hwnd, 0, 255, LWA_ALPHA);
    
    // Create D3D11 device first
    D3D_FEATURE_LEVEL fl;
    D3D_FEATURE_LEVEL fls[] = {D3D_FEATURE_LEVEL_11_0};
    
    // Add debug flag for dev, but Remove for release for speed
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    
    if (FAILED(D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, 
        creationFlags, fls, 1, D3D11_SDK_VERSION, 
        &g_dev, &fl, &g_ctx))) {
        return false;
    }
    
    // High GPU Priority (Undocumented trick for overlay smoothness)
    // We query ID3D11DeviceContext1 to discard view if possible, but mainly we want GPU focus
    IDXGIDevice* dxgiDevice = nullptr;
    if (SUCCEEDED(g_dev->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice))) {
        dxgiDevice->SetGPUThreadPriority(7); // Max priority
        dxgiDevice->Release();
    }
    
    // Get DXGI adapter and factory
    IDXGIAdapter* adapter = nullptr;
    dxgiDevice->GetAdapter(&adapter);
    
    IDXGIFactory2* factory = nullptr;
    adapter->GetParent(__uuidof(IDXGIFactory2), (void**)&factory);
    
    // Try DirectComposition first (unlimited FPS!)
    bool dcompSuccess = false;
    
    if (SUCCEEDED(DCompositionCreateDevice(dxgiDevice, __uuidof(IDCompositionDevice), (void**)&g_dcompDevice))) {
        // Create swap chain for composition (FLIP_SEQUENTIAL with alpha!)
        DXGI_SWAP_CHAIN_DESC1 sd1{};
        sd1.Width = g_gameBounds.right;
        sd1.Height = g_gameBounds.bottom;
        sd1.Format = DXGI_FORMAT_B8G8R8A8_UNORM;  // BGRA for alpha
        sd1.Stereo = FALSE;
        sd1.SampleDesc.Count = 1;
        sd1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd1.BufferCount = 2;
        sd1.Scaling = DXGI_SCALING_STRETCH;
        sd1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;  // FLIP for no vsync limit!
        sd1.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;      // Per-pixel alpha!
        sd1.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT; // Reduces input lag significantly!
        
        if (SUCCEEDED(factory->CreateSwapChainForComposition(g_dev, &sd1, nullptr, &g_swap))) {
            // Set max frame latency to 1 for lowest lag
            IDXGISwapChain2* sc2 = nullptr;
            if (SUCCEEDED(g_swap->QueryInterface(__uuidof(IDXGISwapChain2), (void**)&sc2))) {
                sc2->SetMaximumFrameLatency(1);
                sc2->Release();
            }
            // Create composition target
            if (SUCCEEDED(g_dcompDevice->CreateTargetForHwnd(g_hwnd, TRUE, &g_dcompTarget))) {
                // Create visual
                if (SUCCEEDED(g_dcompDevice->CreateVisual(&g_dcompVisual))) {
                    g_dcompVisual->SetContent(g_swap);
                    g_dcompTarget->SetRoot(g_dcompVisual);
                    g_dcompDevice->Commit();
                    g_useDirectComposition = true;
                    dcompSuccess = true;
                }
            }
        }
    }
    
    // Fallback to classic overlay if DirectComposition failed
    if (!dcompSuccess) {
        // Clean up partial DComp state
        if (g_dcompVisual) { g_dcompVisual->Release(); g_dcompVisual = nullptr; }
        if (g_dcompTarget) { g_dcompTarget->Release(); g_dcompTarget = nullptr; }
        if (g_dcompDevice) { g_dcompDevice->Release(); g_dcompDevice = nullptr; }
        if (g_swap) { g_swap->Release(); g_swap = nullptr; }
        
        // Recreate window without NOREDIRECTIONBITMAP
        DestroyWindow(g_hwnd);
        g_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            L"Overlay", L"", WS_POPUP, 
            wr.left, wr.top, g_gameBounds.right, g_gameBounds.bottom, 
            0, 0, wc.hInstance, 0);
        if (!g_hwnd) {
            factory->Release();
            adapter->Release();
            dxgiDevice->Release();
            return false;
        }
        
        SetLayeredWindowAttributes(g_hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
        MARGINS m{-1, -1, -1, -1}; 
        DwmExtendFrameIntoClientArea(g_hwnd, &m);
        
        // Classic swap chain
        DXGI_SWAP_CHAIN_DESC1 sd1{};
        sd1.Width = g_gameBounds.right;
        sd1.Height = g_gameBounds.bottom;
        sd1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd1.SampleDesc.Count = 1;
        sd1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd1.BufferCount = 1;
        sd1.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        
        if (FAILED(factory->CreateSwapChainForHwnd(g_dev, g_hwnd, &sd1, nullptr, nullptr, &g_swap))) {
            factory->Release();
            adapter->Release();
            dxgiDevice->Release();
            return false;
        }
        
        g_useDirectComposition = false;
    }
    
    factory->Release();
    adapter->Release();
    dxgiDevice->Release();
    
    // Create render target view
    ID3D11Texture2D* buf = nullptr;
    g_swap->GetBuffer(0, IID_PPV_ARGS(&buf));
    g_dev->CreateRenderTargetView(buf, 0, &g_rtv);
    buf->Release();
    
    ShowWindow(g_hwnd, SW_SHOW);
    return true;
}

// ============================================
// Main
// ============================================
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    // =========================================
    // HIGH PRIORITY: Set process and thread priority for minimal latency
    // =========================================
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    timeBeginPeriod(1); // Set system timer resolution to 1ms
    
    // Auto-Load Config
    LoadConfig("config.bin");
    
    // Attach to CS2
    if (!g_mem.attach()) {
        MessageBoxW(0, L"CS2 not found! Start the game first.", L"EXTERNAL", MB_ICONERROR);
        return 1;
    }
    
    // Create overlay
    if (!createOverlay()) {
        MessageBoxW(0, L"Failed to create overlay!\nMake sure CS2 is in Windowed/Borderless mode.", L"EXTERNAL", MB_ICONERROR);
        return 1;
    }
    
    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    // ========================================
    // Custom Theme (hexaPjj style - Purple/Blue Neon)
    // ========================================
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // Rounding
    style.WindowRounding = 6.f;
    style.FrameRounding = 4.f;
    style.PopupRounding = 4.f;
    style.ScrollbarRounding = 4.f;
    style.GrabRounding = 3.f;
    style.TabRounding = 4.f;
    style.ChildRounding = 4.f;
    
    // Sizes
    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 6);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.WindowBorderSize = 1.f;
    style.FrameBorderSize = 0.f;
    style.ScrollbarSize = 12.f;
    
    // Colors - Dark Purple/Blue Theme
    colors[ImGuiCol_Text]                  = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.08f, 0.08f, 0.12f, 0.96f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.10f, 0.10f, 0.15f, 0.90f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.10f, 0.10f, 0.14f, 0.98f);
    colors[ImGuiCol_Border]                = ImVec4(0.35f, 0.25f, 0.55f, 0.60f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.15f, 0.14f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.22f, 0.20f, 0.32f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.28f, 0.25f, 0.40f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.12f, 0.10f, 0.18f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.20f, 0.15f, 0.30f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.10f, 0.08f, 0.15f, 0.80f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.12f, 0.11f, 0.18f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.08f, 0.08f, 0.12f, 0.80f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.35f, 0.30f, 0.50f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.45f, 0.38f, 0.60f, 0.90f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.55f, 0.45f, 0.70f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.55f, 0.40f, 0.90f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.50f, 0.38f, 0.80f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.62f, 0.50f, 0.95f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.28f, 0.22f, 0.45f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.38f, 0.30f, 0.58f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.48f, 0.38f, 0.70f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.28f, 0.22f, 0.42f, 0.90f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.38f, 0.30f, 0.55f, 0.95f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.45f, 0.35f, 0.65f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.35f, 0.28f, 0.50f, 0.50f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.45f, 0.35f, 0.65f, 0.70f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.55f, 0.42f, 0.78f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.40f, 0.30f, 0.60f, 0.40f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.50f, 0.40f, 0.75f, 0.70f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.60f, 0.48f, 0.88f, 1.00f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.20f, 0.16f, 0.32f, 0.90f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.38f, 0.30f, 0.55f, 1.00f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.32f, 0.25f, 0.48f, 1.00f);
    colors[ImGuiCol_PlotLines]             = ImVec4(0.55f, 0.45f, 0.85f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.55f, 0.40f, 0.90f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.40f, 0.32f, 0.65f, 0.50f);
    
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_dev, g_ctx);
    
    // Configure ImGui mouse based on initial menu state
    ImGuiIO& io = ImGui::GetIO();
    if (g_cfg.showMenu) {
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        io.MouseDrawCursor = true;
    } else {
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
        io.MouseDrawCursor = false;
    }
    
    // Start entity reading thread with HIGH PRIORITY
    std::thread t([]() {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        dataReaderLoop();
    });
    
    // Main loop
    MSG msg{};
    while (g_running) {
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) g_running = false;
        }
        if (!g_running) break;
        
        // Toggle menu with INSERT (handled by esp_menu::HandleHotkeys now)
        // Just sync the window style based on menu state
        static bool lastMenuState = esp_menu::g_menuOpen;
        if (esp_menu::g_menuOpen != lastMenuState) {
            lastMenuState = esp_menu::g_menuOpen;
            g_cfg.showMenu = esp_menu::g_menuOpen;
            
            // Update window style for click-through
            auto ex = GetWindowLongPtr(g_hwnd, GWL_EXSTYLE);
            if (esp_menu::g_menuOpen) {
                // Menu visible - accept input (remove WS_EX_TRANSPARENT)
                SetWindowLongPtr(g_hwnd, GWL_EXSTYLE, ex & ~WS_EX_TRANSPARENT);
            } else {
                // Menu hidden - click-through (add WS_EX_TRANSPARENT)
                SetWindowLongPtr(g_hwnd, GWL_EXSTYLE, ex | WS_EX_TRANSPARENT);
                // Return focus to game!
                if (g_mem.gameHwnd) {
                    SetForegroundWindow(g_mem.gameHwnd);
                    SetFocus(g_mem.gameHwnd);
                }
            }
            
            // Update ImGui mouse handling
            ImGuiIO& ioRef = ImGui::GetIO();
            if (esp_menu::g_menuOpen) {
                ioRef.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
                ioRef.MouseDrawCursor = true;
            } else {
                ioRef.ConfigFlags |= ImGuiConfigFlags_NoMouse;
                ioRef.MouseDrawCursor = false;
            }
        }
        
        // Exit with END
        if (GetAsyncKeyState(VK_END) & 1) break;
        
        // Auto-close when CS2 closes
        if (!IsWindow(g_mem.gameHwnd) || !FindWindowW(nullptr, L"Counter-Strike 2")) {
            // Hide overlay immediately to prevent "frozen" screen effect
            ShowWindow(g_hwnd, SW_HIDE);
            break;
        }
        
        // Sync overlay position with game window
        if (g_mem.gameHwnd) {
            RECT wr;
            GetWindowRect(g_mem.gameHwnd, &wr);
            GetClientRect(g_mem.gameHwnd, &g_gameBounds);
            SetWindowPos(g_hwnd, HWND_TOPMOST, wr.left, wr.top, 
                        g_gameBounds.right, g_gameBounds.bottom, SWP_NOACTIVATE);
        }
        
        // Render
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        
        renderESP();
        renderMenu();
        
        ImGui::Render();
        float clear[4] = {0, 0, 0, 0};
        g_ctx->OMSetRenderTargets(1, &g_rtv, 0);
        g_ctx->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        
        // Present: 0 = no vsync (unlimited FPS), 1 = vsync
        g_swap->Present(g_cfg.vsyncOverlay ? 1 : 0, 0);
        
        // Optional FPS limiter (0 = disabled = unlimited)
        if (g_cfg.fpsLimit > 0 && !g_cfg.vsyncOverlay) {
            static auto lastFrame = std::chrono::high_resolution_clock::now();
            auto now = std::chrono::high_resolution_clock::now();
            auto frameTime = std::chrono::duration<float, std::milli>(1000.f / g_cfg.fpsLimit);
            auto elapsed = now - lastFrame;
            if (elapsed < frameTime) {
                auto sleepTime = std::chrono::duration_cast<std::chrono::milliseconds>(frameTime - elapsed);
                if (sleepTime.count() > 0) Sleep((DWORD)sleepTime.count());
            }
            lastFrame = std::chrono::high_resolution_clock::now();
        }
    }
    
    g_running = false;
    if (t.joinable()) t.join();
    
    // Restore mouse cursor immediately
    ShowCursor(TRUE);
    
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    
    timeEndPeriod(1); // Restore system timer resolution
    
    // Release DirectComposition resources
    if (g_dcompVisual) g_dcompVisual->Release();
    if (g_dcompTarget) g_dcompTarget->Release();
    if (g_dcompDevice) g_dcompDevice->Release();
    
    if (g_rtv) g_rtv->Release();
    if (g_swap) g_swap->Release();
    if (g_ctx) g_ctx->Release();
    if (g_dev) g_dev->Release();
    DestroyWindow(g_hwnd);
    
    return 0;
}


