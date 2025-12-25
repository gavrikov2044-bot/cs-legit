/*
 * EXTERNA ESP MENU - Modern Design
 * In-game overlay with collapsible sections
 * Version 2.0
 */

#pragma once
#include <Windows.h>
#include <string>
#include <vector>

// ImGui will be included in main external file
// This is just the structure

namespace esp_menu {
    
    // ============================================
    // Menu State
    // ============================================
    bool g_menuOpen = false;
    bool g_menuMinimized = false;
    
    // ============================================
    // ESP Settings
    // ============================================
    struct ESPSettings {
        // Visuals
        bool boxESP = true;
        bool nameESP = true;
        bool healthBar = true;
        bool weaponInfo = true;
        bool distanceESP = true;
        bool skeletonESP = false;
        bool glowESP = false;
        bool snaplines = false;
        
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
        
        // Aimbot
        bool aimbotEnabled = false;
        float aimbotFOV = 90.0f;
        float aimbotSmooth = 80.0f;
        int aimbotBone = 8; // Head
        int aimbotKey = VK_XBUTTON1; // Mouse4
        bool visibilityCheck = true;
        bool teamCheck = true;
        
        // Misc
        bool bunnyHop = false;
        bool triggerbot = false;
        bool recoilControl = false;
        bool autoStrafe = false;
        
        // Display
        float maxDistance = 500.0f;
        bool showDistance = true;
    };
    
    ESPSettings g_settings;
    
    // ============================================
    // Section States (for collapsible sections)
    // ============================================
    bool g_sectionVisuals = true;
    bool g_sectionAdvanced = false;
    bool g_sectionColors = false;
    bool g_sectionAimbot = false;
    bool g_sectionMisc = false;
    
    // ============================================
    // Theme Colors (matching launcher)
    // ============================================
    struct Colors {
        static constexpr ImVec4 bg_deep      = ImVec4(0.04f, 0.04f, 0.06f, 0.95f);
        static constexpr ImVec4 bg_card      = ImVec4(0.07f, 0.07f, 0.10f, 0.95f);
        static constexpr ImVec4 primary      = ImVec4(0.54f, 0.17f, 0.89f, 1.00f);
        static constexpr ImVec4 success      = ImVec4(0.00f, 1.00f, 0.53f, 1.00f);
        static constexpr ImVec4 text         = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        static constexpr ImVec4 text_secondary = ImVec4(0.63f, 0.63f, 0.69f, 1.00f);
    };
    
    // ============================================
    // Helper Functions
    // ============================================
    
    // Collapsible section header
    bool CollapsingSection(const char* label, bool* open) {
        ImGui::PushStyleColor(ImGuiCol_Header, Colors::primary);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.64f, 0.27f, 0.99f, 1.00f));
        
        bool clicked = ImGui::CollapsingHeader(label, 
            *open ? ImGuiTreeNodeFlags_DefaultOpen : 0);
        
        *open = clicked;
        
        ImGui::PopStyleColor(2);
        return clicked;
    }
    
    // Checkbox with custom styling
    bool StyledCheckbox(const char* label, bool* v) {
        ImGui::PushStyleColor(ImGuiCol_CheckMark, Colors::success);
        bool result = ImGui::Checkbox(label, v);
        ImGui::PopStyleColor();
        return result;
    }
    
    // Slider with label
    bool StyledSlider(const char* label, float* v, float min, float max, const char* format) {
        ImGui::PushItemWidth(200);
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, Colors::primary);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.64f, 0.27f, 0.99f, 1.00f));
        
        bool result = ImGui::SliderFloat(label, v, min, max, format);
        
        ImGui::PopStyleColor(2);
        ImGui::PopItemWidth();
        return result;
    }
    
    // Color edit button
    bool ColorButton(const char* label, float col[4]) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(col[0], col[1], col[2], col[3]));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col[0]*1.2f, col[1]*1.2f, col[2]*1.2f, col[3]));
        
        bool result = ImGui::Button(label, ImVec2(30, 30));
        
        ImGui::PopStyleColor(2);
        return result;
    }
    
    // ============================================
    // Menu Rendering
    // ============================================
    
    void RenderMenu() {
        if (!g_menuOpen) return;
        
        // Minimized state
        if (g_menuMinimized) {
            ImGui::SetNextWindowSize(ImVec2(150, 40));
            ImGui::Begin("##ESP_Mini", &g_menuOpen, 
                ImGuiWindowFlags_NoResize | 
                ImGuiWindowFlags_NoTitleBar | 
                ImGuiWindowFlags_NoScrollbar);
            
            ImGui::TextColored(Colors::primary, "▓ EXTERNA ▓");
            ImGui::SameLine();
            if (ImGui::SmallButton("[+]")) {
                g_menuMinimized = false;
            }
            
            ImGui::End();
            return;
        }
        
        // Full menu
        ImGui::SetNextWindowSize(ImVec2(350, 600), ImGuiCond_FirstUseEver);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Colors::bg_deep);
        ImGui::PushStyleColor(ImGuiCol_Border, Colors::primary);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
        
        ImGui::Begin("▓ EXTERNA ESP ▓", &g_menuOpen, ImGuiWindowFlags_NoCollapse);
        
        // Minimize button
        ImGui::SameLine(ImGui::GetWindowWidth() - 30);
        if (ImGui::SmallButton("[-]")) {
            g_menuMinimized = true;
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // ============================================
        // VISUALS Section
        // ============================================
        if (CollapsingSection("VISUALS", &g_sectionVisuals)) {
            ImGui::Indent(10);
            
            StyledCheckbox("Box ESP", &g_settings.boxESP);
            StyledCheckbox("Name ESP", &g_settings.nameESP);
            StyledCheckbox("Health Bar", &g_settings.healthBar);
            StyledCheckbox("Weapon Info", &g_settings.weaponInfo);
            StyledCheckbox("Distance", &g_settings.distanceESP);
            StyledCheckbox("Skeleton ESP", &g_settings.skeletonESP);
            StyledCheckbox("Glow ESP", &g_settings.glowESP);
            StyledCheckbox("Snaplines", &g_settings.snaplines);
            
            ImGui::Spacing();
            StyledSlider("Max Distance", &g_settings.maxDistance, 100.0f, 1000.0f, "%.0f m");
            
            ImGui::Unindent(10);
        }
        
        ImGui::Spacing();
        
        // ============================================
        // ADVANCED ESP Section
        // ============================================
        if (CollapsingSection("ADVANCED ESP", &g_sectionAdvanced)) {
            ImGui::Indent(10);
            
            StyledCheckbox("Loot ESP", &g_settings.lootESP);
            ImGui::SameLine();
            ImGui::TextColored(Colors::text_secondary, "(NEW)");
            
            StyledCheckbox("Sound ESP", &g_settings.soundESP);
            ImGui::SameLine();
            ImGui::TextColored(Colors::text_secondary, "(NEW)");
            
            StyledCheckbox("Radar Hack", &g_settings.radarHack);
            ImGui::SameLine();
            ImGui::TextColored(Colors::text_secondary, "(NEW)");
            
            StyledCheckbox("Bomb Timer", &g_settings.bombTimer);
            StyledCheckbox("Spectator List", &g_settings.spectatorList);
            
            ImGui::Unindent(10);
        }
        
        ImGui::Spacing();
        
        // ============================================
        // COLORS Section
        // ============================================
        if (CollapsingSection("COLORS", &g_sectionColors)) {
            ImGui::Indent(10);
            
            ImGui::Text("Enemy:");
            ImGui::SameLine();
            if (ColorButton("##enemy", g_settings.enemyColor)) {
                ImGui::OpenPopup("Enemy Color");
            }
            ImGui::SameLine();
            ImGui::TextColored(Colors::text_secondary, "#%.2X%.2X%.2X",
                (int)(g_settings.enemyColor[0] * 255),
                (int)(g_settings.enemyColor[1] * 255),
                (int)(g_settings.enemyColor[2] * 255)
            );
            
            if (ImGui::BeginPopup("Enemy Color")) {
                ImGui::ColorPicker4("##picker", g_settings.enemyColor);
                ImGui::EndPopup();
            }
            
            ImGui::Text("Team:");
            ImGui::SameLine();
            if (ColorButton("##team", g_settings.teamColor)) {
                ImGui::OpenPopup("Team Color");
            }
            ImGui::SameLine();
            ImGui::TextColored(Colors::text_secondary, "#%.2X%.2X%.2X",
                (int)(g_settings.teamColor[0] * 255),
                (int)(g_settings.teamColor[1] * 255),
                (int)(g_settings.teamColor[2] * 255)
            );
            
            if (ImGui::BeginPopup("Team Color")) {
                ImGui::ColorPicker4("##picker", g_settings.teamColor);
                ImGui::EndPopup();
            }
            
            ImGui::Text("Loot:");
            ImGui::SameLine();
            if (ColorButton("##loot", g_settings.lootColor)) {
                ImGui::OpenPopup("Loot Color");
            }
            ImGui::SameLine();
            ImGui::TextColored(Colors::text_secondary, "#%.2X%.2X%.2X",
                (int)(g_settings.lootColor[0] * 255),
                (int)(g_settings.lootColor[1] * 255),
                (int)(g_settings.lootColor[2] * 255)
            );
            
            if (ImGui::BeginPopup("Loot Color")) {
                ImGui::ColorPicker4("##picker", g_settings.lootColor);
                ImGui::EndPopup();
            }
            
            ImGui::Unindent(10);
        }
        
        ImGui::Spacing();
        
        // ============================================
        // AIMBOT Section
        // ============================================
        if (CollapsingSection("AIMBOT", &g_sectionAimbot)) {
            ImGui::Indent(10);
            
            StyledCheckbox("Enable Aimbot", &g_settings.aimbotEnabled);
            
            if (g_settings.aimbotEnabled) {
                StyledSlider("FOV", &g_settings.aimbotFOV, 10.0f, 180.0f, "%.0f°");
                StyledSlider("Smooth", &g_settings.aimbotSmooth, 0.0f, 100.0f, "%.0f%%");
                
                ImGui::Text("Bone:");
                ImGui::SameLine();
                const char* bones[] = { "Head", "Neck", "Chest", "Stomach" };
                ImGui::Combo("##bone", &g_settings.aimbotBone, bones, IM_ARRAYSIZE(bones));
                
                StyledCheckbox("Visibility Check", &g_settings.visibilityCheck);
                StyledCheckbox("Team Check", &g_settings.teamCheck);
            }
            
            ImGui::Unindent(10);
        }
        
        ImGui::Spacing();
        
        // ============================================
        // MISC Section
        // ============================================
        if (CollapsingSection("MISC", &g_sectionMisc)) {
            ImGui::Indent(10);
            
            StyledCheckbox("Bunny Hop", &g_settings.bunnyHop);
            StyledCheckbox("Triggerbot", &g_settings.triggerbot);
            StyledCheckbox("Recoil Control", &g_settings.recoilControl);
            StyledCheckbox("Auto Strafe", &g_settings.autoStrafe);
            
            ImGui::Unindent(10);
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // ============================================
        // Bottom Buttons
        // ============================================
        ImGui::PushStyleColor(ImGuiCol_Button, Colors::primary);
        
        if (ImGui::Button("💾 SAVE", ImVec2(100, 30))) {
            // Save config
        }
        ImGui::SameLine();
        if (ImGui::Button("🔄 RESET", ImVec2(100, 30))) {
            // Reset to defaults
        }
        ImGui::SameLine();
        if (ImGui::Button("📋 LOAD", ImVec2(100, 30))) {
            // Load config
        }
        
        ImGui::PopStyleColor();
        
        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }
    
    // ============================================
    // Hotkeys
    // ============================================
    void HandleHotkeys() {
        // INSERT - Toggle menu
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            g_menuOpen = !g_menuOpen;
        }
        
        // HOME - Toggle ESP
        if (GetAsyncKeyState(VK_HOME) & 1) {
            g_settings.boxESP = !g_settings.boxESP;
        }
        
        // END - Toggle Aimbot
        if (GetAsyncKeyState(VK_END) & 1) {
            g_settings.aimbotEnabled = !g_settings.aimbotEnabled;
        }
    }
    
    // ============================================
    // Config Save/Load (placeholder)
    // ============================================
    void SaveConfig(const char* filename) {
        // TODO: Implement JSON config save
    }
    
    void LoadConfig(const char* filename) {
        // TODO: Implement JSON config load
    }
}

