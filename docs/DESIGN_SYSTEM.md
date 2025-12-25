# 🎨 EXTERNA PROJECT - UNIFIED DESIGN SYSTEM

**Version:** 1.0  
**Date:** December 25, 2025  
**Status:** Official Design Guide

---

## 🎯 **DESIGN PHILOSOPHY**

**"STEALTH MEETS ELEGANCE"**

- Dark, modern, professional
- Cybersecurity aesthetic
- High contrast for readability
- Smooth, subtle animations
- Performance-first

---

## 🎨 **COLOR PALETTE**

### **Primary Colors:**

```css
/* Background Layers */
--bg-primary:    #0a0a0f    /* Deep black */
--bg-secondary:  #12121a    /* Card background */
--bg-tertiary:   #1a1a24    /* Hover state */

/* Accent Colors */
--accent-purple: #8a2be2    /* Primary accent */
--accent-cyan:   #00ffff    /* Secondary accent */
--accent-green:  #00ff88    /* Success/Online */
--accent-red:    #ff3c3c    /* Error/Offline */
--accent-orange: #ffa500    /* Warning/Updating */

/* Text Colors */
--text-primary:   #ffffff   /* Main text */
--text-secondary: #a0a0b0   /* Secondary text */
--text-tertiary:  #606070   /* Disabled text */
--text-accent:    #8a2be2   /* Highlighted text */
```

### **Color Usage:**

| Element | Color | Usage |
|---------|-------|-------|
| Background | `#0a0a0f` | Main app background |
| Cards | `#12121a` | Content containers |
| Buttons (Primary) | `#8a2be2` | Main actions |
| Buttons (Secondary) | `#1a1a24` | Secondary actions |
| Success | `#00ff88` | Status: Operational |
| Warning | `#ffa500` | Status: Updating |
| Error | `#ff3c3c` | Status: Offline |
| Links | `#00ffff` | Clickable links |

---

## 📝 **TYPOGRAPHY**

### **Font Families:**

```css
/* Headers */
--font-heading: 'Orbitron', 'Arial Black', sans-serif;
  Font-weight: 700-900
  Usage: Titles, headings, logos

/* Body */
--font-body: 'Rajdhani', 'Segoe UI', sans-serif;
  Font-weight: 400-700
  Usage: Regular text, descriptions

/* Code */
--font-mono: 'Fira Code', 'Consolas', monospace;
  Usage: Code snippets, technical data
```

### **Font Sizes:**

```css
--text-xs:   0.75rem  /* 12px - Small labels */
--text-sm:   0.875rem /* 14px - Secondary text */
--text-base: 1rem     /* 16px - Body text */
--text-lg:   1.125rem /* 18px - Subheadings */
--text-xl:   1.25rem  /* 20px - Card titles */
--text-2xl:  1.5rem   /* 24px - Section headers */
--text-3xl:  2rem     /* 32px - Page titles */
--text-4xl:  2.5rem   /* 40px - Hero text */
```

---

## 🖼️ **LAYOUT SYSTEM**

### **Spacing Scale:**

```css
--space-1: 0.25rem  /* 4px */
--space-2: 0.5rem   /* 8px */
--space-3: 0.75rem  /* 12px */
--space-4: 1rem     /* 16px */
--space-5: 1.5rem   /* 24px */
--space-6: 2rem     /* 32px */
--space-8: 3rem     /* 48px */
--space-10: 4rem    /* 64px */
```

### **Border Radius:**

```css
--radius-sm: 4px    /* Small elements */
--radius-md: 8px    /* Buttons, inputs */
--radius-lg: 12px   /* Cards */
--radius-xl: 16px   /* Large containers */
--radius-full: 9999px /* Pills, badges */
```

---

## 🎭 **COMPONENT LIBRARY**

### **1. Buttons:**

```
┌─────────────────┐
│  PRIMARY BTN    │  bg: #8a2be2, hover: lighter
└─────────────────┘

┌─────────────────┐
│  SECONDARY BTN  │  bg: #1a1a24, border: #8a2be2
└─────────────────┘

🔘 ICON BTN         Square, icon only
```

**Specs:**
- Height: 40px
- Padding: 12px 24px
- Font: Rajdhani 600
- Transition: 0.2s ease

### **2. Cards:**

```
┌──────────────────────────────┐
│  ╔════════════════════════╗  │
│  ║                        ║  │  Shadow: 0 4px 20px rgba(0,0,0,0.3)
│  ║      CONTENT           ║  │  Border: 1px solid rgba(138,43,226,0.2)
│  ║                        ║  │  Radius: 12px
│  ╚════════════════════════╝  │
└──────────────────────────────┘
```

### **3. Status Indicators:**

```
🟢 OPERATIONAL   #00ff88  ← Pulsating glow
🟠 UPDATING      #ffa500
🔴 OFFLINE       #ff3c3c
⚪ COMING SOON   #606070
```

### **4. Progress Bars:**

```
┌────────────────────────────┐
│████████████░░░░░░░░░░░░░░░░│  75%
└────────────────────────────┘
  Gradient: #8a2be2 → #00ffff
```

---

## ✨ **ANIMATIONS**

### **Transitions:**

```css
/* Fast - UI feedback */
--transition-fast: 0.15s ease-out;

/* Base - Standard interactions */
--transition-base: 0.2s ease-in-out;

/* Slow - Complex animations */
--transition-slow: 0.3s ease-in-out;
```

### **Common Animations:**

**Fade In:**
```css
@keyframes fadeIn {
  from { opacity: 0; }
  to { opacity: 1; }
}
```

**Slide Up:**
```css
@keyframes slideUp {
  from { 
    opacity: 0;
    transform: translateY(20px);
  }
  to {
    opacity: 1;
    transform: translateY(0);
  }
}
```

**Glow Pulse:**
```css
@keyframes pulse {
  0%, 100% { box-shadow: 0 0 10px var(--accent-purple); }
  50% { box-shadow: 0 0 20px var(--accent-purple); }
}
```

---

## 🎮 **LAUNCHER DESIGN**

### **Layout:**

```
┌────────────────────────────────────────────┐
│  [LOGO] EXTERNA     [User]  [⚙️]  [⏻]    │ ← Top Bar (60px)
├──────┬─────────────────────────────────────┤
│      │                                     │
│  🏠  │  🎮 GAMES                           │
│      │                                     │
│  🎮  │  ╔══════════════╗ ╔══════════════╗  │
│      │  ║   CS2        ║ ║  VALORANT    ║  │
│  📊  │  ║ ✅ READY     ║ ║ 🔴 OFFLINE   ║  │
│      │  ║ v2.0.59      ║ ║ Coming Soon  ║  │
│  ⚙️  │  ║ [LAUNCH]     ║ ║ [NOTIFY ME]  ║  │
│      │  ╚══════════════╝ ╚══════════════╝  │
│  👤  │                                     │
│      │  📰 LATEST NEWS                     │
│  📰  │  • CS2 Update detected              │
│      │  • New offsets ready                │
│      │                                     │
└──────┴─────────────────────────────────────┘
  80px        Main Content Area
```

**Sidebar Icons:**
- Home: 🏠
- Games: 🎮
- Stats: 📊
- Settings: ⚙️
- Profile: 👤
- News: 📰

---

## 🌐 **WEBSITE DESIGN**

### **Hero Section:**

```
┌──────────────────────────────────────────┐
│                                          │
│         ▓▓▓ EXTERNA ▓▓▓                 │  ← Large animated logo
│                                          │
│    Next-Gen Game Enhancement Suite      │
│                                          │
│    [⬇️ DOWNLOAD LAUNCHER]               │
│                                          │
│  🎯 ESP  │  🔒 SECURE  │  ⚡ FAST       │
└──────────────────────────────────────────┘
  Animated particle background
  Gradient overlay: purple → cyan
```

### **Status Section:**

```
╔════════════════╗ ╔════════════════╗ ╔════════════════╗
║   LAUNCHER     ║ ║      CS2       ║ ║   VALORANT     ║
║  🟢 READY      ║ ║  🟠 UPDATING   ║ ║  ⚪ SOON       ║
║   v2.0.59      ║ ║   v1.0.3       ║ ║   v0.0.0       ║
╚════════════════╝ ╚════════════════╝ ╚════════════════╝
```

---

## 🎨 **CHEAT OVERLAY (ESP Menu)**

### **Menu Design:**

```
┌─────────────────────────────┐
│  ▓ EXTERNA ESP ▓  [×]       │  ← Header with close
├─────────────────────────────┤
│                             │
│  VISUALS ▼                  │  ← Collapsible sections
│  ├─ ☑ Box ESP               │
│  ├─ ☑ Name ESP              │
│  ├─ ☑ Health Bar            │
│  ├─ ☐ Distance               │
│  └─ ☐ Weapon                │
│                             │
│  COLORS ▼                   │
│  ├─ Enemy:  [🔴]            │  ← Color pickers
│  └─ Team:   [🔵]            │
│                             │
│  AIMBOT ▼                   │
│  ├─ ☐ Enable                │
│  ├─ FOV: [===░░] 90°        │  ← Sliders
│  └─ Smooth: [====░] 80%     │
│                             │
└─────────────────────────────┘
```

**Menu Style:**
- Background: `#12121a` with 95% opacity
- Border: 1px solid `#8a2be2`
- Glow: 0 0 20px `#8a2be2`
- Blur background behind menu
- Draggable header

---

## 🔧 **IMPLEMENTATION NOTES**

### **For Launcher (C++ ImGui):**

```cpp
ImGuiStyle& style = ImGui::GetStyle();

// Colors
style.Colors[ImGuiCol_WindowBg] = ImVec4(0.04f, 0.04f, 0.06f, 0.95f);
style.Colors[ImGuiCol_Button] = ImVec4(0.54f, 0.17f, 0.89f, 1.00f);
style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.64f, 0.27f, 0.99f, 1.00f);

// Rounding
style.WindowRounding = 12.0f;
style.FrameRounding = 8.0f;
style.GrabRounding = 8.0f;

// Spacing
style.WindowPadding = ImVec2(16, 16);
style.ItemSpacing = ImVec2(8, 8);
```

### **For Website (CSS):**

```css
:root {
  --bg-primary: #0a0a0f;
  --accent-purple: #8a2be2;
  /* ... */
}

body {
  background: var(--bg-primary);
  font-family: 'Rajdhani', sans-serif;
  color: var(--text-primary);
}
```

---

## 📐 **GRID SYSTEM**

### **Breakpoints:**

```css
--mobile:  480px
--tablet:  768px
--desktop: 1024px
--wide:    1440px
```

### **Container Widths:**

- Mobile: 100% (padding: 16px)
- Tablet: 720px
- Desktop: 960px
- Wide: 1200px

---

## 🎯 **DESIGN GOALS**

### **✅ DO:**
- Use consistent spacing
- Maintain high contrast
- Add smooth transitions
- Use semantic colors
- Keep animations subtle
- Optimize for performance

### **❌ DON'T:**
- Mix color schemes
- Overuse animations
- Use low contrast text
- Ignore mobile responsiveness
- Add unnecessary elements
- Sacrifice performance

---

## 🚀 **NEXT STEPS**

1. ✅ Define design system (this document)
2. ⏳ Implement launcher redesign
3. ⏳ Update website with new style
4. ⏳ Redesign ESP menu
5. ⏳ Create marketing materials
6. ⏳ Build component library

---

**This design system ensures:**
- ✨ Professional appearance
- 🎨 Visual consistency
- ⚡ Fast performance
- 📱 Responsive design
- 🎯 User-friendly interface

**All components (Launcher, Website, Cheat Menu) use this unified style!**

