# CS2 Game Offsets

Auto-generated offsets for Counter-Strike 2 game memory structures.

## 📁 **Directory Structure:**

```
offsets/
├── json/      → JSON format (for web/tools)
├── cpp/       → C++ headers (.hpp) 
├── csharp/    → C# files (.cs)
├── rust/      → Rust files (.rs)
└── meta/      → Metadata and info
```

## 📋 **Files:**

### **Core Files:**
- `offsets.{json,hpp,cs,rs}` - Main game offsets
- `buttons.{json,hpp,cs,rs}` - Input button codes
- `interfaces.{json,hpp,cs,rs}` - Game interfaces

### **DLL Modules:**
- `client_dll.*` - Client-side game logic (largest, ~400KB)
- `server_dll.*` - Server-side logic (~600KB)
- `engine2_dll.*` - Source 2 engine
- `animationsystem_dll.*` - Animation system
- `particles_dll.*` - Particle effects
- And 16 more modules...

## 🔄 **Auto-Update:**

These offsets are automatically updated when CS2 game updates.

**Updater:** `launcher-server/backend/src/services/telegramMonitor.js`
- Monitors @cstwoupdate Telegram channel
- Downloads new offsets from a2x/cs2-dumper
- Updates all formats (JSON, C++, C#, Rust)
- Notifies via WebSocket

## 💻 **Usage:**

### **C++ (externa cheat):**
```cpp
#include "offsets/cpp/client_dll.hpp"
#include "offsets/cpp/offsets.hpp"

// Use offsets
uintptr_t localPlayer = moduleBase + cs2_dumper::offsets::client_dll::dwLocalPlayerPawn;
```

### **Rust:**
```rust
use offsets::client_dll;

let local_player = module_base + client_dll::dwLocalPlayerPawn;
```

### **C#:**
```csharp
using Offsets;

var localPlayer = moduleBase + ClientDll.dwLocalPlayerPawn;
```

## 📊 **File Sizes:**

| Module | JSON | C++ | C# | Rust |
|--------|------|-----|----|----|
| client_dll | 417KB | 412KB | 369KB | 389KB |
| server_dll | 569KB | 658KB | 586KB | 619KB |
| animationsystem_dll | 250KB | 352KB | 311KB | 334KB |
| particles_dll | 214KB | 334KB | 295KB | 313KB |

**Total:** ~10MB across all formats

## 🔗 **Source:**

Original dumps from: https://github.com/a2x/cs2-dumper

## 📝 **Last Update:**

Check `meta/info.json` for timestamp and build info.

