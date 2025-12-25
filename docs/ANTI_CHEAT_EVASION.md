# 🛡️ MULTI-LAYER ANTI-CHEAT EVASION ARCHITECTURE

**Project:** EXTERNA  
**Focus:** Ring -3 to Ring +3 protection bypass  
**Goal:** Undetectable game enhancement

---

## 🏗️ **PROTECTION LAYERS (RINGS)**

```
┌──────────────────────────────────────────────────────┐
│  Ring +3: Usermode Application (Lowest Privilege)   │
│  ↓ Can be easily detected by anti-cheats            │
├──────────────────────────────────────────────────────┤
│  Ring 0: Kernel Mode (High Privilege)               │
│  ↓ Harder to detect, but still monitored            │
├──────────────────────────────────────────────────────┤
│  Ring -1: Hypervisor (VMX/AMD-V)                    │
│  ↓ Invisible to OS, controls kernel                 │
├──────────────────────────────────────────────────────┤
│  Ring -2: System Management Mode (SMM)              │
│  ↓ Most privileged x86 mode, invisible to hypervisor│
├──────────────────────────────────────────────────────┤
│  Ring -3: Intel ME / AMD PSP (Ultimate Control)     │
│  ↓ Firmware-level, complete hardware control        │
└──────────────────────────────────────────────────────┘
```

---

## 🎮 **OUR IMPLEMENTATIONS**

### **1. 🎯 Ring +3: EXTERNA (External Cheat)** ⭐ MAIN FOCUS

**Location:** `/cheats/externa/`

**Approach:** Memory reading from external process

**Advantages:**
- ✅ Easy to develop
- ✅ Fast iteration
- ✅ No kernel drivers needed
- ✅ Relatively safe (no BSOD risk)
- ✅ Easy to update

**Detection Risk:** 🟡 MEDIUM
- ReadProcessMemory detectable
- Pattern scanning visible
- Can be caught by behavioral analysis

**Anti-Detection Techniques:**
- External process (not injected)
- Minimal API calls
- Syscall direct calls
- Memory pattern obfuscation
- Timing randomization

**Features:**
- ✅ Box ESP
- ✅ Name ESP
- ✅ Health bars
- ✅ Distance ESP
- ✅ Weapon ESP
- ✅ Skeleton ESP
- ✅ Glow ESP
- ✅ Aimbot (smooth, FOV-based)
- ✅ Triggerbot
- ✅ RCS (Recoil Control)
- ✅ Bunny Hop

**Why Focus Here:**
1. **Fastest development** - No kernel complexity
2. **Easy updates** - When CS2 updates offsets
3. **User-friendly** - Simple installation
4. **Stable** - No system crashes
5. **Good performance** - Low overhead

---

### **2. 🔧 Ring 0: Kernel Driver**

**Location:** `/kernel/ring0/`

**Approach:** Kernel-mode memory access

**Advantages:**
- ✅ Full system access
- ✅ Hide process/modules
- ✅ Bypass usermode anti-cheat
- ✅ Direct memory manipulation

**Detection Risk:** 🟠 MEDIUM-HIGH
- Driver signature required
- Kernel callbacks monitored
- Easy Cheat / BattlEye detect unsigned drivers

**Status:** 🔧 EXPERIMENTAL

---

### **3. 🌀 Ring -1: Hypervisor (VMX)**

**Location:** `/hypervisor-cheat/`

**Approach:** Hardware virtualization (Intel VT-x / AMD-V)

**Advantages:**
- ✅ Invisible to OS
- ✅ Control kernel execution
- ✅ EPT (Extended Page Tables) hiding
- ✅ Intercept system calls
- ✅ Memory shadowing

**Detection Risk:** 🟢 LOW
- Very hard to detect from OS level
- Anti-cheats in Ring 0 can't see Ring -1

**Techniques:**
- EPT (Extended Page Tables) manipulation
- VMCALL interface for communication
- Memory hooks at hypervisor level
- Hidden memory regions

**Status:** 🔧 ADVANCED EXPERIMENTAL

**Challenges:**
- Complex development
- Performance overhead (~5-10%)
- Requires VT-x/AMD-V support
- Conflicts with other hypervisors

---

### **4. 🔥 Ring -2: SMM (System Management Mode)**

**Location:** `/kernel/ring_minus2/`

**Approach:** SMM is most privileged x86 mode

**Advantages:**
- ✅ Invisible to hypervisor
- ✅ Complete hardware control
- ✅ Can modify BIOS/UEFI
- ✅ Intercept everything

**Detection Risk:** 🟢 VERY LOW
- Almost impossible to detect
- Even hardware debuggers struggle

**Status:** 🧪 RESEARCH ONLY

**Why Not Main Focus:**
- Extremely complex
- BIOS modification required
- High risk (can brick motherboard)
- Not practical for most users

---

### **5. ⚡ Ring -3: Intel ME / AMD PSP**

**Location:** `/kernel/ring_minus3/`

**Approach:** Firmware coprocessor exploitation

**Advantages:**
- ✅ Ultimate privilege
- ✅ Complete invisibility
- ✅ Control entire system

**Detection Risk:** 🟢 IMPOSSIBLE TO DETECT

**Status:** 🔬 THEORETICAL RESEARCH

**Why Not Practical:**
- Requires firmware exploits
- Motherboard-specific
- Extremely dangerous
- Not ethical for gaming cheats

---

## 🎯 **RECOMMENDED STRATEGY**

### **Primary: Ring +3 (Externa)** ⭐

**Focus 80% effort here:**

1. **ESP System** (Main feature)
   - Box ESP (2D/3D)
   - Name + Health
   - Distance
   - Weapon
   - Skeleton
   - Glow effects
   - Customizable colors
   - Visibility checks

2. **Advanced ESP Features:**
   - 📊 Loot ESP (weapons, utility)
   - 🎯 Crosshair customization
   - 📡 Radar hack (2D minimap)
   - 🔊 Sound ESP (footsteps visualization)
   - 💣 Bomb timer & plant locations
   - 👥 Spectator list
   - 📈 Stats overlay (K/D, accuracy)

3. **Aimbot Features:**
   - Smooth aim (human-like)
   - FOV-based targeting
   - Bone selection (head, chest)
   - Aim key customization
   - Visibility checks
   - Team check
   - RCS (Recoil Control System)

4. **Misc Features:**
   - Bunny hop
   - Auto strafe
   - Triggerbot
   - Flash reducer
   - No smoke (client-side)
   - FOV changer

### **Secondary: Ring 0 (Kernel)** 🔧

**Use for advanced users (20% effort):**

- Memory hiding
- Process protection
- Bypass kernel-level anti-cheat
- Advanced injection methods

### **Research: Ring -1 to -3** 🔬

**Keep as research projects:**
- Proof of concept only
- Not for public release
- Educational purposes
- Future-proofing

---

## 🛡️ **ANTI-DETECTION STRATEGY**

### **For Externa (Ring +3):**

**1. External Process:**
```
Game Process ← ReadProcessMemory ← Cheat Process
(No injection = harder to detect)
```

**2. Direct Syscalls:**
```cpp
// Instead of ReadProcessMemory (easily hooked)
// Use NtReadVirtualMemory directly
NTSTATUS status = NtReadVirtualMemory(
    processHandle,
    baseAddress,
    buffer,
    size,
    nullptr
);
```

**3. Pattern Obfuscation:**
```cpp
// Randomize sleep intervals
Sleep(rand() % 50 + 10); // 10-60ms random

// Rotate memory read patterns
ReadMemoryRotating(address); // Different offsets each time
```

**4. Clean Memory Footprint:**
- No suspicious strings in binary
- XOR encrypt offsets
- Lazy initialization
- Clean up threads

**5. Behavioral Evasion:**
- Human-like timing
- Randomized actions
- Limit read frequency
- Mimic user input patterns

---

## 📊 **COMPARISON TABLE**

| Ring | Complexity | Detection Risk | Performance | Development Time | Recommended |
|------|------------|----------------|-------------|------------------|-------------|
| **+3 (Externa)** | ⭐ Low | 🟡 Medium | ⚡ Fast | 🕐 1-2 weeks | ✅ **YES** |
| **0 (Kernel)** | ⭐⭐⭐ Medium | 🟠 Medium-High | ⚡ Fast | 🕐 1-2 months | 🔧 Advanced |
| **-1 (Hypervisor)** | ⭐⭐⭐⭐ High | 🟢 Low | 🐌 Medium | 🕐 3-6 months | 🔬 Research |
| **-2 (SMM)** | ⭐⭐⭐⭐⭐ Very High | 🟢 Very Low | 🐌 Slow | 🕐 6-12 months | ❌ Impractical |
| **-3 (ME/PSP)** | ⭐⭐⭐⭐⭐ Extreme | 🟢 Impossible | 🐌 Slow | 🕐 1+ years | ❌ Theoretical |

---

## 🎯 **DEVELOPMENT PRIORITIES**

### **Phase 1: Perfect Externa (NOW)** 🎮

**Timeline:** 2-4 weeks

- [ ] Redesign launcher UI
- [ ] Expand ESP features (8+ options)
- [ ] Improve aimbot (smooth, human-like)
- [ ] Add radar hack
- [ ] Sound ESP visualization
- [ ] Loot ESP
- [ ] Performance optimization (<5ms overhead)
- [ ] Anti-detection improvements

**Goal:** Best-in-class external cheat

---

### **Phase 2: Kernel Driver (FUTURE)** 🔧

**Timeline:** 2-3 months

- [ ] Kernel-mode memory access
- [ ] Process hiding
- [ ] Module hiding
- [ ] Callback evasion
- [ ] HWID spoofer
- [ ] Advanced protection

**Goal:** Bypass kernel-level anti-cheats

---

### **Phase 3: Research Projects** 🔬

**Timeline:** Ongoing

- [ ] Hypervisor PoC
- [ ] EPT hiding experiments
- [ ] SMM research (educational)
- [ ] New bypass techniques

**Goal:** Stay ahead of anti-cheat evolution

---

## 📚 **RESOURCES**

### **Ring +3 (Externa):**
- Windows API documentation
- CS2 game structures (a2x/cs2-dumper)
- DirectX overlay techniques

### **Ring 0 (Kernel):**
- Windows Kernel Programming
- Rootkit development guides
- Driver signing bypass

### **Ring -1 (Hypervisor):**
- Intel VT-x documentation
- AMD-V specifications
- Hvpp (Hypervisor framework)

### **Ring -2/-3:**
- SMM documentation (Intel/AMD)
- UEFI firmware guides
- ME/PSP research papers

---

## 🎓 **WHY FOCUS ON EXTERNA?**

### **Practical Reasons:**

1. **✅ User Safety**
   - No kernel drivers = no BSOD risk
   - Easy to uninstall
   - No system modifications

2. **✅ Fast Development**
   - C++ only (no assembly)
   - Standard WinAPI
   - Quick updates for game patches

3. **✅ Good Performance**
   - <5ms overhead
   - External process = no game slowdown
   - Efficient rendering

4. **✅ Maintainability**
   - Easy to debug
   - Clear code structure
   - Fast iteration

5. **✅ User Experience**
   - Simple installation
   - No driver installation
   - Works on most systems

### **Detection Reality:**

**Modern anti-cheats (2025):**
- EasyAntiCheat: Ring 0 + behavioral analysis
- BattlEye: Ring 0 + machine learning
- Vanguard: Ring 0 + boot-level driver
- VAC: Ring +3 (weak but ban-wave based)

**Our Strategy:**
- Ring +3 with smart evasion > Ring 0 detected
- Behavioral mimicry > Raw power
- Frequent updates > Deep hiding

---

## 🚀 **NEXT STEPS**

1. **✅ Reorganized project structure**
2. **⏳ Focus on Externa ESP expansion**
3. **⏳ Implement new launcher**
4. **⏳ Add advanced features**
5. **⏳ Improve anti-detection**
6. **⏳ Performance optimization**

---

**Bottom Line:**

- 🎯 **80% effort on Externa (Ring +3)**
- 🔧 **15% on Kernel (Ring 0) for advanced users**
- 🔬 **5% on research (Ring -1 to -3)**

**Focus = Best external cheat with extensive ESP + solid evasion!**

