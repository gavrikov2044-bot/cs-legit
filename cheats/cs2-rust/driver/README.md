# Externa Kernel Driver

Kernel-mode driver для чтения памяти через Ring 0.

## Требования

1. **Visual Studio 2022** (Community - бесплатная)
   - При установке выбрать: "Desktop development with C++"
   - Скачать: https://visualstudio.microsoft.com/

2. **Windows SDK**
   - Скачать: https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/

3. **Windows Driver Kit (WDK)**
   - Скачать: https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk
   - **ВАЖНО:** Версия WDK должна совпадать с версией SDK!

4. **Rust Nightly**
   ```powershell
   rustup install nightly
   rustup component add rust-src --toolchain nightly
   ```

## Сборка

### Вариант 1: Через batch файл (рекомендуется)
```
Двойной клик на build.bat
```

### Вариант 2: Через командную строку
```powershell
cargo +nightly build --release -Z build-std=core,alloc --target x86_64-pc-windows-msvc
```

### Вариант 3: Через Visual Studio
1. Открыть **Developer Command Prompt for VS 2022**
2. Перейти в папку driver: `cd path\to\driver`
3. Запустить: `build.bat`

## Установка драйвера

### Шаг 1: Включить Test Mode (один раз)
```powershell
# Запустить от администратора
bcdedit /set testsigning on
# Перезагрузить компьютер
```

### Шаг 2: Установить драйвер
```
Правый клик на install.bat → "Запуск от имени администратора"
```

## Удаление

```
Правый клик на uninstall.bat → "Запуск от имени администратора"
```

## Отключить Test Mode (после тестирования)

```powershell
bcdedit /set testsigning off
# Перезагрузить компьютер
```

## Структура

```
driver/
├── build.bat        # Скрипт сборки
├── install.bat      # Установка драйвера
├── uninstall.bat    # Удаление драйвера
├── Cargo.toml       # Rust конфигурация
└── src/
    ├── lib.rs           # Entry point
    ├── memory.rs        # Чтение памяти
    ├── process.rs       # Работа с процессами
    └── communication.rs # IOCTL обработка
```

## Troubleshooting

### "Driver not signed"
- Включите Test Mode (см. выше)

### "WDK not found"
- Убедитесь что установлены SDK и WDK одинаковой версии
- Перезапустите VS/терминал после установки

### "Build failed"
- Откройте Developer Command Prompt for VS 2022
- Проверьте что cargo и rustup доступны

