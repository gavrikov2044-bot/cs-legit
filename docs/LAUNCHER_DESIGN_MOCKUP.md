# 🎨 EXTERNA LAUNCHER - UI DESIGN MOCKUP

**Version:** 2.0  
**Style:** Dark Future (Cyberpunk)  
**Date:** December 25, 2025

---

## 🎯 **DESIGN CONCEPT**

**Name:** "STEALTH NEXUS"

**Visual Identity:**
- Dark, mysterious, high-tech
- Purple & Cyan accents (hacker aesthetic)
- Smooth animations
- Particle effects
- Professional yet edgy

---

## 🎨 **COLOR SCHEME**

```css
/* Background Layers */
--bg-deep:      #0a0a0f    /* Main background */
--bg-card:      #12121a    /* Card containers */
--bg-hover:     #1a1a24    /* Hover states */
--bg-input:     #1e1e28    /* Input fields */

/* Accent Colors */
--primary:      #8a2be2    /* Purple (main) */
--secondary:    #00ffff    /* Cyan (highlights) */
--success:      #00ff88    /* Green (operational) */
--warning:      #ffa500    /* Orange (updating) */
--error:        #ff3c3c    /* Red (offline) */

/* Text */
--text-primary:   #ffffff
--text-secondary: #a0a0b0
--text-disabled:  #606070
```

---

## 📐 **LAYOUT STRUCTURE**

### **Main Window (1200x700px)**

```
┌────────────────────────────────────────────────────────────┐
│  ╔════════════════════════════════════════════════════╗  │ ← Top Bar (60px)
│  ║  [≡]  ▓▓▓ EXTERNA ▓▓▓     [@USER]  [⚙️]  [—][□][×] ║  │
│  ╚════════════════════════════════════════════════════╝  │
├──────┬─────────────────────────────────────────────────────┤
│      │                                                     │
│  🏠  │  ╔═══════════════════════════════════════════════╗ │
│ HOME │  ║                                               ║ │
│      │  ║         🎮 DASHBOARD                         ║ │
│  🎮  │  ║                                               ║ │
│ GAMES│  ║  ┌─────────────┐  ┌─────────────┐           ║ │
│      │  ║  │   CS2       │  │  VALORANT   │           ║ │
│  📊  │  ║  │ ✅ READY    │  │ 🔴 OFFLINE  │           ║ │
│ STATS│  ║  │ v2.0.59     │  │ Coming Soon │           ║ │
│      │  ║  │ [▶ LAUNCH]  │  │ [🔔 NOTIFY] │           ║ │
│  ⚙️  │  ║  └─────────────┘  └─────────────┘           ║ │
│SETTNGS│  ║                                               ║ │
│      │  ║  📰 LATEST NEWS                              ║ │
│  👤  │  ║  • CS2 Update detected                       ║ │
│ PROF │  ║  • New offsets v2.0.59 ready                ║ │
│      │  ║  • Anti-cheat bypass improved                ║ │
│  📰  │  ║                                               ║ │
│ NEWS │  ║                                               ║ │
│      │  ╚═══════════════════════════════════════════════╝ │
│      │                                                     │
└──────┴─────────────────────────────────────────────────────┘
 80px              Main Content (1120x640px)
```

---

## 🎨 **TAB VIEWS**

### **1. 🏠 HOME / DASHBOARD**

```
┌──────────────────────────────────────────────────────┐
│  📊 QUICK STATUS                                     │
│  ┌──────────┬──────────┬──────────┬──────────┐     │
│  │ 🎮 Games │ ✅ Ready │ 🔴 Down │ 🟠 Updating│     │
│  │    4     │    3     │    0    │     1      │     │
│  └──────────┴──────────┴──────────┴──────────┘     │
│                                                      │
│  🎮 YOUR GAMES                                       │
│  ┌─────────────────┐  ┌─────────────────┐          │
│  │ ╔══════════════╗│  │ ╔══════════════╗│          │
│  │ ║   [CS2 IMG]  ║│  │ ║ [VAL IMG]    ║│          │
│  │ ╚══════════════╝│  │ ╚══════════════╝│          │
│  │ Counter-Strike 2│  │ VALORANT       │          │
│  │ ✅ OPERATIONAL  │  │ 🔴 OFFLINE     │          │
│  │ Version 2.0.59  │  │ Coming Soon    │          │
│  │ Last: 2 min ago │  │ Expected: Q1   │          │
│  │                 │  │                │          │
│  │ [▶ LAUNCH]      │  │ [🔔 NOTIFY ME] │          │
│  └─────────────────┘  └─────────────────┘          │
│                                                      │
│  📰 ACTIVITY FEED                                    │
│  • 10:42 - CS2 cheat injected successfully          │
│  • 10:15 - Offsets updated to v2.0.59               │
│  • 09:30 - CS2 game update detected                 │
│  • Yesterday - New ESP feature: Sound ESP           │
└──────────────────────────────────────────────────────┘
```

**Features:**
- Quick status overview
- Game cards with thumbnails
- Status indicators (pulsating)
- Launch buttons
- Activity feed

---

### **2. 🎮 GAMES**

```
┌──────────────────────────────────────────────────────┐
│  🎮 AVAILABLE GAMES                                  │
│                                                      │
│  ┌────────────────────────────────────────┐         │
│  │ Counter-Strike 2                       │         │
│  │ ┌────────┬───────────────────────────┐ │         │
│  │ │ [IMG]  │ Status: ✅ OPERATIONAL    │ │         │
│  │ │  CS2   │ Version: 2.0.59           │ │         │
│  │ │        │ Game: CS2 v1.40.2.3       │ │         │
│  │ └────────┤ Offsets: Up to date       │ │         │
│  │          │ Last Update: 2 min ago    │ │         │
│  │          │                           │ │         │
│  │          │ FEATURES:                 │ │         │
│  │          │ ✅ Box ESP                │ │         │
│  │          │ ✅ Name ESP               │ │         │
│  │          │ ✅ Health Bars            │ │         │
│  │          │ ✅ Weapon Info            │ │         │
│  │          │ ✅ Distance               │ │         │
│  │          │ ✅ Skeleton               │ │         │
│  │          │ ✅ Aimbot                 │ │         │
│  │          │ ✅ Triggerbot             │ │         │
│  │          │                           │ │         │
│  │          │ [▶ LAUNCH CHEAT]          │ │         │
│  │          │ [⚙️ CONFIGURE]            │ │         │
│  │          └───────────────────────────┘ │         │
│  └────────────────────────────────────────┘         │
│                                                      │
│  ┌────────────────────────────────────────┐         │
│  │ VALORANT                               │         │
│  │ ┌────────┬───────────────────────────┐ │         │
│  │ │ [IMG]  │ Status: 🔴 OFFLINE        │ │         │
│  │ │  VAL   │ Version: Coming Soon      │ │         │
│  │ │        │ Expected: Q1 2026         │ │         │
│  │ └────────┤                           │ │         │
│  │          │ [🔔 NOTIFY ME]            │ │         │
│  │          └───────────────────────────┘ │         │
│  └────────────────────────────────────────┘         │
└──────────────────────────────────────────────────────┘
```

**Features:**
- Detailed game info
- Feature list for each game
- Status with real-time updates
- Launch & configure buttons

---

### **3. 📊 STATISTICS**

```
┌──────────────────────────────────────────────────────┐
│  📊 YOUR STATS                                       │
│                                                      │
│  ┌─────────────────────────────────────┐            │
│  │ USAGE STATISTICS                    │            │
│  │                                     │            │
│  │ Total Sessions:  127                │            │
│  │ Total Playtime:  43h 25m            │            │
│  │ Avg Session:     20m 30s            │            │
│  │ Last Session:    2 minutes ago      │            │
│  │                                     │            │
│  │ [═══════════════════════] 100%      │            │
│  │ CS2: 43h 25m                        │            │
│  │                                     │            │
│  └─────────────────────────────────────┘            │
│                                                      │
│  ┌─────────────────────────────────────┐            │
│  │ DETECTION STATUS                    │            │
│  │                                     │            │
│  │ ✅ No bans detected                 │            │
│  │ ✅ Clean HWID                       │            │
│  │ ✅ Anti-cheat bypass: ACTIVE        │            │
│  │ 🔒 Protection level: HIGH           │            │
│  │                                     │            │
│  │ Last scan: Just now                 │            │
│  └─────────────────────────────────────┘            │
│                                                      │
│  ┌─────────────────────────────────────┐            │
│  │ PERFORMANCE                         │            │
│  │                                     │            │
│  │ Avg FPS:     240                    │            │
│  │ Injection:   <500ms                 │            │
│  │ Memory:      32MB                   │            │
│  │ CPU Usage:   <2%                    │            │
│  └─────────────────────────────────────┘            │
└──────────────────────────────────────────────────────┘
```

---

### **4. ⚙️ SETTINGS**

```
┌──────────────────────────────────────────────────────┐
│  ⚙️ SETTINGS                                          │
│                                                      │
│  LAUNCHER                                            │
│  ☑ Start with Windows                               │
│  ☑ Minimize to tray                                 │
│  ☑ Check for updates automatically                  │
│  ☐ Show notifications                               │
│                                                      │
│  INJECTION                                           │
│  Method: ○ Manual  ● Auto  ○ Manual Mapping         │
│  ☑ Inject on game launch                            │
│  ☑ Hide from task manager                           │
│  Delay: [═══░░] 5 seconds                           │
│                                                      │
│  APPEARANCE                                          │
│  Theme: ▼ Dark Future                               │
│  Accent: [🟣]  [🔵]  [🟢]  [🔴]                      │
│  Opacity: [═════░] 95%                              │
│  ☑ Enable animations                                │
│  ☑ Show particle effects                            │
│                                                      │
│  UPDATES                                             │
│  Update Channel: ▼ Stable                           │
│  ☑ Auto-download offsets                            │
│  ☑ Backup old versions                              │
│                                                      │
│  [💾 SAVE SETTINGS]  [🔄 RESET]                     │
└──────────────────────────────────────────────────────┘
```

---

### **5. 👤 PROFILE**

```
┌──────────────────────────────────────────────────────┐
│  👤 PROFILE                                           │
│                                                      │
│  ┌────────┬───────────────────────────────┐         │
│  │ [AVATAR]│ Username: @YourName          │         │
│  │  [PIC]  │ Member Since: Dec 2025       │         │
│  │         │ Plan: Premium                │         │
│  └────────┤ HWID: ****-****-****-1234     │         │
│            │                               │         │
│            │ [🔄 CHANGE AVATAR]            │         │
│            └───────────────────────────────┘         │
│                                                      │
│  LICENSES                                            │
│  ┌─────────────────────────────────────┐            │
│  │ CS2 Premium                         │            │
│  │ Status: ✅ ACTIVE                   │            │
│  │ Expires: Never (Lifetime)           │            │
│  │ Key: CS2-XXXX-XXXX-XXXX             │            │
│  └─────────────────────────────────────┘            │
│                                                      │
│  ┌─────────────────────────────────────┐            │
│  │ VALORANT                            │            │
│  │ Status: 🔴 NOT ACTIVATED            │            │
│  │ [➕ ACTIVATE LICENSE]               │            │
│  └─────────────────────────────────────┘            │
│                                                      │
│  SECURITY                                            │
│  ☑ 2FA Enabled                                      │
│  Last Login: 2 minutes ago                          │
│  Last IP: 192.168.1.xxx                             │
│                                                      │
│  [🔒 CHANGE PASSWORD]  [🚪 LOGOUT]                  │
└──────────────────────────────────────────────────────┘
```

---

### **6. 📰 NEWS & UPDATES**

```
┌──────────────────────────────────────────────────────┐
│  📰 NEWS & UPDATES                                    │
│                                                      │
│  ┌────────────────────────────────────────┐         │
│  │ 📌 PINNED                              │         │
│  │ ▓▓▓ MAJOR UPDATE v2.0 RELEASED ▓▓▓    │         │
│  │ Dec 25, 2025                           │         │
│  │                                        │         │
│  │ What's new:                            │         │
│  │ • Complete launcher redesign           │         │
│  │ • New ESP features                     │         │
│  │ • Improved anti-detection              │         │
│  │ • WebSocket real-time updates          │         │
│  │                                        │         │
│  │ [📖 READ MORE]                         │         │
│  └────────────────────────────────────────┘         │
│                                                      │
│  ┌────────────────────────────────────────┐         │
│  │ CS2 Game Update                        │         │
│  │ 2 hours ago                            │         │
│  │                                        │         │
│  │ New offsets available for CS2         │         │
│  │ v1.40.2.3. Auto-updated.               │         │
│  │                                        │         │
│  │ Status: ✅ Ready                       │         │
│  └────────────────────────────────────────┘         │
│                                                      │
│  ┌────────────────────────────────────────┐         │
│  │ New Feature: Sound ESP                 │         │
│  │ Yesterday                              │         │
│  │                                        │         │
│  │ Visualize enemy footsteps and sounds!  │         │
│  │ Available in CS2 cheat now.            │         │
│  └────────────────────────────────────────┘         │
└──────────────────────────────────────────────────────┘
```

---

## 🎨 **COMPONENT DETAILS**

### **Top Bar**

```
┌──────────────────────────────────────────────────────┐
│ [≡] ▓▓▓ EXTERNA ▓▓▓      [@USER] [⚙️] [—] [□] [×]   │
│  │   └─Logo (animated)     │     │   │   │   └─Close│
│  │                         │     │   │   └─Maximize │
│  │                         │     │   └─Minimize     │
│  │                         │     └─Settings         │
│  │                         └─User menu              │
│  └─Menu toggle                                      │
└──────────────────────────────────────────────────────┘
```

**Features:**
- Animated logo (glow pulse)
- User dropdown (profile, logout)
- Window controls
- Draggable

---

### **Sidebar**

```
┌─────────┐
│  🏠     │ ← Home (active: purple glow)
│  HOME   │
├─────────┤
│  🎮     │ ← Games
│  GAMES  │
├─────────┤
│  📊     │ ← Stats
│  STATS  │
├─────────┤
│  ⚙️     │ ← Settings
│ SETTINGS│
├─────────┤
│  👤     │ ← Profile
│ PROFILE │
├─────────┤
│  📰     │ ← News
│  NEWS   │
└─────────┘
  80px width
```

**Features:**
- Icon + text
- Hover: slight expand + glow
- Active: purple left border + brighter
- Smooth transitions

---

### **Game Card**

```
┌─────────────────────────┐
│ ╔═══════════════════╗   │
│ ║                   ║   │ ← Game thumbnail
│ ║   [GAME IMAGE]    ║   │
│ ║                   ║   │
│ ╚═══════════════════╝   │
│                         │
│ Counter-Strike 2        │ ← Title
│ ✅ OPERATIONAL          │ ← Status (pulsing dot)
│ Version 2.0.59          │ ← Version
│ Last: 2 minutes ago     │ ← Timestamp
│                         │
│ [▶ LAUNCH CHEAT]        │ ← Primary button
│ [⚙️ CONFIGURE]          │ ← Secondary button
└─────────────────────────┘
```

**Animations:**
- Hover: Slight scale (1.02x) + glow
- Click: Scale down (0.98x) then up
- Status dot: Pulsing animation

---

### **Status Indicator**

```
🟢 ● Operational   ← Pulsing green
🟠 ● Updating      ← Pulsing orange
🔴 ● Offline       ← Static red
⚪ ● Coming Soon   ← Static gray
```

---

### **Buttons**

**Primary:**
```
┌─────────────────┐
│  ▶ LAUNCH       │  bg: #8a2be2, hover: lighter
└─────────────────┘
```

**Secondary:**
```
┌─────────────────┐
│  ⚙️ CONFIGURE   │  bg: transparent, border: #8a2be2
└─────────────────┘
```

---

## ✨ **ANIMATIONS**

### **1. Startup Animation**

```
1. Logo fades in (0.5s)
2. Logo glows (pulse 2s)
3. Sidebar slides in from left (0.3s)
4. Content fades in (0.4s)
5. Particles start floating
```

### **2. Tab Switch**

```
1. Current content fades out (0.15s)
2. New content fades in (0.15s)
3. Active tab indicator slides
```

### **3. Status Updates**

```
When status changes (WebSocket):
1. Status dot pulses 3x fast
2. Card border flashes
3. Toast notification appears
```

---

## 🎬 **SPECIAL EFFECTS**

### **Particle Background**

```
- 30-50 purple particles
- Floating upward
- Random spawn positions
- Fade in/out
- Blur effect
```

### **Glow Effects**

```
Active elements:
- Button hover: 0 0 20px rgba(138,43,226,0.5)
- Status online: 0 0 15px rgba(0,255,136,0.6)
- Logo: 0 0 30px rgba(138,43,226,0.7)
```

---

## 🚀 **IMPLEMENTATION NOTES**

### **ImGui Custom Theme:**

```cpp
// Colors
ImVec4 bg_primary   = ImVec4(0.04f, 0.04f, 0.06f, 1.00f);
ImVec4 accent       = ImVec4(0.54f, 0.17f, 0.89f, 1.00f);
ImVec4 accent_hover = ImVec4(0.64f, 0.27f, 0.99f, 1.00f);

// Apply theme
style.Colors[ImGuiCol_WindowBg] = bg_primary;
style.Colors[ImGuiCol_Button] = accent;
style.Colors[ImGuiCol_ButtonHovered] = accent_hover;

// Rounding
style.WindowRounding = 12.0f;
style.FrameRounding = 8.0f;
style.GrabRounding = 8.0f;
```

### **Custom Rendering:**

```cpp
// Particle system
void RenderParticles() {
    for (auto& p : particles) {
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        draw->AddCircleFilled(
            ImVec2(p.x, p.y),
            p.size,
            ImColor(138, 43, 226, p.alpha)
        );
    }
}

// Glow effect
void DrawGlow(ImVec2 pos, float radius, ImU32 color) {
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    for (int i = 0; i < 5; i++) {
        draw->AddCircle(pos, radius + i*2, color, 32, i+1);
    }
}
```

---

## 📐 **RESPONSIVE BEHAVIOR**

**Minimum Size:** 1000x600px

**Sidebar Collapse:**
```
When width < 1100px:
- Sidebar shows only icons
- Tooltip on hover
- Width: 60px → 80px
```

---

## 🎯 **NEXT STEPS**

1. ✅ Design mockup complete
2. ⏳ Implement ImGui custom theme
3. ⏳ Create particle system
4. ⏳ Build tab system
5. ⏳ Add animations
6. ⏳ Integrate with backend

---

**This is the target design!** 🎨✨

The launcher will look professional, modern, and match the cyberpunk aesthetic of the brand.

