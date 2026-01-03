# Externa Rust V2 - CS2 External ESP

> **Version:** 2.3.0  
> **Language:** Rust  
> **Target:** Counter-Strike 2 (x64)  
> **Type:** External Wallhack (ESP)

---

## 📋 Содержание

1. [Обзор проекта](#обзор-проекта)
2. [Архитектура](#архитектура)
3. [Модули](#модули)
4. [Принцип работы](#принцип-работы)
5. [Оффсеты и Netvars](#оффсеты-и-netvars)
6. [Анти-детект меры](#анти-детект-меры)
7. [Сборка и запуск](#сборка-и-запуск)
8. [Горячие клавиши](#горячие-клавиши)
9. [Конфигурация](#конфигурация)
10. [Troubleshooting](#troubleshooting)

---

## Обзор проекта

**Externa Rust V2** — это внешний (external) чит для CS2, написанный на Rust. Чит работает из отдельного процесса, читая память игры через Windows API или прямые syscall'ы.

### Ключевые особенности

| Функция | Описание |
|---------|----------|
| **Box ESP** | Прямоугольники вокруг врагов |
| **Skeleton ESP** | Отрисовка костей персонажа |
| **Health Bar** | Полоска здоровья слева от бокса |
| **Snaplines** | Линии от низа экрана к игрокам |
| **Team Filter** | Автоопределение своей команды |
| **Smoothing** | Сглаживание для уменьшения дёрганий |

### Технологический стек

- **Язык:** Rust 2021 Edition
- **Рендеринг:** Direct2D (аппаратное ускорение)
- **Память:** NtReadVirtualMemory (syscall) / ReadProcessMemory (fallback)
- **Математика:** glam (SIMD-оптимизированная)
- **Оффсеты:** cs2-dumper API + hardcoded fallback

---

## Архитектура

```
┌─────────────────────────────────────────────────────────────┐
│                     EXTERNA RUST V2                         │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────────┐   ┌──────────────┐   ┌──────────────┐   │
│  │   Memory     │   │    Game      │   │   Overlay    │   │
│  │   Thread     │   │    State     │   │   Thread     │   │
│  │              │   │   (Mutex)    │   │              │   │
│  │  - Entities  │◄──┤              ├──►│  - Direct2D  │   │
│  │  - LocalTeam │   │  - entities  │   │  - Draw ESP  │   │
│  │  - Bones     │   │  - local_team│   │  - ViewMatrix│   │
│  └──────────────┘   └──────────────┘   └──────────────┘   │
│         │                                      │           │
│         ▼                                      ▼           │
│  ┌──────────────┐                     ┌──────────────┐    │
│  │   Syscall    │                     │   Smoothing  │    │
│  │  (Halo's     │                     │   Cache      │    │
│  │   Gate)      │                     │  (clamp-based)│   │
│  └──────────────┘                     └──────────────┘    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
          │                                      │
          ▼                                      ▼
    ┌──────────┐                          ┌──────────┐
    │  CS2.exe │                          │  Screen  │
    │ (Memory) │                          │ (Overlay)│
    └──────────┘                          └──────────┘
```

### Потоки

1. **Main Thread** — инициализация, input handling (hotkeys)
2. **Memory Thread** — чтение entities и local team (~500 Hz)
3. **Overlay Thread** — рендеринг ESP (~140 FPS, 7ms tick)

---

## Модули

### 📁 `src/main.rs`
Точка входа, инициализация, главный цикл.

```rust
// Ключевые структуры
struct EspConfig {
    enabled: AtomicBool,      // INSERT toggle
    show_box: AtomicBool,     // Box ESP
    show_skeleton: AtomicBool,// Skeleton ESP
    show_snaplines: AtomicBool,
    show_health: AtomicBool,
}

struct GameState {
    entities: Vec<Entity>,    // Все игроки
    local_team: i32,          // Наша команда (2=T, 3=CT)
}

struct ScreenPosCache {
    positions: HashMap<usize, (f32, f32, f32, f32)>,
    tick_duration: f32,       // Для time-based smoothing
}
```

### 📁 `src/game/`

#### `entity.rs`
Определения Entity и Bones.

```rust
pub struct Entity {
    pub pawn: usize,          // Адрес C_CSPlayerPawn
    pub controller: usize,    // Адрес CCSPlayerController
    pub pos: Vec3,            // Позиция в мире
    pub health: i32,          // Здоровье (0-100)
    pub team: i32,            // Команда
    pub bones: Bones,         // Скелет
}

pub struct Bones {
    pub head: Vec3,
    pub neck: Vec3,
    pub spine_1: Vec3,
    pub spine_2: Vec3,
    pub pelvis: Vec3,
    // ... все 17 костей
}
```

#### `math.rs`
World-to-Screen преобразование.

```rust
/// Преобразует 3D координаты в 2D пиксели экрана
pub fn w2s(matrix: &[[f32; 4]; 4], pos: Vec3, screen_width: f32, screen_height: f32) -> Option<Vec2> {
    // Row-major matrix multiplication
    let vx = pos.x * matrix[0][0] + pos.y * matrix[0][1] + pos.z * matrix[0][2] + matrix[0][3];
    let vy = pos.x * matrix[1][0] + pos.y * matrix[1][1] + pos.z * matrix[1][2] + matrix[1][3];
    let vw = pos.x * matrix[3][0] + pos.y * matrix[3][1] + pos.z * matrix[3][2] + matrix[3][3];
    
    if vw < 0.1 { return None; } // За камерой
    
    // NDC → пиксели
    let ndx = vx / vw;
    let ndy = vy / vw;
    let screen_x = (ndx * 0.5 + 0.5) * screen_width;
    let screen_y = (1.0 - (ndy * 0.5 + 0.5)) * screen_height;
    
    Some(Vec2::new(screen_x, screen_y))
}
```

#### `offsets.rs`
Оффсеты CS2 с динамической загрузкой.

```rust
// Hardcoded fallback (cs2-dumper 2026-01-03)
pub const DW_ENTITY_LIST: usize = 0x1A146C8;
pub const DW_LOCAL_PLAYER_CONTROLLER: usize = 0x1A6ED90;
pub const DW_VIEW_MATRIX: usize = 0x1A84490;

// Netvars (меняются реже)
pub mod netvars {
    pub const M_I_HEALTH: usize = 0x34C;
    pub const M_I_TEAM_NUM: usize = 0x3EB;
    pub const M_H_PLAYER_PAWN: usize = 0x8FC;
    pub const M_V_OLD_ORIGIN: usize = 0x15A0;
    pub const M_P_GAME_SCENE_NODE: usize = 0x330;
    pub const M_MODEL_STATE: usize = 0x190;
    pub const M_BONE_ARRAY: usize = 0x80;
}
```

### 📁 `src/memory/`

#### `handle.rs`
Работа с процессом и чтение памяти.

```rust
impl ProcessReader for Memory {
    fn read_raw(&self, address: usize, buffer: &mut [u8]) -> bool {
        unsafe {
            // 1. Пробуем syscall (быстрее, обходит хуки)
            if syscall::is_active() {
                let status = syscall::nt_read(...);
                if status == 0 { return true; }
            }
            // 2. Fallback на WinAPI
            ReadProcessMemory(...).is_ok()
        }
    }
}
```

#### `syscall.rs`
Прямые syscall'ы с Halo's Gate.

```rust
// SSN (Syscall Service Number) для NtReadVirtualMemory
static SSN_NT_READ: AtomicU32 = AtomicU32::new(0);

pub fn init() {
    // 1. Прямое извлечение (если не хукнуто)
    // Pattern: 4C 8B D1 B8 XX XX XX XX
    
    // 2. Halo's Gate - ищем в соседних функциях
    // Syscall stubs идут с шагом 32 байта
}

// Ассемблерный stub
global_asm!(
    "syscall_stub:",
    "mov r10, rcx",
    "mov eax, [rsp + 48]",  // SSN из 6-го аргумента
    "syscall",
    "ret"
);
```

#### `scanner.rs`
Pattern scanner для поиска оффсетов (отключен).

```rust
// Пример паттерна для dwEntityList:
// "48 8B 0D ? ? ? ? 48 8B D7 E8 ? ? ? ? 48 8B CB"
//           ↑ ↑ ↑ ↑
//       RIP-relative offset
```

### 📁 `src/overlay/`

#### `renderer.rs`
Direct2D overlay для отрисовки.

```rust
pub struct Direct2DOverlay {
    hwnd: HWND,                         // Handle окна
    target: ID2D1HwndRenderTarget,      // D2D render target
    brush_enemy: ID2D1SolidColorBrush,  // Красный
    brush_team: ID2D1SolidColorBrush,   // Зелёный
    brush_skeleton: ID2D1SolidColorBrush,// Белый
    width: u32,                         // Автодетект разрешения
    height: u32,
    class_name: String,                 // Рандомное имя окна
}
```

**Характеристики окна:**
- `WS_EX_TOPMOST` — всегда поверх
- `WS_EX_LAYERED` — прозрачность
- `WS_EX_TRANSPARENT` — клики проходят насквозь
- `WS_EX_NOACTIVATE` — не перехватывает фокус
- `LWA_COLORKEY(0)` — чёрный = прозрачный

---

## Принцип работы

### 1. Инициализация

```
1. init_logging()           → externa.log + console
2. syscall::init()          → Halo's Gate, находим SSN
3. find_process("cs2.exe")  → PID
4. find_module("client.dll")→ base address
5. Offsets::fetch_with_scan → API / hardcoded
6. Direct2DOverlay::new()   → создаём окно
7. spawn threads            → memory + input
```

### 2. Memory Thread (каждые 2ms)

```
1. Читаем local player controller
2. Находим local team
3. Итерируем entity list (1-64)
4. Для каждого игрока:
   - controller → pawn handle
   - pawn → position, health, team, bones
5. Записываем в GameState (Mutex)
```

### 3. Overlay Loop (каждые 7ms, ~143 FPS)

```
1. PeekMessage() → обработка WM_*
2. Читаем ViewMatrix (синхронизация!)
3. Для каждого entity:
   - Skip если teammate
   - W2S(head) и W2S(feet)
   - Smoothing (clamp-based)
   - Draw box/skeleton/health
4. EndDraw()
5. Sleep(7ms - elapsed)
```

### 4. Smoothing (сглаживание)

```rust
// Clamp-based smoothing (Colin's approach)
let max_move = 180.0 * tick_duration;  // 180 px/tick max

let dx = (new_x - old_x).clamp(-max_move, max_move);
let dy = (new_y - old_y).clamp(-max_move, max_move);

smooth_x = old_x + dx;
smooth_y = old_y + dy;
```

**Зачем:** При резком движении мыши позиция на экране может прыгнуть на 500+ пикселей за кадр. Clamp ограничивает скорость движения бокса, создавая эффект "прилипания".

---

## Оффсеты и Netvars

### Источники оффсетов

| Приоритет | Источник | Надёжность |
|-----------|----------|------------|
| 1 | cs2-dumper API | ✅ Актуальные, онлайн |
| 2 | Hardcoded | ⚠️ Требует ручного обновления |
| 3 | Pattern Scanner | ❌ Отключен (давал неверные результаты) |

### Как обновлять оффсеты

1. **Автоматически:** cs2-dumper API обновляется автором после каждого патча CS2

2. **Вручную:** Если API недоступен:
   ```rust
   // src/game/offsets.rs
   pub const DW_ENTITY_LIST: usize = 0x???;
   pub const DW_LOCAL_PLAYER_CONTROLLER: usize = 0x???;
   pub const DW_VIEW_MATRIX: usize = 0x???;
   ```
   
   Значения брать из:
   - https://github.com/a2x/cs2-dumper/blob/main/output/offsets.json

### Entity List структура

```
client.dll + dwEntityList
    └─→ EntityList*
        ├─ [0x10] → Chunk 0 (entities 0-511)
        │           ├─ [0 * 0x70] → Entity 0
        │           ├─ [1 * 0x70] → Entity 1
        │           └─ ...
        ├─ [0x18] → Chunk 1 (entities 512-1023)
        └─ ...

Entity (CCSPlayerController):
    +0x8FC  → m_hPlayerPawn (handle → pawn)

Pawn (C_CSPlayerPawn):
    +0x34C  → m_iHealth
    +0x3EB  → m_iTeamNum
    +0x15A0 → m_vOldOrigin (Vec3)
    +0x330  → m_pGameSceneNode
        +0x190 → m_modelState
            +0x80 → BoneArray
```

---

## Анти-детект меры

### Реализованные

| Техника | Описание |
|---------|----------|
| **Syscall** | Прямые системные вызовы, обходят usermode хуки |
| **Halo's Gate** | Находит SSN даже если NtReadVirtualMemory хукнут |
| **Random Window Class** | Имя окна рандомизируется при каждом запуске |
| **No Module Load** | Не загружаем DLL в CS2 |
| **External** | Полностью внешний, нет модификации памяти CS2 |

### Пример имён окна

```
MicrosoftHelper892347
WindowsService123089
SystemManager456712
DesktopWorker789023
```

### Чего НЕТ (потенциальные улучшения)

- [ ] Manual syscall (без ntdll.dll)
- [ ] Spoof process name
- [ ] Hide from EnumWindows
- [ ] Driver-level reading

---

## Сборка и запуск

### Требования

- Rust 1.70+ (stable)
- Windows 10/11 x64
- Visual Studio Build Tools (для windows crate)

### Сборка

```bash
cd cheats/externa-rust-v2
cargo build --release
```

Бинарник: `target/release/externa-rust-v2.exe`

### Запуск

1. Запустить CS2
2. Дождаться загрузки в матч
3. Запустить `externa-rust-v2.exe` от **администратора**
4. ESP должно появиться автоматически

### Профили сборки

```toml
[profile.release]
opt-level = 3      # Максимальная оптимизация
lto = true         # Link-time optimization
codegen-units = 1  # Один codegen unit
panic = "abort"    # Без unwinding
strip = true       # Убрать символы
```

---

## Горячие клавиши

| Клавиша | Действие |
|---------|----------|
| **INSERT** | Вкл/выкл ESP |
| **END** | Выход из программы |

---

## Конфигурация

### EspConfig (по умолчанию)

```rust
enabled: true,       // ESP включен
show_box: true,      // Боксы
show_skeleton: true, // Скелет
show_snaplines: false,// Линии к игрокам
show_health: true,   // Полоска HP
```

### Изменение настроек

В текущей версии конфиг хардкод. Для изменения редактируйте `main.rs`:

```rust
impl Default for EspConfig {
    fn default() -> Self {
        Self {
            show_snaplines: AtomicBool::new(true), // включить snaplines
            // ...
        }
    }
}
```

---

## Troubleshooting

### ESP не появляется

1. ✅ Проверьте права администратора
2. ✅ CS2 должен быть запущен ДО чита
3. ✅ Проверьте `externa.log` на ошибки

### LocalTeam = 0

```
[INFO] Entities: 9 | LocalTeam: 0  ← ПРОБЛЕМА
```

**Причина:** Неверный dwLocalPlayerController offset

**Решение:** Обновите оффсеты в `offsets.rs`

### ESP дёргается

**Причина:** Рассинхронизация ViewMatrix и позиций

**Решение:** Уже исправлено в V2.3 (ViewMatrix читается в render loop)

### Нет syscall (fallback to WinAPI)

```
[WARN] [Syscall] Failed to find SSN, falling back to WinAPI
```

**Причина:** ntdll.dll сильно захучен

**Решение:** Работает через ReadProcessMemory, менее безопасно но функционально

### Game crash

Чит **не должен** вызывать краши (только чтение памяти). Если происходит — это не связано с читом.

---

## Зависимости

```toml
[dependencies]
windows = "0.59"     # Windows API bindings
glam = "0.29"        # Fast math (SIMD)
anyhow = "1.0"       # Error handling
log = "0.4"          # Logging facade
env_logger = "0.11"  # Logger implementation
parking_lot = "0.12" # Fast mutex
rand = "0.8"         # Random generation
chrono = "0.4"       # Timestamps
serde = "1.0"        # JSON serialization
serde_json = "1.0"   # JSON parsing
ureq = "2.9"         # HTTP client
```

---

## Changelog

### V2.3.0 (2026-01-03)

- ✅ Time-based smoothing (clamp 180°/s)
- ✅ ViewMatrix sync в render loop
- ✅ cs2-dumper API для оффсетов
- ✅ Skeleton ESP
- ✅ Random window class
- ✅ File logging (externa.log)
- ✅ Halo's Gate для syscall SSN
- ✅ Auto screen resolution

---

## License

Этот проект предоставляется "как есть" исключительно в образовательных целях. Использование читов в онлайн-играх нарушает ToS и может привести к бану.

---

*Documentation generated: 2026-01-03*

