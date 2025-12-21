<div align="center">

# ⚡ EXTERNAL ESP v1.0.0

### 🎮 Complete CS2 Enhancement Suite

<br/>

![Version](https://img.shields.io/badge/VERSION-1.0.0-blueviolet?style=for-the-badge)
![Platform](https://img.shields.io/badge/PLATFORM-Windows_x64-0078D6?style=for-the-badge&logo=windows)
![C++](https://img.shields.io/badge/C++-23-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![DirectX](https://img.shields.io/badge/DirectX-11-green?style=for-the-badge)

<br/>

| 🚀 LAUNCHER | 🔓 EXTERNAL | 🔒 INTERNAL | 💉 INJECTOR |
|:-----------:|:----------:|:----------:|:-----------:|
| Premium GUI | Overlay ESP | DLL Injection | Manual Map |
| Update Check | Direct Syscall | Shadow VMT Hook | Stealth |
| One-Click Launch | Unlimited FPS | Zero Latency | No PEB Entry |

<br/>

[📥 Download](#-сборка) • [📖 Features](#-функции) • [⚙️ Config](#-конфигурация) • [🔧 Build](#-сборка)

</div>

---

## 📋 Содержание

- [🚀 Компоненты](#-компоненты)
- [🔓 EXTERNAL vs INTERNAL](#-external-vs-internal)
- [🎯 Функции](#-функции)
- [💾 Архитектура](#-архитектура)
- [🎨 Меню](#-меню)
- [⚙️ Конфигурация](#-конфигурация)
- [🔧 Сборка](#-сборка)
- [🎮 Использование](#-использование)
- [📁 Структура проекта](#-структура-проекта)
- [🛡️ Безопасность](#-безопасность)

---

## 🚀 Компоненты

> 🎯 **4 компонента = полный пакет**

### 🚀 LAUNCHER (Рекомендуется)

Premium GUI менеджер для запуска чита.

```
✅ Современный UI с анимациями и эффектами
✅ Выбор режима: External или Internal
✅ Проверка обновлений игры (защита от краша)
✅ Закруглённые углы (Windows 10/11 DWM API)
✅ Рандомный заголовок окна (антидетект)
✅ Автозакрытие после запуска
```

### 🔓 EXTERNAL (Overlay ESP)

Внешний чит - работает как отдельный процесс поверх игры.

```
✅ DirectComposition overlay (обход 60fps лимита DWM)
✅ Direct Syscall через ASM (обход хуков ntdll)
✅ SIMD WorldToScreen оптимизация (SSE intrinsics)
✅ Async Snapshot System (144Hz чтение данных)
✅ GPU Priority 7 (максимальный приоритет рендера)
✅ Smart Bone Cache (адаптивное кэширование костей)
✅ Frame Latency Waitable Object
✅ Auto-close при выходе из CS2
✅ Save/Load Config
```

### 🔒 INTERNAL (DLL Injection)

Внутренний чит - DLL инъекция в процесс игры.

```
✅ Manual Map инъекция (без LoadLibrary)
✅ Shadow VMT Hook (без патчинга кода)
✅ XOR String Encryption (compile-time)
✅ Lazy Import Resolution (скрытые API вызовы)
✅ Present hook = pixel-perfect ESP
✅ Нет записи в PEB
✅ Save/Load Config
```

### 💉 INJECTOR (Manual Map)

Стелс-инжектор для Internal чита.

```
✅ Manual Map без LoadLibrary
✅ Section mapping + Relocations
✅ Import resolution + SEH support
✅ Headers wiping для скрытности
✅ Нет записи в PEB модулей
```

---

## 🔓 EXTERNAL vs INTERNAL

| Характеристика | 🔓 **EXTERNAL** | 🔒 **INTERNAL** |
|:--|:--------------|:--------------|
| **Метод работы** | Отдельный процесс + Overlay | DLL в процессе игры |
| **ESP Стабильность** | ~98% (Async Snapshot) | 100% (pixel-perfect) |
| **Риск детекта** | Низкий | Средний |
| **Требует** | Ничего | Инжектор |
| **Чтение памяти** | Direct Syscall (ASM) | Прямой указатель |
| **Рендер** | DirectComposition | Present Hook |
| **FPS оверлея** | Unlimited | Синхронизирован с игрой |
| **Anti-Detect** | Minimal | XOR + LazyImport + Shadow VMT |

**Рекомендация:**
- 🔓 **EXTERNAL** - для безопасной игры, меньше риск бана
- 🔒 **INTERNAL** - для идеальной стабильности ESP

---

## 🎯 Функции

### 📊 ESP Features

| Функция | 🔓 EXTERNAL | 🔒 INTERNAL | Описание |
|:-------:|:-------:|:-------:|----------|
| 📦 **Box** | ✅ | ✅ | Обычный прямоугольник |
| ⌜⌝ **Cornered** | ✅ | ✅ | Только углы (ohud-style) |
| 🎨 **Fill** | ✅ | ✅ | Полупрозрачная заливка (5-50%) |
| 🦴 **Skeleton** | ✅ | ✅ | Кости игрока (до 50м) |
| 🎯 **Head Dot** | ✅ | ✅ | Точка на голове (размер 1-5) |
| ❤️ **Health Bar** | ✅ | ✅ | Градиент HP (красный→жёлтый→зелёный) |
| 🛡️ **Armor Bar** | ✅ | ✅ | Синяя полоска брони |
| 👤 **Name** | ✅ | ✅ | Имя игрока над боксом |
| 📏 **Distance** | ✅ | ✅ | Дистанция в метрах |
| ➖ **Snaplines** | ✅ | ❌ | Линии от центра экрана к врагам |
| 🎯 **Crosshair** | ✅ | ✅ | Перекрестие для снайперов без скопа |
| ✨ **Outlined Text** | ✅ | ✅ | Текст с чёрной обводкой |

### 🎚️ Фильтры и настройки

```
☑️ Enemy Only          Показывать только врагов
📏 Max Distance        Максимальная дистанция отрисовки (0-500m)
🎨 Custom Colors       Настраиваемые цвета:
                       • Enemy (враги)
                       • Team (союзники)
                       • Skeleton (кости)
                       • Head Dot (точка на голове)
```

---

## 💾 Архитектура

### 🚀 LAUNCHER

```
┌─────────────────────────────────────────────────────────────┐
│                    LAUNCHER v1.0.0                           │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ╭─────────────────╮    ╭─────────────────╮                 │
│  │    EXTERNAL     │    │    INTERNAL     │                 │
│  │   Overlay Mode  │    │   Inject Mode   │                 │
│  ╰─────────────────╯    ╰─────────────────╯                 │
│                                                              │
│           ╭───────────────────────────────╮                 │
│           │         LAUNCH                │                 │
│           ╰───────────────────────────────╯                 │
│                                                              │
│  • DWM Rounded Corners (Win10/11)                           │
│  • Game Update Detection                                     │
│  • XOR String Encryption                                     │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 🔓 EXTERNAL

```
┌─────────────────────────────────────────────────────────────┐
│                 EXTERNAL - Async Architecture                │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────────────┐      ┌──────────────────────┐     │
│  │   READER THREAD      │      │   RENDER THREAD      │     │
│  │   (144Hz polling)    │      │   (Unlimited FPS)    │     │
│  │                      │      │                      │     │
│  │  • ViewMatrix        │ ───► │  • WorldToScreen     │     │
│  │  • LocalPlayer       │      │    (SIMD SSE)        │     │
│  │  • 64 Players        │      │  • ImGui Draw        │     │
│  │  • Health/Armor      │      │  • DirectComposition │     │
│  │  • Bones (Smart)     │      │    Present           │     │
│  │    <20m: every frame │      │                      │     │
│  │    >20m: every 5th   │      │  GPU Priority: 7     │     │
│  │                      │      │  Frame Latency: 1    │     │
│  │  Direct Syscall      │      │                      │     │
│  │  (NtReadVirtualMem)  │      │                      │     │
│  └──────────────────────┘      └──────────────────────┘     │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                 DrawSnapshot Buffer                   │   │
│  │  Double-buffered: Back (write) ←→ Front (read)       │   │
│  │  Atomic swap (minimal lock contention)                │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 🔒 INTERNAL

```
┌─────────────────────────────────────────────────────────────┐
│                 INTERNAL - Stealth Architecture              │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────────────┐      ┌──────────────────────┐     │
│  │   ANTI-DETECT        │      │   SHADOW VMT HOOK    │     │
│  │                      │      │                      │     │
│  │  • XOR Strings       │      │  IDXGISwapChain      │     │
│  │    XString("...")    │      │  ┌─────────────────┐ │     │
│  │                      │      │  │ Original VTable │ │     │
│  │  • Lazy Import       │      │  │ [0] QueryIntf   │ │     │
│  │    LI_FN(WinAPI)     │      │  │ [8] Present ────┼─┼──►  │
│  │                      │      │  │     ...         │ │     │
│  │  • Manual Map        │      │  └─────────────────┘ │     │
│  │    No PEB entry      │      │                      │     │
│  │                      │      │  Shadow VTable copy  │     │
│  │  • Headers Wiped     │      │  (no code patches!)  │     │
│  └──────────────────────┘      └──────────────────────┘     │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                   HookedPresent()                     │   │
│  │  • Direct memory read (no syscall overhead)           │   │
│  │  • W2S calculated in same frame as game render        │   │
│  │  • ImGui render → Original Present                    │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### ⚡ Технологии

| Технология | Компонент | Описание |
|:----------:|:---------:|----------|
| **Direct Syscall** | EXTERNAL | ASM stub для `NtReadVirtualMemory`, обходит хуки ntdll |
| **DirectComposition** | EXTERNAL | DXGI 1.2 + DComp, обходит DWM 60fps лимит |
| **SIMD W2S** | EXTERNAL | SSE intrinsics (`_mm_*`) для быстрого WorldToScreen |
| **Shadow VMT** | INTERNAL | Копия VTable вместо патчинга оригинала |
| **XOR Strings** | INTERNAL/LAUNCHER | Compile-time шифрование строк |
| **Lazy Import** | INTERNAL | Скрытые API вызовы без Import Table |
| **Manual Map** | INJECTOR | Инъекция без LoadLibrary и PEB записи |
| **Frame Latency** | EXTERNAL | `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT` |
| **GPU Priority** | EXTERNAL | `SetGPUThreadPriority(7)` - максимальный приоритет |
| **Smart Bone Cache** | EXTERNAL | Кости: каждый кадр если < 20m, иначе каждый 5-й |

---

## 🎨 Меню

### Внешний вид

```
┌────────────────────────────────────────────────────┐
│  EXTERNAL v1.0.0 / INTERNAL v1.0.0                 │
├────────────────────────────────────────────────────┤
│                                                    │
│  [ESP]  [Config]                                   │
│                                                    │
│  ☑️ Enable ESP                                     │
│                                                    │
│  ═══════════ Visuals ═══════════                  │
│  ☑️ Box          ☐ Cornered                       │
│  ☐ Fill         [████░░░░░░] 15%                  │
│  ☐ Skeleton                                        │
│  ☐ Head Dot     [████░░░░░░] 2.0                  │
│  ☑️ Health Bar   ☑️ Armor Bar                      │
│  ☑️ Name         ☑️ Distance                       │
│  ☐ Snaplines    ☑️ Crosshair                      │
│  ☑️ Outlined Text                                  │
│                                                    │
│  ═══════════ Filters ═══════════                  │
│  ☑️ Enemy Only                                     │
│  Max Range:     [████████░░] 500m                 │
│                                                    │
│  ═══════════ Colors ═══════════                   │
│  Enemy:    [🔴]  Team:     [🔵]                   │
│  Skeleton: [⚪]  Head Dot: [⚪]                   │
│                                                    │
├────────────────────────────────────────────────────┤
│  INSERT - Toggle Menu | END - Exit                 │
└────────────────────────────────────────────────────┘
```

### Config Tab

```
┌────────────────────────────────────────────────────┐
│  [ESP]  [Config]                                   │
├────────────────────────────────────────────────────┤
│                                                    │
│  ═══════════ Configuration ═══════════            │
│                                                    │
│  [💾 Save Config]     Сохранить в config.bin      │
│  [📂 Load Config]     Загрузить настройки         │
│  [🔄 Reset Default]   Сбросить всё                │
│                                                    │
│  ═══════════ Performance ═══════════              │
│  (только EXTERNAL)                                 │
│                                                    │
│  [DirectComposition ACTIVE - Unlimited FPS!]       │
│                                                    │
│  ☐ VSync Overlay                                   │
│  FPS Limit: [Unlimited ▼]                         │
│                                                    │
└────────────────────────────────────────────────────┘
```

---

## ⚙️ Конфигурация

### Файлы конфигурации

| Файл | Компонент | Описание |
|:----:|:---------:|----------|
| `config.bin` | EXTERNAL | Бинарный файл настроек External ESP |
| `config_int.bin` | INTERNAL | Бинарный файл настроек Internal ESP |

### Автозагрузка

При запуске чит автоматически загружает конфигурацию из соответствующего файла.

### Горячие клавиши

| Клавиша | Действие |
|:-------:|:--------:|
| `INSERT` | Показать/Скрыть меню |
| `END` | Выход из чита (только EXTERNAL) |

---

## 🔧 Сборка

### 📋 Требования

```
✅ Windows 10/11 x64
✅ Visual Studio 2022 (MSVC v143)
✅ CMake 3.20+
✅ Ninja (рекомендуется)
✅ DirectX 11 SDK
✅ Права администратора (для запуска)
```

### 📦 Полная сборка

```bash
# Клонирование
git clone https://github.com/user/external-esp.git
cd external-esp

# Сборка всех компонентов
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Результат:
# build/externa/external.exe   - External ESP
# build/interna/internal.dll   - Internal DLL
# build/injector/injector.exe  - Injector
# build/launcher/launcher.exe  - Launcher
```

### 📦 Отдельная сборка

```bash
# Только EXTERNAL
cd externa
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Только INTERNAL + INJECTOR
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target interna
cmake --build build --target injector

# Только LAUNCHER
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target launcher
```

### 🔄 GitHub Actions

Все билды запускаются **вручную** через `workflow_dispatch`:

1. Перейдите в **Actions** на GitHub
2. Выберите нужный workflow:
   - `🔓 Build External`
   - `🔒 Build Internal`
   - `🚀 Build Launcher`
3. Нажмите **Run workflow**
4. Скачайте артефакт после завершения

---

## 🎮 Использование

### 🚀 Через Launcher (Рекомендуется)

1. Запустите **CS2** (Windowed или Borderless Windowed)
2. Запустите **`launcher.exe`** от администратора
3. Выберите режим: **EXTERNAL** или **INTERNAL**
4. Нажмите **LAUNCH**
5. Нажмите `INSERT` в игре для открытия меню

### 🔓 EXTERNAL напрямую

1. Запустите CS2 в режиме **Windowed** или **Borderless Windowed**
2. Запустите **`external.exe`** от администратора
3. Нажмите `INSERT` для открытия меню
4. `END` для выхода

### 🔒 INTERNAL напрямую

1. Запустите **CS2**
2. Запустите **`injector.exe`** (найдёт `internal.dll` автоматически)
3. Дождитесь сообщения "Injection Successful!"
4. Нажмите `INSERT` в игре для открытия меню

### ⚠️ Важные заметки

```
📌 Лучшая стабильность ESP на СРЕДНИХ настройках графики CS2!
📌 Skeleton и Head Dot отображаются только до 50 метров
📌 Config сохраняется в текущую директорию
📌 При обновлении CS2 нужно дождаться обновления оффсетов
```

---

## 📁 Структура проекта

```
📂 external-esp/
│
├── 📂 externa/                   # 🔓 External ESP
│   ├── 📂 src/
│   │   ├── main.cpp              # Основной код (~1600 строк)
│   │   │   ├── Memory            # Direct Syscall wrapper
│   │   │   ├── DrawSnapshot      # Async buffer system
│   │   │   ├── dataReaderLoop    # 144Hz data polling
│   │   │   ├── renderESP         # SIMD W2S + ImGui
│   │   │   ├── createOverlay     # DirectComposition setup
│   │   │   └── renderMenu        # ImGui menu
│   │   └── syscall.asm           # Direct syscall stub (ASM)
│   └── CMakeLists.txt
│
├── 📂 interna/                   # 🔒 Internal DLL
│   ├── 📂 src/
│   │   ├── dllmain.cpp           # VMT Hook + ESP (~720 строк)
│   │   ├── xorstr.hpp            # XOR string encryption
│   │   └── lazy_importer.hpp     # Hidden API calls
│   └── CMakeLists.txt
│
├── 📂 injector/                  # 💉 Manual Map Injector
│   ├── 📂 src/
│   │   └── main.cpp              # Stealth injection (~350 строк)
│   └── CMakeLists.txt
│
├── 📂 launcher/                  # 🚀 Premium Launcher
│   ├── 📂 src/
│   │   ├── main.cpp              # ImGui GUI (~750 строк)
│   │   └── xorstr.hpp            # XOR strings
│   └── CMakeLists.txt
│
├── 📂 output/                    # 📊 CS2 Offsets (auto-generated)
│   ├── offsets.json
│   ├── client_dll.json
│   └── ...
│
├── 📂 .github/workflows/         # ⚙️ GitHub Actions
│   ├── build.yml                 # External build
│   ├── internal.yml              # Internal build
│   └── launcher.yml              # Launcher build
│
├── 📜 CMakeLists.txt             # Root CMake
└── 📜 README.md                  # Документация
```

---

## 🛡️ Безопасность

### Таблица технологий

| Технология | EXTERNAL | INTERNAL | LAUNCHER | INJECTOR |
|:----------:|:--------:|:--------:|:--------:|:--------:|
| DLL Injection | ❌ | ✅ | ❌ | ✅ |
| WriteProcessMemory | ❌ | ❌ | ❌ | ✅ |
| Direct Syscall | ✅ | ❌ | ❌ | ❌ |
| XOR Strings | ❌ | ✅ | ✅ | ❌ |
| Lazy Import | ❌ | ✅ | ❌ | ❌ |
| Shadow VMT | ❌ | ✅ | ❌ | ❌ |
| No PEB Entry | N/A | ✅ | N/A | ✅ |
| Random Window Title | ❌ | ❌ | ✅ | ❌ |
| Game Update Check | ❌ | ❌ | ✅ | ❌ |
| Manual Map | ❌ | ✅ | ❌ | ✅ |
| Headers Wiping | ❌ | ❌ | ❌ | ✅ |

### Уровни риска

| Компонент | Уровень риска | Причина |
|:---------:|:-------------:|---------|
| 🔓 EXTERNAL | 🟢 Низкий | Отдельный процесс, только чтение памяти |
| 🔒 INTERNAL | 🟡 Средний | DLL в процессе игры |
| 💉 INJECTOR | 🟡 Средний | Манипуляции с памятью процесса |
| 🚀 LAUNCHER | 🟢 Низкий | Только запуск других программ |

---

<div align="center">

> ⚠️ **Только для образовательных целей!**
> 
> Использование читов в онлайн-играх нарушает правила сервиса
> и может привести к блокировке аккаунта.

---

<br/>

## ⚡ EXTERNAL ESP v1.0.0

### 🎮 Complete CS2 Enhancement Suite

<br/>

```
┌─────────────────────────────────────────┐
│                                         │
│   LAUNCHER → EXTERNAL или INTERNAL      │
│                  ↓                      │
│            INSERT = Menu                │
│                                         │
└─────────────────────────────────────────┘
```

<br/>

**Made with 💜 for CS2**

</div>
