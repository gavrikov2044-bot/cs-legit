/*
 * EXTERNA LAUNCHER - NEW UI STRUCTURE
 * Tab-based interface with modern design
 * Version 2.0
 */

#pragma once
#include "imgui.h"
#include "modern_theme.hpp"
#include "config.hpp"  // Need Config::g_config
#include <vector>
#include <string>

namespace ui {
    // ============================================
    // Tab System
    // ============================================
    enum class Tab {
        Home,
        Games,
        Stats,
        Settings,
        Profile,
        News
    };
    
    Tab g_currentTab = Tab::Home;
    
    // Tab icons and labels (using text icons instead of emoji)
    struct TabInfo {
        Tab id;
        const char* icon;
        const char* label;
    };
    
    const TabInfo g_tabs[] = {
        { Tab::Home,     "[H]", "HOME" },
        { Tab::Games,    "[G]", "GAMES" },
        { Tab::Stats,    "[S]", "STATS" },
        { Tab::Settings, "[*]", "SETTINGS" },
        { Tab::Profile,  "[P]", "PROFILE" },
        { Tab::News,     "[N]", "NEWS" }
    };
    
    // ============================================
    // Particle System
    // ============================================
    struct Particle {
        float x, y;
        float vx, vy;
        float size;
        float alpha;
        float life;
    };
    
    std::vector<Particle> g_particles;
    
    void InitParticles(int count = 30) {
        g_particles.clear();
        for (int i = 0; i < count; i++) {
            Particle p;
            p.x = (float)(rand() % 1200);
            p.y = (float)(rand() % 700);
            p.vx = 0.0f;
            p.vy = -20.0f - (rand() % 30); // Float upward
            p.size = 2.0f + (rand() % 3);
            p.alpha = 0.3f + (rand() % 40) / 100.0f;
            p.life = 1.0f;
            g_particles.push_back(p);
        }
    }
    
    void UpdateParticles(float dt) {
        for (auto& p : g_particles) {
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.life -= dt * 0.1f;
            
            // Respawn at bottom when reaching top or dying
            if (p.y < 0 || p.life <= 0) {
                p.x = (float)(rand() % 1200);
                p.y = 700.0f;
                p.life = 1.0f;
            }
        }
    }
    
    void RenderParticles(ImDrawList* draw, ImVec2 windowPos) {
        for (const auto& p : g_particles) {
            ImVec2 pos(windowPos.x + p.x, windowPos.y + p.y);
            float alpha = p.alpha * p.life;
            draw->AddCircleFilled(
                pos,
                p.size,
                ImGui::ColorConvertFloat4ToU32(ImVec4(
                    theme::primary.x,
                    theme::primary.y,
                    theme::primary.z,
                    alpha
                )),
                8
            );
        }
    }
    
    // ============================================
    // Sidebar
    // ============================================
    void RenderSidebar() {
        ImGui::BeginChild("##sidebar", ImVec2(80, 0), true, ImGuiWindowFlags_NoScrollbar);
        
        ImGui::SetCursorPosY(20);
        
        for (const auto& tab : g_tabs) {
            ImGui::SetCursorPosX(10);
            
            bool isActive = (g_currentTab == tab.id);
            
            // Active indicator (left border)
            if (isActive) {
                ImDrawList* draw = ImGui::GetWindowDrawList();
                ImVec2 pos = ImGui::GetCursorScreenPos();
                draw->AddRectFilled(
                    ImVec2(pos.x - 10, pos.y),
                    ImVec2(pos.x - 6, pos.y + 60),
                    ImGui::ColorConvertFloat4ToU32(theme::primary),
                    0
                );
            }
            
            // Button
            ImGui::PushStyleColor(ImGuiCol_Button, isActive ? theme::bg_hover : theme::bg_card);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::bg_hover);
            ImGui::PushStyleColor(ImGuiCol_Text, isActive ? theme::text : theme::text_secondary);
            
            if (ImGui::Button(
                (std::string(tab.icon) + "\n" + tab.label).c_str(),
                ImVec2(60, 60)
            )) {
                g_currentTab = tab.id;
            }
            
            ImGui::PopStyleColor(3);
            ImGui::Spacing();
            ImGui::Spacing();
        }
        
        ImGui::EndChild();
    }
    
    // ============================================
    // Top Bar
    // ============================================
    void RenderTopBar(const char* username) {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        
        // Background
        draw->AddRectFilled(
            pos,
            ImVec2(pos.x + 1200, pos.y + 60),
            ImGui::ColorConvertFloat4ToU32(theme::bg_card),
            0
        );
        
        ImGui::SetCursorPosX(20);
        ImGui::SetCursorPosY(15);
        
        // Logo with glow
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]); // Bold font
        ImGui::TextColored(theme::primary, "▓▓▓ EXTERNA ▓▓▓");
        ImGui::PopFont();
        
        // User info (right side)
        ImGui::SameLine(1200 - 150);
        ImGui::TextColored(theme::text_secondary, "@%s", username);
        
        ImGui::SetCursorPosY(60);
        ImGui::Separator();
    }
    
    // ============================================
    // Tab Content Rendering
    // ============================================
    
    void RenderHomeTab(const std::vector<std::string>& games) {
        ImGui::BeginChild("##home_content");
        
        ImGui::SetCursorPos(ImVec2(40, 20));
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
        ImGui::TextColored(theme::text, "[*] DASHBOARD");
        ImGui::PopFont();
        
        ImGui::SetCursorPos(ImVec2(40, 70));
        
        // Quick Stats
        ImGui::BeginChild("##quick_stats", ImVec2(0, 80), true);
        ImGui::Columns(4, nullptr, false);
        
        ImGui::TextColored(theme::text_secondary, "[G] Games");
        ImGui::TextColored(theme::text, "4");
        ImGui::NextColumn();
        
        ImGui::TextColored(theme::text_secondary, "[+] Ready");
        ImGui::TextColored(theme::success, "3");
        ImGui::NextColumn();
        
        ImGui::TextColored(theme::text_secondary, "[!] Down");
        ImGui::TextColored(theme::error, "0");
        ImGui::NextColumn();
        
        ImGui::TextColored(theme::text_secondary, "[~] Updating");
        ImGui::TextColored(theme::warning, "1");
        
        ImGui::Columns(1);
        ImGui::EndChild();
        
        ImGui::SetCursorPosX(40);
        ImGui::Spacing();
        ImGui::Spacing();
        
        // Game Cards
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
        ImGui::TextColored(theme::text, "YOUR GAMES");
        ImGui::PopFont();
        
        ImGui::Spacing();
        
        // Render game cards here (will be passed from main)
        
        ImGui::EndChild();
    }
    
    void RenderGamesTab() {
        ImGui::BeginChild("##games_content");
        
        ImGui::SetCursorPos(ImVec2(40, 20));
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
        ImGui::TextColored(theme::text, "[G] AVAILABLE GAMES");
        ImGui::PopFont();
        
        ImGui::SetCursorPos(ImVec2(40, 70));
        
        // Detailed game list will be here
        ImGui::TextColored(theme::text_secondary, "Game library coming soon...");
        
        ImGui::EndChild();
    }
    
    void RenderStatsTab() {
        ImGui::BeginChild("##stats_content");
        
        ImGui::SetCursorPos(ImVec2(40, 20));
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
        ImGui::TextColored(theme::text, "[S] YOUR STATS");
        ImGui::PopFont();
        
        ImGui::SetCursorPos(ImVec2(40, 70));
        
        // Usage stats
        ImGui::BeginChild("##usage_stats", ImVec2(400, 200), true);
        ImGui::TextColored(theme::text, "USAGE STATISTICS");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextColored(theme::text_secondary, "Total Sessions:");
        ImGui::SameLine(200);
        ImGui::TextColored(theme::text, "127");
        
        ImGui::TextColored(theme::text_secondary, "Total Playtime:");
        ImGui::SameLine(200);
        ImGui::TextColored(theme::text, "43h 25m");
        
        ImGui::TextColored(theme::text_secondary, "Avg Session:");
        ImGui::SameLine(200);
        ImGui::TextColored(theme::text, "20m 30s");
        
        ImGui::Spacing();
        theme::ProgressBar(1.0f, ImVec2(360, 20), "CS2: 43h 25m");
        
        ImGui::EndChild();
        
        ImGui::SetCursorPos(ImVec2(40, 290));
        
        // Detection status
        ImGui::BeginChild("##detection_status", ImVec2(400, 200), true);
        ImGui::TextColored(theme::text, "DETECTION STATUS");
        ImGui::Separator();
        ImGui::Spacing();
        
        theme::StatusIndicator("No bans detected", true, theme::success);
        theme::StatusIndicator("Clean HWID", true, theme::success);
        theme::StatusIndicator("Anti-cheat bypass: ACTIVE", true, theme::success);
        ImGui::TextColored(theme::text_secondary, "🔒 Protection level: HIGH");
        
        ImGui::EndChild();
        
        ImGui::EndChild();
    }
    
    void RenderSettingsTab() {
        ImGui::BeginChild("##settings_content");
        
        ImGui::SetCursorPos(ImVec2(40, 20));
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
        ImGui::TextColored(theme::text, "[*] SETTINGS");
        ImGui::PopFont();
        
        ImGui::SetCursorPos(ImVec2(40, 70));
        
        // LAUNCHER section
        ImGui::TextColored(theme::text, "LAUNCHER");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::Checkbox("Start with Windows", &Config::g_config.startWithWindows);
        ImGui::Checkbox("Minimize to tray", &Config::g_config.minimizeToTray);
        ImGui::Checkbox("Auto Update", &Config::g_config.autoUpdate);
        ImGui::Checkbox("Check updates on startup", &Config::g_config.checkUpdatesOnStartup);
        
        ImGui::Spacing();
        ImGui::TextColored(theme::text_secondary, "Theme:");
        const char* themes[] = { "Dark Future", "Cyber", "Classic" };
        ImGui::Combo("##theme", &Config::g_config.theme, themes, 3);
        
        ImGui::Spacing();
        ImGui::Spacing();
        
        // INJECTION section
        ImGui::TextColored(theme::text, "INJECTION");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::RadioButton("Manual", &Config::g_config.injectionMethod, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Auto", &Config::g_config.injectionMethod, 1);
        ImGui::SameLine();
        ImGui::RadioButton("Manual Mapping", &Config::g_config.injectionMethod, 2);
        
        ImGui::Spacing();
        ImGui::TextColored(theme::text_secondary, "Injection Delay:");
        ImGui::SliderFloat("##delay", &Config::g_config.injectionDelay, 0.0f, 10.0f, "%.1f seconds");
        
        ImGui::Spacing();
        ImGui::Spacing();
        
        // ADVANCED section
        ImGui::TextColored(theme::text, "ADVANCED");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::Checkbox("Enable WebSocket", &Config::g_config.enableWebSocket);
        ImGui::Checkbox("Verbose Logging", &Config::g_config.verboseLogging);
        ImGui::Checkbox("Show Notifications", &Config::g_config.showNotifications);
        
        ImGui::Spacing();
        ImGui::TextColored(theme::text_secondary, "Status Refresh Interval:");
        ImGui::SliderInt("##status_refresh", &Config::g_config.statusRefreshInterval, 1, 10, "%d seconds");
        
        ImGui::Spacing();
        ImGui::Spacing();
        
        // Buttons
        ImGui::PushStyleColor(ImGuiCol_Button, theme::success);
        if (ImGui::Button("[SAVE] SETTINGS", ImVec2(200, 40))) {
            Config::Apply(); // Saves and applies startup registry
        }
        ImGui::PopStyleColor();
        
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, theme::warning);
        if (ImGui::Button("[RESET] DEFAULT", ImVec2(200, 40))) {
            Config::g_config = Config::LauncherConfig(); // Reset to defaults
        }
        ImGui::PopStyleColor();
        
        ImGui::EndChild();
    }
    
    void RenderProfileTab(const char* username) {
        ImGui::BeginChild("##profile_content");
        
        ImGui::SetCursorPos(ImVec2(40, 20));
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
        ImGui::TextColored(theme::text, "[P] PROFILE");
        ImGui::PopFont();
        
        ImGui::SetCursorPos(ImVec2(40, 70));
        
        // User info card
        ImGui::BeginChild("##user_info", ImVec2(500, 150), true);
        
        // Avatar placeholder
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 avatarPos = ImGui::GetCursorScreenPos();
        draw->AddCircleFilled(
            ImVec2(avatarPos.x + 50, avatarPos.y + 50),
            40,
            ImGui::ColorConvertFloat4ToU32(theme::primary),
            32
        );
        
        ImGui::SetCursorPos(ImVec2(120, 20));
        ImGui::TextColored(theme::text, "Username: @%s", username);
        ImGui::SetCursorPos(ImVec2(120, 45));
        ImGui::TextColored(theme::text_secondary, "Member Since: Dec 2025");
        ImGui::SetCursorPos(ImVec2(120, 70));
        ImGui::TextColored(theme::text_secondary, "Plan: Premium");
        ImGui::SetCursorPos(ImVec2(120, 95));
        ImGui::TextColored(theme::text_secondary, "HWID: ****-****-****-1234");
        
        ImGui::EndChild();
        
        ImGui::SetCursorPosX(40);
        ImGui::Spacing();
        
        // Licenses
        ImGui::TextColored(theme::text, "LICENSES");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::BeginChild("##license_cs2", ImVec2(500, 100), true);
        ImGui::TextColored(theme::text, "CS2 Premium");
        theme::StatusIndicator("ACTIVE", true, theme::success);
        ImGui::TextColored(theme::text_secondary, "Expires: Never (Lifetime)");
        ImGui::TextColored(theme::text_secondary, "Key: CS2-XXXX-XXXX-XXXX");
        ImGui::EndChild();
        
        ImGui::EndChild();
    }
    
    void RenderNewsTab() {
        ImGui::BeginChild("##news_content");
        
        ImGui::SetCursorPos(ImVec2(40, 20));
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
        ImGui::TextColored(theme::text, "[N] NEWS & UPDATES");
        ImGui::PopFont();
        
        ImGui::SetCursorPos(ImVec2(40, 70));
        
        // Pinned news
        ImGui::BeginChild("##pinned_news", ImVec2(700, 150), true);
        ImGui::TextColored(theme::primary, "📌 PINNED");
        ImGui::Spacing();
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
        ImGui::TextColored(theme::text, "▓▓▓ MAJOR UPDATE v2.0 RELEASED ▓▓▓");
        ImGui::PopFont();
        ImGui::TextColored(theme::text_secondary, "Dec 25, 2025");
        ImGui::Spacing();
        ImGui::TextColored(theme::text, "What's new:");
        ImGui::BulletText("Complete launcher redesign");
        ImGui::BulletText("New ESP features");
        ImGui::BulletText("Improved anti-detection");
        ImGui::BulletText("WebSocket real-time updates");
        ImGui::EndChild();
        
        ImGui::SetCursorPosX(40);
        ImGui::Spacing();
        
        // Recent updates
        ImGui::BeginChild("##recent_news", ImVec2(700, 100), true);
        ImGui::TextColored(theme::text, "CS2 Game Update");
        ImGui::TextColored(theme::text_secondary, "2 hours ago");
        ImGui::Spacing();
        ImGui::TextWrapped("New offsets available for CS2 v1.40.2.3. Auto-updated.");
        theme::StatusIndicator("Ready", true, theme::success);
        ImGui::EndChild();
        
        ImGui::EndChild();
    }
}

