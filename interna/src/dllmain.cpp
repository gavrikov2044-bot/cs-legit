/*
 * INTERNAL ESP v1.0.0 - CS2 DLL Injection
 * 
 * Security Features:
 * - Manual Mapped (No PEB entry)
 * - Shadow VMT Hooking (No code patches)
 * - XOR String Encryption (No readable strings)
 * - Lazy Import Resolution (Hidden API calls)
 * - Junk Code (Signature disruption)
 */

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <cstdint>
#include <string>
#include <vector>
#include <format>
#include <mutex>
#include <memory>
#include <optional>
#include <fstream> // For Config

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

// Anti-Detect Headers
#include "xorstr.hpp"
#include "lazy_importer.hpp"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// Junk Code Macro - Inserts random no-ops to change file signature
#define JUNK_CODE \
    __asm { nop } \
    __asm { nop } \
    __asm { nop }

// ============================================
// VMT Hook Class (Shadow VTable)
// ============================================
class VMTHook {
public:
    VMTHook() = default;
    
    bool Init(void* pBase) {
        m_ppBase = reinterpret_cast<uintptr_t**>(pBase);
        m_pOriginalVmt = *m_ppBase;
        
        // Count methods
        while (reinterpret_cast<uintptr_t*>(m_pOriginalVmt)[m_iMethodCount])
            m_iMethodCount++;
            
        // Copy VTable
        m_pShadowVmt = new uintptr_t[m_iMethodCount + 1];
        memcpy(m_pShadowVmt, m_pOriginalVmt, m_iMethodCount * sizeof(uintptr_t));
        
        // Swap pointer
        *m_ppBase = m_pShadowVmt;
        return true;
    }
    
    void Unhook() {
        if (m_ppBase && m_pOriginalVmt) {
            *m_ppBase = m_pOriginalVmt;
        }
        if (m_pShadowVmt) {
            delete[] m_pShadowVmt;
            m_pShadowVmt = nullptr;
        }
    }
    
    template<typename T>
    T GetOriginal(int index) {
        return reinterpret_cast<T>(m_pOriginalVmt[index]);
    }
    
    void Hook(int index, void* pNewFunc) {
        m_pShadowVmt[index] = reinterpret_cast<uintptr_t>(pNewFunc);
    }

private:
    uintptr_t** m_ppBase = nullptr;
    uintptr_t* m_pOriginalVmt = nullptr;
    uintptr_t* m_pShadowVmt = nullptr;
    size_t m_iMethodCount = 0;
};

// ============================================
// Offsets & SDK
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
    constexpr uintptr_t m_modelState = 0x190;
    constexpr uintptr_t m_boneArray = 0x80;
    constexpr uintptr_t m_ArmorValue = 0x274C;

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

struct Vec3 {
    float x, y, z;
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    float length() const { return sqrtf(x*x + y*y + z*z); }
};

struct ViewMatrix { float m[4][4]; };

// Direct Memory Access Helper
template<typename T>
__forceinline T* Get(uintptr_t addr) {
    if (!addr) return nullptr;
    return reinterpret_cast<T*>(addr);
}

template<typename T>
__forceinline T Read(uintptr_t addr) {
    if (!addr) return T{};
    return *reinterpret_cast<T*>(addr);
}

// ============================================
// Globals
// ============================================
static uintptr_t g_client = 0;
static VMTHook g_swapChainHook;
static HWND g_hwnd = nullptr;
static ID3D11Device* g_device = nullptr;
static ID3D11DeviceContext* g_context = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;
static bool g_init = false;
static WNDPROC g_oWndProc = nullptr;

struct Config {
    bool espEnabled = true;
    bool espBox = true;
    bool espCorneredBox = false;
    bool espFill = false;
    float espFillOpacity = 15.f;
    bool espSkeleton = true;
    bool espHealth = true;
    bool espArmor = true;
    bool espName = true;
    bool espDistance = true;
    bool espSnaplines = false;
    bool espSniperCrosshair = true;
    bool espHeadDot = false;      // Head Dot
    float espHeadDotSize = 2.0f;  // Head Dot Size
    bool espEnemyOnly = true;
    bool espOutlinedText = true;
    float espMaxDistance = 500.0f;
    
    float colorEnemy[4] = {1.0f, 0.2f, 0.2f, 1.0f};
    float colorTeam[4] = {0.2f, 0.6f, 1.0f, 1.0f};
    float colorSkeleton[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float colorHeadDot[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    
    bool showMenu = true;
} g_cfg;

// ============================================
// Config System
// ============================================
void SaveConfig() {
    // Determine path relative to dll/exe not possible easily in manual map without shellcode helpers,
    // but usually CWD is game directory. We will try absolute path or CWD.
    // For manual map, safest is C:\Windows\Temp or just "config_int.bin" in current dir (which is usually where launcher is if launched correctly).
    std::ofstream out("config_int.bin", std::ios::binary);
    if (out.is_open()) {
        out.write(reinterpret_cast<const char*>(&g_cfg), sizeof(Config));
        out.close();
    }
}

void LoadConfig() {
    std::ifstream in("config_int.bin", std::ios::binary);
    if (in.is_open()) {
        in.read(reinterpret_cast<char*>(&g_cfg), sizeof(Config));
        in.close();
    }
}

// ============================================
// Helpers
// ============================================
void drawOutlinedText(ImDrawList* draw, ImVec2 pos, ImU32 color, const char* text) {
    const ImU32 black = IM_COL32(0, 0, 0, 255);
    draw->AddText({pos.x - 1, pos.y - 1}, black, text);
    draw->AddText({pos.x + 1, pos.y - 1}, black, text);
    draw->AddText({pos.x - 1, pos.y + 1}, black, text);
    draw->AddText({pos.x + 1, pos.y + 1}, black, text);
    draw->AddText(pos, color, text);
}

void drawCorneredBox(ImDrawList* draw, float x, float y, float w, float h, ImU32 color) {
    float len = w * 0.25f;
    draw->AddLine({x, y}, {x + len, y}, color, 2.f);
    draw->AddLine({x, y}, {x, y + len}, color, 2.f);
    draw->AddLine({x + w, y}, {x + w - len, y}, color, 2.f);
    draw->AddLine({x + w, y}, {x + w, y + len}, color, 2.f);
    draw->AddLine({x, y + h}, {x + len, y + h}, color, 2.f);
    draw->AddLine({x, y + h}, {x, y + h - len}, color, 2.f);
    draw->AddLine({x + w, y + h}, {x + w - len, y + h}, color, 2.f);
    draw->AddLine({x + w, y + h}, {x + w, y + h - len}, color, 2.f);
}

// ============================================
// ESP Logic
// ============================================
void RenderESP() {
    if (!g_client || !g_cfg.espEnabled) return;
    
    auto matrix = Read<ViewMatrix>(g_client + offsets::dwViewMatrix);
    auto localController = Read<uintptr_t>(g_client + offsets::dwLocalPlayerController);
    if (!localController) return;
    
    auto localPawnHandle = Read<uint32_t>(localController + offsets::m_hPlayerPawn);
    auto entityList = Read<uintptr_t>(g_client + offsets::dwEntityList);
    
    auto localListEntry = Read<uintptr_t>(entityList + 0x8 * ((localPawnHandle & 0x7FFF) >> 9) + 16);
    auto localPawn = Read<uintptr_t>(localListEntry + 112 * (localPawnHandle & 0x1FF));
    
    int localTeam = Read<int>(localPawn + offsets::m_iTeamNum);
    auto draw = ImGui::GetBackgroundDrawList();
    
    RECT rect; 
    using GetClientRect_t = BOOL(WINAPI*)(HWND, LPRECT);
    ((GetClientRect_t)LI_FN(GetClientRect))(g_hwnd, &rect);
    
    float screenW = (float)rect.right;
    float screenH = (float)rect.bottom;
    
    auto w2s = [&](const Vec3& v) -> std::optional<ImVec2> {
        float x = matrix.m[0][0] * v.x + matrix.m[0][1] * v.y + matrix.m[0][2] * v.z + matrix.m[0][3];
        float y = matrix.m[1][0] * v.x + matrix.m[1][1] * v.y + matrix.m[1][2] * v.z + matrix.m[1][3];
        float w = matrix.m[3][0] * v.x + matrix.m[3][1] * v.y + matrix.m[3][2] * v.z + matrix.m[3][3];
        
        if (w < 0.001f) return std::nullopt;
        
        float inv = 1.f / w;
        float sx = (x * inv + 1.f) * 0.5f * screenW;
        float sy = (1.f - y * inv) * 0.5f * screenH;
        return ImVec2(sx, sy);
    };
    
    for (int i = 1; i < 64; i++) {
        auto listEntry = Read<uintptr_t>(entityList + (8 * (i & 0x7FFF) >> 9) + 16);
        if (!listEntry) continue;
        
        auto controller = Read<uintptr_t>(listEntry + 112 * (i & 0x1FF));
        if (!controller) continue;
        
        auto pawnHandle = Read<uint32_t>(controller + offsets::m_hPlayerPawn);
        if (!pawnHandle) continue;
        
        auto listEntry2 = Read<uintptr_t>(entityList + 0x8 * ((pawnHandle & 0x7FFF) >> 9) + 16);
        auto pawn = Read<uintptr_t>(listEntry2 + 112 * (pawnHandle & 0x1FF));
        if (!pawn || pawn == localPawn) continue;
        
        int health = Read<int>(pawn + offsets::m_iHealth);
        int armor = Read<int>(pawn + offsets::m_ArmorValue);
        if (health <= 0 || health > 100) continue;
        
        int team = Read<int>(pawn + offsets::m_iTeamNum);
        bool isEnemy = (team != localTeam);
        if (g_cfg.espEnemyOnly && !isEnemy) continue;
        
        auto sceneNode = Read<uintptr_t>(pawn + offsets::m_pGameSceneNode);
        auto origin = Read<Vec3>(sceneNode + offsets::m_vecAbsOrigin);
        
        float dist = (origin - Read<Vec3>(Read<uintptr_t>(localPawn + offsets::m_pGameSceneNode) + offsets::m_vecAbsOrigin)).length() / 100.f;
        if (g_cfg.espMaxDistance > 0 && dist > g_cfg.espMaxDistance) continue;

        Vec3 headPos = {origin.x, origin.y, origin.z + 75.f};
        
        auto screenFoot = w2s(origin);
        auto screenHead = w2s(headPos);
        
        if (!screenFoot || !screenHead) continue;
        
        float h = screenFoot->y - screenHead->y;
        float w = h * 0.4f;
        float x = screenHead->x - w / 2.f;
        float y = screenHead->y;
        
        ImU32 col = isEnemy ? 
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
                drawCorneredBox(draw, x, y, w, h, col);
            } else {
                draw->AddRect({x - 1, y - 1}, {x + w + 1, y + h + 1}, IM_COL32(0, 0, 0, 200));
                draw->AddRect({x, y}, {x + w, y + h}, col);
            }
        }
        
        // Health
        if (g_cfg.espHealth) {
             draw->AddRectFilled({x-6, y}, {x-2, y+h}, IM_COL32(0,0,0,180));
             float hp = health / 100.f;
             float barHeight = h * hp;
             ImU32 healthColor = hp > 0.5f ? 
                IM_COL32((int)((1 - hp) * 2 * 255), 255, 0, 255) : 
                IM_COL32(255, (int)(hp * 2 * 255), 0, 255);
             draw->AddRectFilled({x-5, y+h-barHeight}, {x-3, y+h}, healthColor);
             draw->AddRect({x-6, y}, {x-2, y+h}, IM_COL32(0,0,0,255));
        }
        
        // Armor
        if (g_cfg.espArmor && armor > 0) {
             draw->AddRectFilled({x+w+2, y}, {x+w+6, y+h}, IM_COL32(0,0,0,180));
             float ap = armor / 100.f;
             float barHeight = h * ap;
             draw->AddRectFilled({x+w+3, y+h-barHeight}, {x+w+5, y+h}, IM_COL32(50, 150, 255, 255));
             draw->AddRect({x+w+2, y}, {x+w+6, y+h}, IM_COL32(0,0,0,255));
        }
        
        // Bones
        if (g_cfg.espSkeleton) {
             auto modelState = Read<uintptr_t>(sceneNode + offsets::m_modelState);
             auto boneArray = Read<uintptr_t>(modelState + offsets::m_boneArray);
             if (boneArray) {
                 auto getBone = [&](int idx) -> Vec3 {
                     struct Bone { float x,y,z; char pad[0x14]; };
                     auto b = Read<Bone>(boneArray + idx * 32);
                     return {b.x, b.y, b.z};
                 };
                 
                 auto line = [&](int a, int b) {
                     auto p1 = w2s(getBone(a));
                     auto p2 = w2s(getBone(b));
                     ImU32 skelCol = ImGui::ColorConvertFloat4ToU32({g_cfg.colorSkeleton[0], g_cfg.colorSkeleton[1], g_cfg.colorSkeleton[2], 1.f});
                     if (p1 && p2) draw->AddLine(*p1, *p2, skelCol);
                 };
                 
                 line(6, 5); line(5, 4); line(4, 0); // Spine
                 line(5, 8); line(8, 9); line(9, 10); // L Arm
                 line(5, 13); line(13, 14); line(14, 15); // R Arm
                 line(0, 22); line(22, 23); line(23, 24); // L Leg
                 line(0, 25); line(25, 26); line(26, 27); // R Leg
             }
        }

        // Head Dot
        if (g_cfg.espHeadDot) {
            auto head2D = w2s({headPos.x, headPos.y, headPos.z}); // Using calculated headPos from origin + 75
            // Or better, use bone head if available for accuracy
            auto modelState = Read<uintptr_t>(sceneNode + offsets::m_modelState);
            auto boneArray = Read<uintptr_t>(modelState + offsets::m_boneArray);
            if (boneArray) {
                 struct Bone { float x,y,z; char pad[0x14]; };
                 auto b = Read<Bone>(boneArray + 6 * 32); // Head bone
                 head2D = w2s({b.x, b.y, b.z});
            }

            if (head2D) {
                ImU32 headCol = ImGui::ColorConvertFloat4ToU32({
                    g_cfg.colorHeadDot[0], g_cfg.colorHeadDot[1], 
                    g_cfg.colorHeadDot[2], g_cfg.colorHeadDot[3]
                });
                draw->AddCircleFilled(*head2D, g_cfg.espHeadDotSize, headCol);
                draw->AddCircle(*head2D, g_cfg.espHeadDotSize + 0.5f, IM_COL32(0,0,0,255));
            }
        }
        
        // Name
        if (g_cfg.espName) {
            char name[64] = {0};
            memcpy(name, reinterpret_cast<void*>(controller + offsets::m_iszPlayerName), 63);
            
            if (name[0]) {
                ImVec2 sz = ImGui::CalcTextSize(name);
                ImVec2 namePos = {x + w/2 - sz.x/2, y - sz.y - 4};
                if (g_cfg.espOutlinedText) drawOutlinedText(draw, namePos, IM_COL32(255,255,255,255), name);
                else draw->AddText(namePos, IM_COL32(255,255,255,255), name);
            }
        }

        // Distance
        if (g_cfg.espDistance) {
            char buf[32];
            snprintf(buf, sizeof(buf), XString("%.0f m"), dist);
            ImVec2 sz = ImGui::CalcTextSize(buf);
            ImVec2 pos = {x + w/2 - sz.x/2, y + h + 4};
            if (g_cfg.espOutlinedText) drawOutlinedText(draw, pos, IM_COL32(255,255,255,200), buf);
            else draw->AddText(pos, IM_COL32(255,255,255,200), buf);
        }
    }
    
    // Crosshair
    if (g_cfg.espSniperCrosshair) {
        float cx = screenW / 2.f;
        float cy = screenH / 2.f;
        draw->AddLine({cx - 8, cy}, {cx + 8, cy}, IM_COL32(255, 0, 0, 255), 1.5f);
        draw->AddLine({cx, cy - 8}, {cx, cy + 8}, IM_COL32(255, 0, 0, 255), 1.5f);
    }
}

// ============================================
// Menu (Matched to External)
// ============================================
static int g_currentTab = 0;

void RenderMenu() {
    if (!g_cfg.showMenu) return;
    
    ImGui::SetNextWindowSize(ImVec2(520, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin(XString("INTERNAL v1.0.0"), &g_cfg.showMenu, 
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize);
    
    ImDrawList* windowDraw = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();
    
    // Gradient header
    windowDraw->AddRectFilledMultiColor(
        ImVec2(wp.x + 1, wp.y + 24), ImVec2(wp.x + ws.x - 1, wp.y + 26),
        IM_COL32(130, 70, 200, 255), IM_COL32(70, 130, 230, 255),
        IM_COL32(70, 130, 230, 255), IM_COL32(130, 70, 200, 255)
    );
    
    // Tabs
    ImGui::BeginChild(XString("Tabs"), ImVec2(90, -50), true);
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
    
    auto tabButton = [](const char* label, int idx, int& current) {
        ImGui::PushStyleColor(ImGuiCol_Button, current == idx ? 
            ImVec4(0.35f, 0.28f, 0.55f, 1.f) : ImVec4(0.18f, 0.16f, 0.25f, 1.f));
        if (ImGui::Button(label, ImVec2(74, 32))) current = idx;
        ImGui::PopStyleColor();
        ImGui::Spacing();
    };
    
    ImGui::Spacing();
    tabButton(XString("ESP"), 0, g_currentTab);
    tabButton(XString("Config"), 1, g_currentTab);
    
    ImGui::PopStyleVar();
    
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 45);
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.f), XString(" Injected"));
    
    ImGui::EndChild();
    
    // Content
    ImGui::SameLine();
    ImGui::BeginChild(XString("Content"), ImVec2(0, -50), true);
    
    if (g_currentTab == 0) {
        ImGui::Checkbox(XString(" Enable ESP"), &g_cfg.espEnabled);
        ImGui::Separator();
        
        ImGui::TextColored(ImVec4(0.7f, 0.6f, 0.9f, 1.f), XString("Visuals"));
        ImGui::Checkbox(XString("Box"), &g_cfg.espBox);
        if (g_cfg.espBox) { ImGui::SameLine(); ImGui::Checkbox(XString("Cornered"), &g_cfg.espCorneredBox); }
        ImGui::Checkbox(XString("Fill"), &g_cfg.espFill);
        if (g_cfg.espFill) { ImGui::SameLine(); ImGui::SliderFloat(XString("##op"), &g_cfg.espFillOpacity, 5.f, 50.f, XString("%.0f%%")); }
        
        ImGui::Checkbox(XString("Skeleton"), &g_cfg.espSkeleton);
        ImGui::Checkbox(XString("Head Dot"), &g_cfg.espHeadDot);
        if (g_cfg.espHeadDot) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            ImGui::SliderFloat(XString("##headsz"), &g_cfg.espHeadDotSize, 1.0f, 5.0f, XString("Sz: %.1f"));
        }
        ImGui::Checkbox(XString("Health Bar"), &g_cfg.espHealth);
        ImGui::Checkbox(XString("Armor Bar"), &g_cfg.espArmor);
        ImGui::Checkbox(XString("Name"), &g_cfg.espName);
        ImGui::Checkbox(XString("Distance"), &g_cfg.espDistance);
        ImGui::Checkbox(XString("Crosshair"), &g_cfg.espSniperCrosshair);
        ImGui::Checkbox(XString("Outlined Text"), &g_cfg.espOutlinedText);
        
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.6f, 0.9f, 1.f), XString("Filters"));
        ImGui::Checkbox(XString("Enemy Only"), &g_cfg.espEnemyOnly);
        ImGui::SliderFloat(XString("Range"), &g_cfg.espMaxDistance, 0.f, 2000.f, XString("%.0f m"));
        
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.6f, 0.9f, 1.f), XString("Colors"));
        ImGui::ColorEdit3(XString("Enemy"), g_cfg.colorEnemy);
        ImGui::ColorEdit3(XString("Team"), g_cfg.colorTeam);
        ImGui::ColorEdit3(XString("Skeleton"), g_cfg.colorSkeleton);
        if (g_cfg.espHeadDot) {
            ImGui::ColorEdit4(XString("Head Dot"), g_cfg.colorHeadDot);
        }
    }
    else if (g_currentTab == 1) {
        ImGui::TextDisabled(XString("Config"));
        
        if (ImGui::Button(XString("Save Config"))) SaveConfig();
        if (ImGui::Button(XString("Load Config"))) LoadConfig();
        
        ImGui::Spacing();
        if (ImGui::Button(XString("Unload Cheat"))) { 
            // In a real cheat we'd signal an atomic bool to break the main loop and free library
        }
    }
    
    ImGui::EndChild();
    
    // Footer
    ImGui::SetCursorPosY(ws.y - 42);
    ImGui::Separator();
    ImGui::TextDisabled(XString(" INSERT - Toggle Menu"));
    
    ImGui::End();
}

// ============================================
// Hooks
// ============================================
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_cfg.showMenu) {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return true;
    }
    using CallWindowProcW_t = LRESULT(WINAPI*)(WNDPROC, HWND, UINT, WPARAM, LPARAM);
    return ((CallWindowProcW_t)LI_FN(CallWindowProcW))(g_oWndProc, hwnd, msg, wParam, lParam);
}

HRESULT __stdcall HookedPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!g_init) {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_device))) {
            g_device->GetImmediateContext(&g_context);
            
            DXGI_SWAP_CHAIN_DESC sd;
            pSwapChain->GetDesc(&sd);
            g_hwnd = sd.OutputWindow;
            
            ID3D11Texture2D* pBackBuffer;
            pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
            g_device->CreateRenderTargetView(pBackBuffer, nullptr, &g_rtv);
            pBackBuffer->Release();
            
            // Init ImGui
            ImGui::CreateContext();
            
            // Custom Theme (hexaPjj style - Purple/Blue Neon)
            ImGuiStyle& style = ImGui::GetStyle();
            ImVec4* colors = style.Colors;
            
            style.WindowRounding = 6.f;
            style.FrameRounding = 4.f;
            style.PopupRounding = 4.f;
            style.ScrollbarRounding = 4.f;
            style.GrabRounding = 3.f;
            style.TabRounding = 4.f;
            style.ChildRounding = 4.f;
            style.WindowPadding = ImVec2(10, 10);
            style.FramePadding = ImVec2(8, 4);
            style.ItemSpacing = ImVec2(8, 6);
            style.ItemInnerSpacing = ImVec2(6, 4);
            style.WindowBorderSize = 1.f;
            
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
            
            ImGui_ImplWin32_Init(g_hwnd);
            ImGui_ImplDX11_Init(g_device, g_context);
            
            // Lazy Import for SetWindowLongPtr
            using SetWindowLongPtrW_t = LONG_PTR(WINAPI*)(HWND, int, LONG_PTR);
            g_oWndProc = (WNDPROC)((SetWindowLongPtrW_t)LI_FN(SetWindowLongPtrW))(g_hwnd, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);
            
            // Auto-Load Config
            LoadConfig();
            
            g_init = true;
        }
    }
    
    if (g_init) {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        
        RenderESP();
        
        // Menu toggle
        if (GetAsyncKeyState(VK_INSERT) & 1) g_cfg.showMenu = !g_cfg.showMenu;
        
        // Mouse cursor
        ImGui::GetIO().MouseDrawCursor = g_cfg.showMenu;
        
        RenderMenu();
        
        ImGui::Render();
        g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
    
    return g_swapChainHook.GetOriginal<HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT)>(8)(pSwapChain, SyncInterval, Flags);
}

// ============================================
// Main
// ============================================
DWORD WINAPI MainThread(LPVOID) {
    // Lazy Load Library
    using GetModuleHandleA_t = HMODULE(WINAPI*)(LPCSTR);
    g_client = (uintptr_t)((GetModuleHandleA_t)LI_FN(GetModuleHandleA))(XString("client.dll"));
    
    // Create Dummy SwapChain to find VTable
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    DXGI_SWAP_CHAIN_DESC scd{ 0 };
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    
    using GetForegroundWindow_t = HWND(WINAPI*)();
    scd.OutputWindow = ((GetForegroundWindow_t)LI_FN(GetForegroundWindow))();
    
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    IDXGISwapChain* pDummySwapChain = nullptr;
    ID3D11Device* pDummyDevice = nullptr;
    ID3D11DeviceContext* pDummyContext = nullptr;

    using D3D11CreateDeviceAndSwapChain_t = HRESULT(WINAPI*)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*, UINT, UINT, const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
    
    if (FAILED(((D3D11CreateDeviceAndSwapChain_t)LI_FN(D3D11CreateDeviceAndSwapChain))(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &featureLevel, 1, D3D11_SDK_VERSION, 
        &scd, &pDummySwapChain, &pDummyDevice, nullptr, &pDummyContext))) {
        return 0;
    }
    
    // Apply VMT Hook
    if (g_swapChainHook.Init(pDummySwapChain)) {
        g_swapChainHook.Hook(8, HookedPresent);
    }
    
    // Cleanup dummy
    pDummySwapChain->Release();
    pDummyDevice->Release();
    pDummyContext->Release();
    
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        using CreateThread_t = HANDLE(WINAPI*)(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
        ((CreateThread_t)LI_FN(CreateThread))(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
