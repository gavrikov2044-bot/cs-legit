# BYOVD Driver Mapper

Стелс-загрузка неподписанных kernel драйверов через уязвимый Intel драйвер.

## ⚠️ ВНИМАНИЕ

**Только для образовательных целей!**

Использование данного кода может:
- Привести к BSOD (синий экран)
- Быть обнаружено античитами
- Нарушать ToS игр
- Быть незаконным в некоторых юрисдикциях

## 🔧 Как это работает

```
1. Загружаем подписанный Intel драйвер (iqvw64e.sys)
2. Используем уязвимость для доступа к памяти ядра
3. Маппим наш драйвер в память ядра
4. Выгружаем Intel драйвер
5. Наш драйвер работает без следов!
```

## 📦 Необходимые файлы

### Intel Driver (iqvw64e.sys)

Скачать можно из:
- Intel Network Adapter Driver package
- kdmapper releases

**SHA256:** Проверьте хэш перед использованием!

## 🚀 Использование

```cpp
#include "driver_mapper.hpp"

int main() {
    mapper::DriverMapper mapper;
    
    // Инициализация с путём к Intel драйверу
    auto status = mapper.Initialize(L"C:\\iqvw64e.sys");
    if (status != mapper::DriverMapper::Status::Success) {
        printf("Init failed: %s\n", mapper.GetLastError().c_str());
        return 1;
    }
    
    // Маппинг нашего драйвера
    status = mapper.MapDriver(L"C:\\my_driver.sys");
    if (status != mapper::DriverMapper::Status::Success) {
        printf("Map failed: %s\n", mapper.GetLastError().c_str());
        return 1;
    }
    
    printf("Driver mapped at: 0x%llX\n", mapper.GetMappedBase());
    
    // Cleanup (удаляет Intel драйвер)
    mapper.Cleanup();
    
    return 0;
}
```

## 🛡️ Anti-Detection Features

### Обфускация строк
```cpp
// Строки шифруются в compile-time
auto name = OBF("iqvw64e.sys");  // XOR encrypted
```

### Dynamic API Resolution
```cpp
// API разрешаются динамически, не через IAT
api::g_apis.NtLoadDriver(...);
```

### Randomization
- Случайное имя сервиса
- Случайные задержки
- Случайные timestamp'ы файлов

### Checks
- Проверка на VM
- Проверка на debugger
- Проверка на security software

## ⚡ Статус реализации

| Компонент | Статус |
|-----------|--------|
| Intel driver loading | ✅ Готово |
| Physical memory R/W | ✅ Готово |
| PE parser | ✅ Готово |
| Symbol resolver | ✅ Готово |
| Memory allocation | ⚠️ TODO |
| Section mapping | ✅ Готово |
| Import resolution | ✅ Готово |
| Relocation fix | ✅ Готово |
| Entry point call | ⚠️ TODO |

## 🔴 TODO

1. **Kernel memory allocation** - нужно вызвать ExAllocatePoolWithTag
2. **Driver entry execution** - нужен APC injection или shellcode
3. **Virtual to Physical translation** - нужно пройти по page tables

## 📚 Ресурсы

- [kdmapper](https://github.com/TheCruZ/kdmapper)
- [Intel driver vulnerability](https://www.cvedetails.com/cve/CVE-2015-2291/)
- [Windows Internals](https://docs.microsoft.com/en-us/sysinternals/)

## 🎮 Совместимость

| Античит | Детект Intel driver? |
|---------|---------------------|
| VAC | ❌ Нет |
| Faceit | ✅ Да (возможно) |
| ESEA | ✅ Да (возможно) |
| EAC | 🟡 Возможно |
| BattlEye | 🟡 Возможно |

**Для CS2 (VAC) — kernel driver НЕ НУЖЕН!** Usermode достаточно.

