# 🎯 EXTERNA ESP MENU - DESIGN MOCKUP

**Version:** 2.0  
**Style:** Matches Launcher (Dark Future)  
**Type:** In-game overlay  
**Date:** December 25, 2025

---

## 🎨 **DESIGN PHILOSOPHY**

**In-game overlay должно быть:**
- Минимальное (не мешает игре)
- Быстрый доступ (горячие клавиши)
- Красивое (соответствует лаунчеру)
- Интуитивное (простое управление)

---

## 📐 **MENU LAYOUT**

### **Closed State (Compact):**

```
┌─────────────┐
│ ▓ EXTERNA ▓ │  ← Just header bar (draggable)
└─────────────┘
  Press INSERT to open
```

### **Open State (Full Menu):**

```
┌─────────────────────────────────────┐
│  ▓ EXTERNA ESP ▓         [_] [×]    │  ← Header (draggable)
├─────────────────────────────────────┤
│                                     │
│  VISUALS                      [▼]   │  ← Collapsible section
│  ├─ ☑ Box ESP                       │
│  ├─ ☑ Name ESP                      │
│  ├─ ☑ Health Bar                    │
│  ├─ ☑ Weapon Info                   │
│  ├─ ☑ Distance                      │
│  ├─ ☑ Skeleton ESP                  │
│  ├─ ☐ Glow ESP                      │
│  └─ ☐ Snaplines                     │
│                                     │
│  ADVANCED ESP                 [▼]   │
│  ├─ ☐ Loot ESP                      │
│  ├─ ☐ Sound ESP                     │
│  ├─ ☐ Radar Hack                    │
│  ├─ ☐ Bomb Timer                    │
│  └─ ☐ Spectator List                │
│                                     │
│  COLORS                       [▼]   │
│  ├─ Enemy:  [🔴] #ff3c3c            │
│  ├─ Team:   [🔵] #00b4ff            │
│  └─ Loot:   [🟡] #ffd700            │
│                                     │
│  AIMBOT                       [▼]   │
│  ├─ ☐ Enable Aimbot                 │
│  ├─ FOV:      [═══░░] 90°           │
│  ├─ Smooth:   [====░] 80%           │
│  ├─ Bone: ▼ Head                    │
│  ├─ Key: ▼ Mouse4                   │
│  └─ ☑ Visibility Check              │
│                                     │
│  MISC                         [▼]   │
│  ├─ ☑ Bunny Hop                     │
│  ├─ ☐ Triggerbot                    │
│  ├─ ☐ RCS (Recoil Control)          │
│  └─ ☐ Auto Strafe                   │
│                                     │
│  ───────────────────────────────    │
│  [💾 SAVE]  [🔄 RESET]  [📋 LOAD]  │
│                                     │
└─────────────────────────────────────┘
   300px wide × dynamic height
```

---

## 🎨 **VISUAL STYLE**

### **Colors (Same as Launcher):**

```css
/* Background */
--menu-bg:      rgba(18, 18, 26, 0.95)    /* Semi-transparent */
--menu-border:  #8a2be2                   /* Purple */
--menu-glow:    0 0 20px rgba(138,43,226,0.5)

/* Text */
--text-primary:   #ffffff
--text-secondary: #a0a0b0
--text-disabled:  #606070

/* Elements */
--checkbox-on:    #00ff88
--checkbox-off:   #606070
--slider-fill:    #8a2be2
--slider-bg:      #2a2a34
```

### **Effects:**

```css
/* Background blur (behind menu) */
backdrop-filter: blur(10px);

/* Border glow */
box-shadow: 0 0 20px rgba(138,43,226,0.5);

/* Hover animations */
transition: all 0.2s ease;
```

---

## 🎯 **COMPONENT DETAILS**

### **1. Header Bar**

```
┌─────────────────────────────────────┐
│  ▓ EXTERNA ESP ▓         [_] [×]    │
│   └─Logo (glow)          │   └─Close│
│                          └─Minimize │
└─────────────────────────────────────┘
```

**Features:**
- Draggable (click & drag anywhere)
- Logo with glow effect
- Minimize button (collapse to compact)
- Close button (hide menu)

---

### **2. Collapsible Section**

```
VISUALS                      [▼]  ← Expanded
├─ Items shown...

AIMBOT                       [▶]  ← Collapsed
```

**Behavior:**
- Click title to toggle
- Smooth expand/collapse animation
- Icon rotates (▼/▶)
- Only one section expanded at a time (optional)

---

### **3. Checkbox**

```
☑ Box ESP     ← Enabled (green checkmark)
☐ Glow ESP    ← Disabled (gray)
```

**Colors:**
- Enabled: `#00ff88` (green)
- Disabled: `#606070` (gray)
- Hover: slight glow

---

### **4. Color Picker**

```
Enemy:  [🔴] #ff3c3c
         │
         └─Click to open color picker
```

**Expanded:**
```
┌─────────────────┐
│ Enemy Color     │
│ ┌─────────────┐ │
│ │ [Color Grid]│ │  ← HSV color picker
│ └─────────────┘ │
│ Hex: #ff3c3c    │
│ [OK] [Cancel]   │
└─────────────────┘
```

---

### **5. Slider**

```
FOV:      [═══░░] 90°
          └─Drag to adjust
```

**Features:**
- Visual fill bar
- Current value displayed
- Min/Max limits shown on hover
- Smooth dragging

**Variants:**
```
[═══════░] 80%   ← Percentage (0-100)
[════░░░░] 90°   ← Degrees (0-180)
[══░░░░░░] 5.0   ← Decimal (0-10)
```

---

### **6. Dropdown**

```
Bone: ▼ Head
      │
      └─Click to expand

Expanded:
┌──────────┐
│ • Head   │  ← Selected
│   Neck   │
│   Chest  │
│   Stomach│
└──────────┘
```

---

### **7. Keybind**

```
Key: ▼ Mouse4
     │
     └─Click to rebind

Rebinding:
Key: [Press any key...]
```

---

## 🎮 **HOTKEYS**

```
INSERT       → Open/Close menu
HOME         → Toggle ESP
END          → Toggle Aimbot
DELETE       → Panic (unload cheat)
PAGEUP/DOWN  → Change ESP distance

Ctrl+S       → Save config
Ctrl+L       → Load config
Ctrl+R       → Reset to default
```

---

## 📊 **TABS (Optional Advanced)**

If too many features, use tabs:

```
┌─────────────────────────────────────┐
│  ▓ EXTERNA ESP ▓         [_] [×]    │
├─────────────────────────────────────┤
│  [ESP] [AIM] [MISC] [CFG]           │  ← Tab bar
├─────────────────────────────────────┤
│                                     │
│  Currently viewing: ESP             │
│  ... content ...                    │
│                                     │
└─────────────────────────────────────┘
```

---

## 🎨 **ESP VISUAL EXAMPLES**

### **Box ESP:**

```
        ┌────────┐
        │ PLAYER │  ← Name
        │        │
        │  100   │  ← Health bar (green)
  50m   │        │  ← Distance
        │  AK-47 │  ← Weapon
        └────────┘
```

**Styles:**
- 2D Box (corners or full)
- 3D Box
- Filled box (with transparency)
- Rounded corners

---

### **Skeleton ESP:**

```
        ●          ← Head
       /|\         ← Body
      / | \
       / \         ← Legs
```

**Bones Connected:**
- Head → Neck → Chest → Hips
- Shoulders → Elbows → Hands
- Hips → Knees → Feet

---

### **Health Bar:**

```
┌─────────────┐
│█████████░░░░│ 75 HP  ← Gradient: Green → Yellow → Red
└─────────────┘
```

**Colors:**
- 100-75%: Green (#00ff88)
- 75-25%: Yellow (#ffd700)
- 25-0%: Red (#ff3c3c)

---

### **Sound ESP:**

```
     ~~~~👂~~~~      ← Footsteps (circular wave)
        🔊50m
     
Direction: 🧭 NW     ← Compass direction
Type: Footsteps      ← Sound type
```

---

### **Radar Hack (Minimap):**

```
┌─────────────────┐
│  🔴 🔴          │  ← Enemies (red dots)
│     🟢          │  ← You (green)
│  🔴             │
│        🔵 🔵    │  ← Team (blue)
└─────────────────┘
   Bottom-left corner (150x150px)
```

---

### **Loot ESP:**

```
💰 $5000    ← Money
🔫 AWP      ← Weapon (rare = purple text)
💣 C4       ← Bomb (flashing red)
🛡️ Armor    ← Utility
```

**Distance filter:**
- <10m: Bright + Large font
- 10-30m: Normal
- >30m: Dim + Small font

---

## ✨ **ANIMATIONS**

### **1. Menu Open/Close:**

```
Closed → Open:
1. Scale up from 0 to 1 (0.2s)
2. Fade in opacity 0 → 1 (0.2s)

Open → Closed:
1. Scale down 1 → 0 (0.2s)
2. Fade out 1 → 0 (0.2s)
```

### **2. Section Expand/Collapse:**

```
1. Icon rotates ▼ → ▶ (0.15s)
2. Content height animates (0.15s)
3. Items fade in/out (0.1s)
```

### **3. Checkbox Toggle:**

```
Off → On:
1. Checkmark appears (scale 0 → 1.2 → 1)
2. Color green glow pulse
3. Haptic click sound (optional)
```

---

## 🎯 **CONFIG SYSTEM**

### **Save/Load Configs:**

```
CONFIG MANAGER                  [×]
┌──────────────────────────────┐
│ Available Configs:           │
│ ● Default                    │
│   Rage Mode                  │
│   Legit Play                 │
│   ESP Only                   │
│                              │
│ [➕ NEW]  [💾 SAVE]  [🗑️ DEL] │
│                              │
│ Current: Default             │
└──────────────────────────────┘
```

**Config Storage:**
```
configs/
├── default.json
├── rage.json
├── legit.json
└── esp_only.json
```

---

## 🚀 **IMPLEMENTATION PLAN**

### **Phase 1: Core Menu (Week 1)**

- [x] ImGui window
- [ ] Header bar (draggable)
- [ ] Collapsible sections
- [ ] Checkboxes
- [ ] Basic styling

### **Phase 2: Controls (Week 1)**

- [ ] Sliders
- [ ] Color pickers
- [ ] Dropdowns
- [ ] Keybinds

### **Phase 3: ESP Features (Week 2)**

- [ ] Box ESP (2D/3D)
- [ ] Name ESP
- [ ] Health bars
- [ ] Weapon info
- [ ] Distance
- [ ] Skeleton

### **Phase 4: Advanced ESP (Week 2)**

- [ ] Sound ESP
- [ ] Loot ESP
- [ ] Radar hack
- [ ] Bomb timer
- [ ] Spectator list

### **Phase 5: Aimbot (Week 3)**

- [ ] FOV circle
- [ ] Target selection
- [ ] Smooth aim
- [ ] RCS
- [ ] Triggerbot

### **Phase 6: Polish (Week 3)**

- [ ] Animations
- [ ] Particle effects
- [ ] Config system
- [ ] Hotkeys
- [ ] Performance optimization

---

## 📏 **POSITIONING**

### **Default Position:**

```
Screen (1920x1080)
┌──────────────────────────────┐
│ [Menu]                       │  ← Top-left (50px, 50px)
│                              │
│                              │
│                              │
│                        [Map] │  ← Bottom-right (radar)
└──────────────────────────────┘
```

**Draggable anywhere!**

---

## 🎨 **VISUAL COMPARISON**

### **Before (Current):**

```
┌──────────────┐
│ MENU         │
│ □ Box ESP    │
│ □ Name       │
│ □ Health     │
└──────────────┘
```
❌ Basic, ugly, amateur

### **After (New Design):**

```
┌─────────────────────────────────────┐
│  ▓ EXTERNA ESP ▓         [_] [×]    │
├─────────────────────────────────────┤
│  VISUALS                      [▼]   │
│  ├─ ☑ Box ESP       [═══░] 50m     │
│  ├─ ☑ Name ESP                      │
│  └─ ☑ Health Bar                    │
│  COLORS                       [▼]   │
│  └─ Enemy: [🔴] #ff3c3c             │
└─────────────────────────────────────┘
```
✅ Professional, modern, beautiful!

---

## 🔗 **INTEGRATION WITH LAUNCHER**

**Launcher sets config:**
```cpp
// Launcher saves config to file
SaveConfig("configs/default.json");
```

**Cheat loads on inject:**
```cpp
// External cheat reads config
LoadConfig("configs/default.json");
ApplySettings();
```

**Live sync (future):**
- WebSocket connection
- Change in launcher → Instant update in-game
- No need to restart cheat

---

## ✅ **CHECKLIST**

- [ ] Design mockup ✅ (this document)
- [ ] ImGui custom theme
- [ ] Menu framework (tabs, sections)
- [ ] All controls (checkbox, slider, etc.)
- [ ] ESP rendering
- [ ] Aimbot system
- [ ] Config save/load
- [ ] Hotkeys
- [ ] Animations
- [ ] Performance (<5ms overhead)

---

**This menu will be beautiful, functional, and match the professional launcher design!** 🎯✨

