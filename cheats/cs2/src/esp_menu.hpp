/*
 * EXTERNA ESP MENU - Professional ESP Design
 * External cheat with advanced ESP features only
 * Version 3.0
 */

#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <atomic>
#include "uiaccess.hpp"  // Fullscreen overlay support

// ImGui will be included in main external file

// Forward declarations for global variables (defined in main.cpp)
extern std::atomic<bool> g_running;
#if LUAJIT_ENABLED
extern bool g_luaEnabled;
#endif
#if DRIVER_MAPPER_ENABLED
extern bool g_kernelModeActive;
#endif

namespace esp_menu {
    
    // ============================================
    // Menu State
    // ============================================
    bool g_menuOpen = false;
    bool g_menuMinimized = false;
    float g_animationTime = 0.0f; // For smooth animations
    
    // ============================================
    // ESP Settings (ESP ONLY - No Aimbot/Misc)
    // ============================================
    // ESP Mode enum (synced with main.cpp)
    enum class ESPMode : int { CPP = 0, LUA = 1, BOTH = 2 };
    
    struct ESPSettings {
        // Render Mode
        ESPMode espMode = ESPMode::CPP;  // 0=C++, 1=Lua, 2=Both
        
        // Basic Visuals
        bool boxESP = true;
        bool nameESP = true;
        bool healthBar = true;
        bool distanceESP = true;
        bool skeletonESP = false;
        bool snaplines = false;
        
        // Advanced Skeleton (from UC)
        bool jointDots = false;              // Draw dots on each joint
        float jointDotSize = 3.0f;           // Joint dot size
        bool headHitbox = false;             // Draw circle around head
        float headHitboxRadius = 8.0f;       // Head hitbox radius
        bool healthBasedSkeleton = false;    // Color skeleton based on HP
        float skeletonThickness = 1.5f;      // Skeleton line thickness
        
        // Advanced ESP
        bool lootESP = false;
        bool soundESP = false;
        bool radarHack = false;
        bool bombTimer = false;
        bool spectatorList = false;
        
        // Colors
        float enemyColor[4] = {1.0f, 0.24f, 0.24f, 1.0f}; // Red
        float teamColor[4] = {0.0f, 0.71f, 1.0f, 1.0f};   // Blue
        float lootColor[4] = {1.0f, 0.84f, 0.0f, 1.0f};   // Gold
        
        // Display
        float maxDistance = 500.0f;
    };
    
    ESPSettings g_settings;
    
    // ============================================
    // Presets
    // ============================================
    enum class Preset {
        Safe,    // Минимальные настройки
        Legit,   // Незаметный WH
        Full     // Все фичи
    };
    
    void ApplyPreset(Preset preset) {
        switch (preset) {
            case Preset::Safe:
                // Минимум для безопасности
                g_settings.boxESP = true;
                g_settings.nameESP = false;
                g_settings.healthBar = true;
                g_settings.distanceESP = false;
                g_settings.skeletonESP = false;
                g_settings.snaplines = false;
                g_settings.lootESP = false;
                g_settings.soundESP = false;
                g_settings.radarHack = false;
                g_settings.bombTimer = false;
                g_settings.spectatorList = false;
                g_settings.maxDistance = 300.0f;
                break;
                
            case Preset::Legit:
                // Незаметный легит
                g_settings.boxESP = true;
                g_settings.nameESP = true;
                g_settings.healthBar = true;
                g_settings.distanceESP = true;
                g_settings.skeletonESP = false;
                g_settings.snaplines = false;
                g_settings.lootESP = false;
                g_settings.soundESP = false;
                g_settings.radarHack = true;
                g_settings.bombTimer = true;
                g_settings.spectatorList = true;
                g_settings.maxDistance = 500.0f;
                break;
                
            case Preset::Full:
                // Все функции
                g_settings.boxESP = true;
                g_settings.nameESP = true;
                g_settings.healthBar = true;
                g_settings.distanceESP = true;
                g_settings.skeletonESP = true;
                g_settings.snaplines = true;
                // Advanced Skeleton (UC)
                g_settings.jointDots = true;
                g_settings.jointDotSize = 3.0f;
                g_settings.headHitbox = true;
                g_settings.headHitboxRadius = 8.0f;
                g_settings.healthBasedSkeleton = true;
                g_settings.skeletonThickness = 2.0f;
                g_settings.lootESP = true;
                g_settings.soundESP = true;
                g_settings.radarHack = true;
                g_settings.bombTimer = true;
                g_settings.spectatorList = true;
                g_settings.maxDistance = 1000.0f;
                break;
        }
    }
    
    // ============================================
    // Section States
    // ============================================
    bool g_sectionVisuals = true;
    bool g_sectionAdvanced = false;
    bool g_sectionColors = false;
    bool g_sectionPresets = false;
    
    // ============================================
    // Theme Colors
    // ============================================
    struct Colors {
        static constexpr ImVec4 bg_deep      = ImVec4(0.04f, 0.04f, 0.06f, 0.97f);
        static constexpr ImVec4 bg_card      = ImVec4(0.07f, 0.07f, 0.10f, 0.95f);
        static constexpr ImVec4 primary      = ImVec4(0.54f, 0.17f, 0.89f, 1.00f);
        static constexpr ImVec4 success      = ImVec4(0.00f, 1.00f, 0.53f, 1.00f);
        static constexpr ImVec4 warning      = ImVec4(1.00f, 0.65f, 0.00f, 1.00f);
        static constexpr ImVec4 danger       = ImVec4(1.00f, 0.24f, 0.24f, 1.00f);
        static constexpr ImVec4 text         = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        static constexpr ImVec4 text_secondary = ImVec4(0.63f, 0.63f, 0.69f, 1.00f);
    };
    
    // ============================================
    // Helper Functions
    // ============================================
    
    // Help Marker (Tooltip)
    void HelpMarker(const char* desc) {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    // Animated collapsible section
    bool CollapsingSection(const char* label, bool* open) {
        ImGui::PushStyleColor(ImGuiCol_Header, Colors::primary);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.64f, 0.27f, 0.99f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.74f, 0.37f, 1.09f, 1.00f));
        
        bool clicked = ImGui::CollapsingHeader(label, 
            *open ? ImGuiTreeNodeFlags_DefaultOpen : 0);
        
        *open = clicked;
        
        ImGui::PopStyleColor(3);
        return clicked;
    }
    
    // Enhanced checkbox with glow effect
    bool StyledCheckbox(const char* label, bool* v, const char* tooltip = nullptr) {
        ImGui::PushStyleColor(ImGuiCol_CheckMark, Colors::success);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.16f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.15f, 0.15f, 0.20f, 1.0f));
        bool result = ImGui::Checkbox(label, v);
        ImGui::PopStyleColor(3);
        
        if (tooltip && ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(tooltip);
            ImGui::EndTooltip();
        }
        return result;
    }
    
    // Styled slider with gradient
    bool StyledSlider(const char* label, float* v, float min, float max, const char* format) {
        ImGui::PushItemWidth(220);
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, Colors::primary);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.64f, 0.27f, 0.99f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.16f, 1.0f));
        
        bool result = ImGui::SliderFloat(label, v, min, max, format);
        
        ImGui::PopStyleColor(3);
        ImGui::PopItemWidth();
        return result;
    }
    
    // Color button with preview
    bool ColorButton(const char* label, float col[4]) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(col[0], col[1], col[2], col[3]));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col[0]*1.2f, col[1]*1.2f, col[2]*1.2f, col[3]));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        
        bool result = ImGui::Button(label, ImVec2(35, 35));
        
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        return result;
    }
    
    // Mini ESP preview window
    void DrawESPPreview(float x, float y, float w, float h) {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        
        // Background
        draw->AddRectFilled(
            ImVec2(x, y), 
            ImVec2(x + w, y + h), 
            ImGui::ColorConvertFloat4ToU32(Colors::bg_card)
        );
        draw->AddRect(
            ImVec2(x, y), 
            ImVec2(x + w, y + h), 
            ImGui::ColorConvertFloat4ToU32(Colors::primary),
            4.0f, 0, 1.5f
        );
        
        // Center position for "player"
        float centerX = x + w / 2;
        float centerY = y + h / 2;
        
        // Draw mini ESP preview
        float boxW = 30;
        float boxH = 60;
        float boxX = centerX - boxW / 2;
        float boxY = centerY - boxH / 2;
        
        ImU32 enemyCol = ImGui::ColorConvertFloat4ToU32({
            g_settings.enemyColor[0], 
            g_settings.enemyColor[1], 
            g_settings.enemyColor[2], 
            1.0f
        });
        
        // Box
        if (g_settings.boxESP) {
            draw->AddRect(ImVec2(boxX, boxY), ImVec2(boxX + boxW, boxY + boxH), enemyCol, 0, 0, 2.0f);
        }
        
        // Health bar
        if (g_settings.healthBar) {
            float healthH = boxH * 0.75f;
            draw->AddRectFilled(
                ImVec2(boxX - 5, boxY + boxH - healthH), 
                ImVec2(boxX - 3, boxY + boxH), 
                ImGui::ColorConvertFloat4ToU32(Colors::success)
            );
        }
        
        // Name
        if (g_settings.nameESP) {
            const char* name = "Enemy";
            ImVec2 textSize = ImGui::CalcTextSize(name);
            draw->AddText(
                ImVec2(centerX - textSize.x / 2, boxY - textSize.y - 2), 
                ImGui::ColorConvertFloat4ToU32(Colors::text), 
                name
            );
        }
        
        // Distance
        if (g_settings.distanceESP) {
            const char* dist = "25m";
            ImVec2 textSize = ImGui::CalcTextSize(dist);
            draw->AddText(
                ImVec2(centerX - textSize.x / 2, boxY + boxH + 2), 
                ImGui::ColorConvertFloat4ToU32(Colors::text_secondary), 
                dist
            );
        }
        
        // Skeleton
        if (g_settings.skeletonESP) {
            draw->AddLine(
                ImVec2(centerX, boxY + 15), 
                ImVec2(centerX, boxY + boxH - 15), 
                enemyCol, 1.5f
            );
        }
        
        // Label
        const char* label = "PREVIEW";
        ImVec2 labelSize = ImGui::CalcTextSize(label);
        draw->AddText(
            ImVec2(x + (w - labelSize.x) / 2, y + 5), 
            ImGui::ColorConvertFloat4ToU32(Colors::text_secondary), 
            label
        );
    }
    
    // ============================================
    // Config Save/Load (INI format for simplicity)
    // ============================================
    
    void SaveConfig(const char* filename) {
        std::ofstream file(filename);
        if (!file.is_open()) return;
        
        file << "[ESP Settings]\n";
        file << "boxESP=" << g_settings.boxESP << "\n";
        file << "nameESP=" << g_settings.nameESP << "\n";
        file << "healthBar=" << g_settings.healthBar << "\n";
        file << "distanceESP=" << g_settings.distanceESP << "\n";
        file << "skeletonESP=" << g_settings.skeletonESP << "\n";
        file << "snaplines=" << g_settings.snaplines << "\n";
        // Advanced Skeleton (UC)
        file << "jointDots=" << g_settings.jointDots << "\n";
        file << "jointDotSize=" << g_settings.jointDotSize << "\n";
        file << "headHitbox=" << g_settings.headHitbox << "\n";
        file << "headHitboxRadius=" << g_settings.headHitboxRadius << "\n";
        file << "healthBasedSkeleton=" << g_settings.healthBasedSkeleton << "\n";
        file << "skeletonThickness=" << g_settings.skeletonThickness << "\n";
        file << "lootESP=" << g_settings.lootESP << "\n";
        file << "soundESP=" << g_settings.soundESP << "\n";
        file << "radarHack=" << g_settings.radarHack << "\n";
        file << "bombTimer=" << g_settings.bombTimer << "\n";
        file << "spectatorList=" << g_settings.spectatorList << "\n";
        file << "maxDistance=" << g_settings.maxDistance << "\n";
        
        file << "\n[Colors]\n";
        file << "enemyR=" << g_settings.enemyColor[0] << "\n";
        file << "enemyG=" << g_settings.enemyColor[1] << "\n";
        file << "enemyB=" << g_settings.enemyColor[2] << "\n";
        file << "teamR=" << g_settings.teamColor[0] << "\n";
        file << "teamG=" << g_settings.teamColor[1] << "\n";
        file << "teamB=" << g_settings.teamColor[2] << "\n";
        file << "lootR=" << g_settings.lootColor[0] << "\n";
        file << "lootG=" << g_settings.lootColor[1] << "\n";
        file << "lootB=" << g_settings.lootColor[2] << "\n";
        
        file.close();
    }
    
    void LoadConfig(const char* filename) {
        std::ifstream file(filename);
        if (!file.is_open()) return;
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '[') continue;
            
            size_t pos = line.find('=');
            if (pos == std::string::npos) continue;
            
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // Parse bools
            if (key == "boxESP") g_settings.boxESP = (value == "1");
            else if (key == "nameESP") g_settings.nameESP = (value == "1");
            else if (key == "healthBar") g_settings.healthBar = (value == "1");
            else if (key == "distanceESP") g_settings.distanceESP = (value == "1");
            else if (key == "skeletonESP") g_settings.skeletonESP = (value == "1");
            else if (key == "snaplines") g_settings.snaplines = (value == "1");
            // Advanced Skeleton (UC)
            else if (key == "jointDots") g_settings.jointDots = (value == "1");
            else if (key == "jointDotSize") g_settings.jointDotSize = std::stof(value);
            else if (key == "headHitbox") g_settings.headHitbox = (value == "1");
            else if (key == "headHitboxRadius") g_settings.headHitboxRadius = std::stof(value);
            else if (key == "healthBasedSkeleton") g_settings.healthBasedSkeleton = (value == "1");
            else if (key == "skeletonThickness") g_settings.skeletonThickness = std::stof(value);
            else if (key == "lootESP") g_settings.lootESP = (value == "1");
            else if (key == "soundESP") g_settings.soundESP = (value == "1");
            else if (key == "radarHack") g_settings.radarHack = (value == "1");
            else if (key == "bombTimer") g_settings.bombTimer = (value == "1");
            else if (key == "spectatorList") g_settings.spectatorList = (value == "1");
            // Parse floats
            else if (key == "maxDistance") g_settings.maxDistance = std::stof(value);
            // Parse colors
            else if (key == "enemyR") g_settings.enemyColor[0] = std::stof(value);
            else if (key == "enemyG") g_settings.enemyColor[1] = std::stof(value);
            else if (key == "enemyB") g_settings.enemyColor[2] = std::stof(value);
            else if (key == "teamR") g_settings.teamColor[0] = std::stof(value);
            else if (key == "teamG") g_settings.teamColor[1] = std::stof(value);
            else if (key == "teamB") g_settings.teamColor[2] = std::stof(value);
            else if (key == "lootR") g_settings.lootColor[0] = std::stof(value);
            else if (key == "lootG") g_settings.lootColor[1] = std::stof(value);
            else if (key == "lootB") g_settings.lootColor[2] = std::stof(value);
        }
        
        file.close();
    }
    
    // ============================================
    // Menu Rendering
    // ============================================
    
    void RenderMenu() {
        if (!g_menuOpen) return;
        
        // Update animation
        g_animationTime += ImGui::GetIO().DeltaTime;
        
        // Minimized state
        if (g_menuMinimized) {
            ImGui::SetNextWindowSize(ImVec2(180, 45));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, Colors::bg_deep);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
            
            ImGui::Begin("##ESP_Mini", &g_menuOpen, 
                ImGuiWindowFlags_NoResize | 
                ImGuiWindowFlags_NoTitleBar | 
                ImGuiWindowFlags_NoScrollbar);
            
            ImGui::TextColored(Colors::primary, "[ EXTERNA ESP ]");
            ImGui::SameLine();
            if (ImGui::SmallButton("[+]")) {
                g_menuMinimized = false;
            }
            
            ImGui::End();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            return;
        }
        
        // Full menu
        ImGui::SetNextWindowSize(ImVec2(600, 480), ImGuiCond_FirstUseEver);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Colors::bg_deep);
        ImGui::PushStyleColor(ImGuiCol_Border, Colors::primary);
        ImGui::PushStyleColor(ImGuiCol_Tab, Colors::bg_card);
        ImGui::PushStyleColor(ImGuiCol_TabHovered, Colors::primary);
        ImGui::PushStyleColor(ImGuiCol_TabActive, Colors::primary);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 15));
        
        ImGui::Begin("[ EXTERNA ESP - EXTERNAL CHEAT ]", &g_menuOpen, ImGuiWindowFlags_NoCollapse);
        
        // Header with minimize button
        ImGui::SameLine(ImGui::GetWindowWidth() - 35);
        if (ImGui::SmallButton("[-]")) {
            g_menuMinimized = true;
        }
        
        ImGui::Spacing();
        
        // Status indicator
        ImGui::TextColored(Colors::success, "[*] STATUS: ACTIVE");
        ImGui::SameLine();
        if (uiaccess::CheckUIAccess()) {
            ImGui::TextColored(Colors::primary, "[FULLSCREEN]");
        } else {
            ImGui::TextColored(Colors::text_secondary, "[Borderless]");
        }
        ImGui::SameLine();
#if DRIVER_MAPPER_ENABLED
        if (g_kernelModeActive) {
            ImGui::TextColored(Colors::success, "[Ring 0]");
        } else {
            ImGui::TextColored(Colors::text_secondary, "[Ring 3]");
        }
#else
        ImGui::TextColored(Colors::text_secondary, "[Ring 3]");
#endif
        ImGui::SameLine(ImGui::GetWindowWidth() - 280);
        ImGui::TextColored(Colors::warning, "F2(Menu) | F3(Lua) | F4(Exit)");
        
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::BeginTabBar("##Tabs")) {
            
            // --- TAB: VISUALS ---
            if (ImGui::BeginTabItem(" VISUALS ")) {
                ImGui::Spacing();
                
                // ESP MODE SELECTOR
                ImGui::TextColored(Colors::warning, "ESP MODE");
                ImGui::Spacing();
                
                const char* modeNames[] = { "C++ ESP", "Lua ESP", "Both (C++ + Lua)" };
                int currentMode = static_cast<int>(g_settings.espMode);
                ImGui::SetNextItemWidth(200);
                if (ImGui::Combo("##espmode", &currentMode, modeNames, 3)) {
                    g_settings.espMode = static_cast<ESPMode>(currentMode);
                }
                ImGui::SameLine();
                ImGui::TextColored(Colors::text_secondary, "(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("C++ ESP = Fast built-in ESP");
                    ImGui::Text("Lua ESP = Custom scripts only");
                    ImGui::Text("Both = C++ + Lua overlay");
                    ImGui::EndTooltip();
                }
                
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                ImGui::Columns(2, "visual_cols", false);
                
                // Column 1: ESP General
                ImGui::TextColored(Colors::primary, "GENERAL ESP");
                ImGui::Spacing();
                
                StyledCheckbox("Box ESP", &g_settings.boxESP, "Draw 2D Box around enemies");
                StyledCheckbox("Name ESP", &g_settings.nameESP, "Show enemy names");
                StyledCheckbox("Health Bar", &g_settings.healthBar, "Show health status");
                StyledCheckbox("Distance", &g_settings.distanceESP, "Show distance in meters");
                StyledCheckbox("Snaplines", &g_settings.snaplines, "Draw lines to enemies");

                ImGui::Spacing();
                ImGui::TextColored(Colors::primary, "SETTINGS");
                ImGui::Spacing();
                StyledSlider("Max Distance", &g_settings.maxDistance, 100.0f, 1000.0f, "%.0f m");
                
                ImGui::NextColumn();
                
                // Column 2: Skeleton
                ImGui::TextColored(Colors::primary, "SKELETON & BONES");
                ImGui::Spacing();
                
                StyledCheckbox("Skeleton ESP", &g_settings.skeletonESP, "Draw player skeleton");
                StyledCheckbox("Health Color", &g_settings.healthBasedSkeleton, "Color skeleton based on HP");
                StyledSlider("Thickness", &g_settings.skeletonThickness, 0.5f, 5.0f, "%.1f px");
                
                ImGui::Spacing();
                
                StyledCheckbox("Joint Dots", &g_settings.jointDots, "Draw dots on joints");
                if (g_settings.jointDots) {
                    ImGui::Indent();
                    StyledSlider("Dot Size", &g_settings.jointDotSize, 1.0f, 8.0f, "%.1f");
                    ImGui::Unindent();
                }
                
                StyledCheckbox("Head Hitbox", &g_settings.headHitbox, "Draw circle around head");
                if (g_settings.headHitbox) {
                    ImGui::Indent();
                    StyledSlider("Hitbox Radius", &g_settings.headHitboxRadius, 3.0f, 20.0f, "%.1f");
                    ImGui::Unindent();
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                ImGui::TextColored(Colors::text_secondary, "PREVIEW");
                DrawESPPreview(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y + 5, 180, 220);

                ImGui::Columns(1);
                ImGui::EndTabItem();
            }
            
            // --- TAB: WORLD / MISC ---
            if (ImGui::BeginTabItem(" WORLD & MISC ")) {
                ImGui::Spacing();
                
                ImGui::Columns(2, "misc_cols", false);
                
                ImGui::TextColored(Colors::primary, "WORLD VISUALS");
                ImGui::Spacing();
                
                StyledCheckbox("Loot ESP", &g_settings.lootESP, "Show weapons on ground");
                StyledCheckbox("Bomb Timer", &g_settings.bombTimer, "Show C4 countdown");
                StyledCheckbox("Sound ESP", &g_settings.soundESP, "Visualize footsteps");
                
                ImGui::NextColumn();
                
                ImGui::TextColored(Colors::primary, "MISC");
                ImGui::Spacing();
                
                StyledCheckbox("Radar Hack", &g_settings.radarHack, "2D Radar in corner");
                StyledCheckbox("Spectator List", &g_settings.spectatorList, "Show who is watching you");
                
                ImGui::Columns(1);
                
                ImGui::EndTabItem();
            }
            
            // --- TAB: COLORS ---
            if (ImGui::BeginTabItem(" COLORS ")) {
                ImGui::Spacing();
                
                auto ColorPickerItem = [](const char* name, float* col) {
                    ImGui::PushID(name);
                    ImGui::Text("%s", name);
                    ImGui::SameLine(120);
                    ColorButton("##btn", col);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(200);
                    ImGui::ColorEdit4("##edit", col, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                    ImGui::PopID();
                };

                ColorPickerItem("Enemy Color", g_settings.enemyColor);
                ColorPickerItem("Team Color", g_settings.teamColor);
                ColorPickerItem("Loot Color", g_settings.lootColor);
                
                ImGui::EndTabItem();
            }
            
            // --- TAB: CONFIG ---
            if (ImGui::BeginTabItem(" CONFIG ")) {
                ImGui::Spacing();
                
                ImGui::TextColored(Colors::primary, "PRESETS");
                ImGui::Spacing();
                
                if (ImGui::Button("SAFE MODE", ImVec2(120, 30))) ApplyPreset(Preset::Safe);
                ImGui::SameLine(); ImGui::TextDisabled("Minimal features for safety");
                
                if (ImGui::Button("LEGIT MODE", ImVec2(120, 30))) ApplyPreset(Preset::Legit);
                ImGui::SameLine(); ImGui::TextDisabled("Balanced ESP + Radar");
                
                if (ImGui::Button("FULL MODE", ImVec2(120, 30))) ApplyPreset(Preset::Full);
                ImGui::SameLine(); ImGui::TextDisabled("All features enabled");
                
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                ImGui::TextColored(Colors::primary, "MANAGEMENT");
                ImGui::Spacing();
                
                if (ImGui::Button("SAVE CONFIG", ImVec2(120, 30))) SaveConfig("esp_config.ini");
                ImGui::SameLine();
                if (ImGui::Button("LOAD CONFIG", ImVec2(120, 30))) LoadConfig("esp_config.ini");
                ImGui::SameLine();
                if (ImGui::Button("RESET", ImVec2(120, 30))) g_settings = ESPSettings();
                
                ImGui::EndTabItem();
            }
            
            // --- TAB: LUA ---
            #if LUAJIT_ENABLED
            if (ImGui::BeginTabItem(" LUA ")) {
                ImGui::Spacing();
                
                ImGui::TextColored(Colors::primary, "LUA SCRIPTING");
                ImGui::Spacing();
                
                bool luaState = g_luaEnabled;
                if (ImGui::Checkbox("Enable Lua System", &luaState)) {
                    g_luaEnabled = luaState;
                }
                
                ImGui::Spacing();
                if (g_luaEnabled) {
                    ImGui::TextColored(Colors::success, "Lua Engine: RUNNING");
                    ImGui::TextDisabled("Scripts loaded from /scripts folder");
                    ImGui::Spacing();
                    
                    if (ImGui::Button("RELOAD SCRIPTS", ImVec2(150, 30))) {
                         // This would need to call reloadLua() from main.cpp
                         // But we can't easily call it here without extern.
                         // For now user can use Hotkey F6
                    }
                    ImGui::TextDisabled("Press ScrollLock to reload scripts");
                    ImGui::TextDisabled("Press PgDn to open Lua Menu");
                } else {
                    ImGui::TextColored(Colors::danger, "Lua Engine: STOPPED");
                }
                
                ImGui::EndTabItem();
            }
            #endif
            
            ImGui::EndTabBar();
        }
        
        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(5);
    }
    
    // ============================================
    // Hotkeys
    // ============================================
    void HandleHotkeys() {
        // INSERT - Toggle menu (no CS2 conflict)
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            g_menuOpen = !g_menuOpen;
        }
        
        // PAGE UP - Toggle all ESP (panic key, no CS2 conflict)
        if (GetAsyncKeyState(VK_PRIOR) & 1) {  // VK_PRIOR = PAGE UP
            bool newState = !g_settings.boxESP;
            g_settings.boxESP = newState;
            g_settings.nameESP = newState;
            g_settings.healthBar = newState;
            g_settings.distanceESP = newState;
        }
    }
}
