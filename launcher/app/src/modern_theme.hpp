/*
 * EXTERNA LAUNCHER - MODERN THEME
 * "Dark Future" / "Stealth Nexus" Design
 * Version 2.0
 */

#pragma once
#include "imgui.h"
#include <cmath>

namespace theme {
    // ============================================
    // Color Palette
    // ============================================
    
    // Background Layers
    constexpr ImVec4 bg_deep      = ImVec4(0.04f, 0.04f, 0.06f, 1.00f); // #0a0a0f
    constexpr ImVec4 bg_card      = ImVec4(0.07f, 0.07f, 0.10f, 1.00f); // #12121a
    constexpr ImVec4 bg_hover     = ImVec4(0.10f, 0.10f, 0.14f, 1.00f); // #1a1a24
    constexpr ImVec4 bg_input     = ImVec4(0.12f, 0.12f, 0.16f, 1.00f); // #1e1e28
    
    // Accent Colors
    constexpr ImVec4 primary      = ImVec4(0.54f, 0.17f, 0.89f, 1.00f); // #8a2be2 Purple
    constexpr ImVec4 primary_hover= ImVec4(0.64f, 0.27f, 0.99f, 1.00f); // Lighter purple
    constexpr ImVec4 primary_active=ImVec4(0.44f, 0.07f, 0.79f, 1.00f); // Darker purple
    
    constexpr ImVec4 secondary    = ImVec4(0.00f, 1.00f, 1.00f, 1.00f); // #00ffff Cyan
    constexpr ImVec4 success      = ImVec4(0.00f, 1.00f, 0.53f, 1.00f); // #00ff88 Green
    constexpr ImVec4 warning      = ImVec4(1.00f, 0.65f, 0.00f, 1.00f); // #ffa500 Orange
    constexpr ImVec4 error        = ImVec4(1.00f, 0.24f, 0.24f, 1.00f); // #ff3c3c Red
    
    // Text Colors
    constexpr ImVec4 text         = ImVec4(1.00f, 1.00f, 1.00f, 1.00f); // #ffffff
    constexpr ImVec4 text_secondary=ImVec4(0.63f, 0.63f, 0.69f, 1.00f); // #a0a0b0
    constexpr ImVec4 text_disabled= ImVec4(0.38f, 0.38f, 0.44f, 1.00f); // #606070
    constexpr ImVec4 text_accent  = ImVec4(0.54f, 0.17f, 0.89f, 1.00f); // #8a2be2
    
    // ============================================
    // Backwards Compatibility Aliases (for old RenderMainOld)
    // ============================================
    constexpr ImVec4 bg           = bg_deep;
    constexpr ImVec4 sidebar      = ImVec4(0.06f, 0.06f, 0.09f, 1.0f);
    constexpr ImVec4 surface      = ImVec4(0.08f, 0.08f, 0.12f, 1.0f);
    constexpr ImVec4 surfaceHover = ImVec4(0.12f, 0.12f, 0.18f, 1.0f);
    constexpr ImVec4 border       = ImVec4(0.15f, 0.15f, 0.22f, 1.0f);
    constexpr ImVec4 accent       = primary;
    constexpr ImVec4 accentHover  = primary_hover;
    constexpr ImVec4 textSec      = text_secondary;
    constexpr ImVec4 textDim      = text_disabled;
    constexpr ImU32  gradientA    = IM_COL32(140, 90, 245, 255);
    constexpr ImU32  gradientB    = IM_COL32(60, 180, 255, 255);
    
    // ============================================
    // Apply Modern Theme
    // ============================================
    inline void ApplyModernTheme() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;
        
        // Window
        colors[ImGuiCol_WindowBg] = bg_deep;
        colors[ImGuiCol_ChildBg] = bg_card;
        colors[ImGuiCol_PopupBg] = bg_card;
        
        // Border
        colors[ImGuiCol_Border] = ImVec4(primary.x, primary.y, primary.z, 0.3f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
        
        // Frame (inputs, etc.)
        colors[ImGuiCol_FrameBg] = bg_input;
        colors[ImGuiCol_FrameBgHovered] = bg_hover;
        colors[ImGuiCol_FrameBgActive] = bg_hover;
        
        // Title
        colors[ImGuiCol_TitleBg] = bg_card;
        colors[ImGuiCol_TitleBgActive] = primary;
        colors[ImGuiCol_TitleBgCollapsed] = bg_card;
        
        // Scrollbar
        colors[ImGuiCol_ScrollbarBg] = bg_card;
        colors[ImGuiCol_ScrollbarGrab] = primary;
        colors[ImGuiCol_ScrollbarGrabHovered] = primary_hover;
        colors[ImGuiCol_ScrollbarGrabActive] = primary_active;
        
        // Slider
        colors[ImGuiCol_SliderGrab] = primary;
        colors[ImGuiCol_SliderGrabActive] = primary_hover;
        
        // Button
        colors[ImGuiCol_Button] = primary;
        colors[ImGuiCol_ButtonHovered] = primary_hover;
        colors[ImGuiCol_ButtonActive] = primary_active;
        
        // Header
        colors[ImGuiCol_Header] = primary;
        colors[ImGuiCol_HeaderHovered] = primary_hover;
        colors[ImGuiCol_HeaderActive] = primary_active;
        
        // Separator
        colors[ImGuiCol_Separator] = ImVec4(primary.x, primary.y, primary.z, 0.3f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(primary.x, primary.y, primary.z, 0.5f);
        colors[ImGuiCol_SeparatorActive] = primary;
        
        // Tab
        colors[ImGuiCol_Tab] = bg_card;
        colors[ImGuiCol_TabHovered] = bg_hover;
        colors[ImGuiCol_TabActive] = primary;
        colors[ImGuiCol_TabUnfocused] = bg_card;
        colors[ImGuiCol_TabUnfocusedActive] = bg_hover;
        
        // CheckMark
        colors[ImGuiCol_CheckMark] = success;
        
        // Text
        colors[ImGuiCol_Text] = text;
        colors[ImGuiCol_TextDisabled] = text_disabled;
        colors[ImGuiCol_TextSelectedBg] = ImVec4(primary.x, primary.y, primary.z, 0.3f);
        
        // Rounding
        style.WindowRounding = 12.0f;
        style.ChildRounding = 8.0f;
        style.FrameRounding = 8.0f;
        style.PopupRounding = 8.0f;
        style.ScrollbarRounding = 8.0f;
        style.GrabRounding = 8.0f;
        style.TabRounding = 8.0f;
        
        // Spacing
        style.WindowPadding = ImVec2(16.0f, 16.0f);
        style.FramePadding = ImVec2(12.0f, 8.0f);
        style.ItemSpacing = ImVec2(12.0f, 8.0f);
        style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
        style.IndentSpacing = 20.0f;
        style.ScrollbarSize = 14.0f;
        style.GrabMinSize = 10.0f;
        
        // Borders
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.TabBorderSize = 0.0f;
    }
    
    // ============================================
    // Custom Widgets
    // ============================================
    
    // Status Indicator with pulsing animation
    inline void StatusIndicator(const char* label, bool active, ImVec4 color) {
        static float pulse = 0.0f;
        pulse += 0.05f;
        if (pulse > 6.28f) pulse = 0.0f;
        
        float alpha = active ? (0.7f + 0.3f * sinf(pulse)) : 0.3f;
        ImVec4 dotColor = ImVec4(color.x, color.y, color.z, alpha);
        
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        
        // Draw pulsing dot
        float dotRadius = 6.0f;
        if (active) {
            // Outer glow
            draw->AddCircleFilled(
                ImVec2(pos.x + dotRadius, pos.y + 10),
                dotRadius + 2.0f * alpha,
                ImGui::ColorConvertFloat4ToU32(ImVec4(dotColor.x, dotColor.y, dotColor.z, alpha * 0.3f)),
                16
            );
        }
        // Inner dot
        draw->AddCircleFilled(
            ImVec2(pos.x + dotRadius, pos.y + 10),
            dotRadius,
            ImGui::ColorConvertFloat4ToU32(dotColor),
            16
        );
        
        ImGui::SetCursorScreenPos(ImVec2(pos.x + dotRadius * 3, pos.y));
        ImGui::TextColored(active ? text : text_disabled, "%s", label);
    }
    
    // Game Card
    inline bool GameCard(const char* title, const char* status, const char* version, bool available, bool selected) {
        ImVec2 size(260, 180);
        ImGui::PushID(title);
        
        // Card background
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        
        ImVec4 cardBg = selected ? bg_hover : bg_card;
        ImU32 cardColor = ImGui::ColorConvertFloat4ToU32(cardBg);
        ImU32 borderColor = selected ? 
            ImGui::ColorConvertFloat4ToU32(primary) :
            ImGui::ColorConvertFloat4ToU32(ImVec4(primary.x, primary.y, primary.z, 0.2f));
        
        // Card with border
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), cardColor, 12.0f);
        draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), borderColor, 12.0f, 0, 2.0f);
        
        // Glow effect when selected
        if (selected) {
            draw->AddRect(
                ImVec2(pos.x - 2, pos.y - 2),
                ImVec2(pos.x + size.x + 2, pos.y + size.y + 2),
                ImGui::ColorConvertFloat4ToU32(ImVec4(primary.x, primary.y, primary.z, 0.3f)),
                12.0f, 0, 3.0f
            );
        }
        
        bool clicked = ImGui::InvisibleButton("##card", size);
        bool hovered = ImGui::IsItemHovered();
        
        // Content
        ImGui::SetCursorScreenPos(ImVec2(pos.x + 20, pos.y + 20));
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]); // Assuming bold font
        ImGui::TextColored(text, "%s", title);
        ImGui::PopFont();
        
        ImGui::SetCursorScreenPos(ImVec2(pos.x + 20, pos.y + 60));
        ImVec4 statusColor = available ? success : text_disabled;
        StatusIndicator(status, available, statusColor);
        
        ImGui::SetCursorScreenPos(ImVec2(pos.x + 20, pos.y + 90));
        ImGui::TextColored(text_secondary, "Version: %s", version);
        
        // Launch button (if available)
        if (available) {
            ImGui::SetCursorScreenPos(ImVec2(pos.x + 20, pos.y + size.y - 50));
            ImGui::PushStyleColor(ImGuiCol_Button, primary);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, primary_hover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, primary_active);
            if (ImGui::Button("▶ LAUNCH", ImVec2(size.x - 40, 30))) {
                ImGui::PopStyleColor(3);
                ImGui::PopID();
                return true; // Launch clicked
            }
            ImGui::PopStyleColor(3);
        }
        
        ImGui::SetCursorScreenPos(ImVec2(pos.x + size.x + 20, pos.y)); // Move cursor for next card
        
        ImGui::PopID();
        return clicked && !available; // Return if card clicked but not launch
    }
    
    // Progress Bar with gradient
    inline void ProgressBar(float fraction, const ImVec2& size, const char* label = nullptr) {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        
        // Background
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), 
            ImGui::ColorConvertFloat4ToU32(bg_input), 4.0f);
        
        // Progress (gradient)
        if (fraction > 0.0f) {
            float width = size.x * fraction;
            ImU32 col_a = ImGui::ColorConvertFloat4ToU32(primary);
            ImU32 col_b = ImGui::ColorConvertFloat4ToU32(secondary);
            draw->AddRectFilledMultiColor(
                pos,
                ImVec2(pos.x + width, pos.y + size.y),
                col_a, col_b, col_b, col_a
            );
        }
        
        // Label
        if (label) {
            ImVec2 text_size = ImGui::CalcTextSize(label);
            ImVec2 text_pos = ImVec2(
                pos.x + (size.x - text_size.x) * 0.5f,
                pos.y + (size.y - text_size.y) * 0.5f
            );
            draw->AddText(text_pos, ImGui::ColorConvertFloat4ToU32(text), label);
        }
        
        ImGui::Dummy(size);
    }
    
    // Animated Spinner (simplified - no internal API)
    inline void Spinner(const char* label, float radius, float thickness, ImU32 color) {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        
        // Animated rotation
        static float time = 0.0f;
        time += ImGui::GetIO().DeltaTime * 8.0f;
        
        // Draw arc
        int num_segments = 30;
        float a_min = time;
        float a_max = time + 3.14159f * 1.5f; // 270 degrees
        
        ImVec2 centre(pos.x + radius, pos.y + radius);
        
        draw->PathClear();
        for (int i = 0; i <= num_segments; i++) {
            float a = a_min + ((float)i / (float)num_segments) * (a_max - a_min);
            draw->PathLineTo(ImVec2(
                centre.x + cosf(a) * radius,
                centre.y + sinf(a) * radius
            ));
        }
        draw->PathStroke(color, 0, thickness);
        
        ImGui::Dummy(ImVec2(radius * 2, radius * 2));
    }
}

