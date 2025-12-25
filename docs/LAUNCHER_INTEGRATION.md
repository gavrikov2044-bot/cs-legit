# LAUNCHER INTEGRATION GUIDE

This guide explains how to integrate the new UI system into main.cpp

## Step 1: Add includes at top of main.cpp

```cpp
#include "modern_theme.hpp"  // After imgui includes
#include "new_ui.hpp"        // After modern_theme.hpp
```

## Step 2: In InitImGui() or startup, apply theme:

```cpp
void InitImGui() {
    // ... existing ImGui init code ...
    
    // Apply modern theme
    theme::ApplyModernTheme();
    
    // Initialize particles
    ui::InitParticles(30);
    
    // ... rest of init ...
}
```

## Step 3: In main render loop, update particles:

```cpp
void RenderUI() {
    float dt = ImGui::GetIO().DeltaTime;
    
    // Update particles
    ui::UpdateParticles(dt);
    
    // ... rest of rendering ...
}
```

## Step 4: Replace main window rendering with new UI:

```cpp
void RenderMainWindow() {
    ImGui::SetNextWindowSize(ImVec2(1200, 700), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    
    ImGui::Begin("EXTERNA", nullptr, 
        ImGuiWindowFlags_NoTitleBar | 
        ImGuiWindowFlags_NoResize);
    
    // Get draw list for particles
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    
    // Render particles first (background)
    ui::RenderParticles(draw, windowPos);
    
    // Top bar
    ui::RenderTopBar(g_username[0] ? g_username : "Guest");
    
    // Main layout
    ImGui::BeginGroup();
    {
        // Sidebar
        ui::RenderSidebar();
        
        ImGui::SameLine();
        
        // Content area based on selected tab
        switch (ui::g_currentTab) {
            case ui::Tab::Home:
                ui::RenderHomeTab({});
                break;
            case ui::Tab::Games:
                ui::RenderGamesTab();
                break;
            case ui::Tab::Stats:
                ui::RenderStatsTab();
                break;
            case ui::Tab::Settings:
                ui::RenderSettingsTab();
                break;
            case ui::Tab::Profile:
                ui::RenderProfileTab(g_username);
                break;
            case ui::Tab::News:
                ui::RenderNewsTab();
                break;
        }
    }
    ImGui::EndGroup();
    
    ImGui::End();
}
```

## Step 5: Update window size to 1200x700:

```cpp
#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 700
```

## Step 6: Keep existing Toast system, integrate with theme colors:

The new theme colors can be used in ShowToast:

```cpp
void ShowToast(const std::string& message, bool isError = false) {
    Toast toast;
    toast.message = message;
    toast.color = isError ? theme::error : theme::success;
    toast.lifetime = 3.0f;
    toast.alpha = 1.0f;
    toast.isError = isError;
    g_toasts.push_back(toast);
}
```

## Complete integration will:

1. ✅ Apply modern theme
2. ✅ Initialize particle system
3. ✅ Render animated background
4. ✅ Show tab-based navigation
5. ✅ Display all 6 tabs
6. ✅ Use theme colors throughout
7. ✅ Keep existing functionality (login, API calls, etc.)

## Benefits:

- Professional modern UI
- Smooth animations
- Organized tab structure
- Better user experience
- Matches design mockup

## The old UI code can be gradually removed as features are migrated to the new tab system.

