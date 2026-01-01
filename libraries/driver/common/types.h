/*
 * KERNEL BYPASS FRAMEWORK
 * Common Type Definitions
 */

#pragma once

#include <stdint.h>

#ifdef _KERNEL_MODE
    #include <ntddk.h>
#else
    #include <Windows.h>
#endif

// ============================================
// Common Types
// ============================================

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef uintptr_t uptr;
typedef intptr_t  iptr;

// ============================================
// Ring Level Identification
// ============================================

typedef enum _RING_LEVEL {
    RING_LEVEL_USER     = 3,    // Ring 3 - User mode
    RING_LEVEL_KERNEL   = 0,    // Ring 0 - Kernel mode
    RING_LEVEL_HYPER    = -1,   // Ring -1 - Hypervisor
    RING_LEVEL_SMM      = -2,   // Ring -2 - System Management Mode
    RING_LEVEL_FIRMWARE = -3,   // Ring -3 - Intel ME / Firmware
} RING_LEVEL;

// ============================================
// Backend Type (for usermode selection)
// ============================================

typedef enum _BACKEND_TYPE {
    BACKEND_AUTO = 0,           // Auto-detect best available
    BACKEND_USERMODE,           // Ring 3 - ReadProcessMemory
    BACKEND_SYSCALL,            // Ring 3 - Direct syscall
    BACKEND_DRIVER,             // Ring 0 - Kernel driver
    BACKEND_VULNERABLE_DRIVER,  // Ring 0 - Exploited driver
    BACKEND_HYPERVISOR,         // Ring -1 - Hypervisor
    BACKEND_SMM,                // Ring -2 - SMM
    BACKEND_FIRMWARE,           // Ring -3 - Firmware/ME
} BACKEND_TYPE;

// ============================================
// Operation Status
// ============================================

typedef enum _KB_STATUS {
    KB_SUCCESS = 0,
    KB_ERROR_GENERIC = 1,
    KB_ERROR_NOT_INITIALIZED = 2,
    KB_ERROR_INVALID_PARAMETER = 3,
    KB_ERROR_ACCESS_DENIED = 4,
    KB_ERROR_PROCESS_NOT_FOUND = 5,
    KB_ERROR_MODULE_NOT_FOUND = 6,
    KB_ERROR_READ_FAILED = 7,
    KB_ERROR_WRITE_FAILED = 8,
    KB_ERROR_DRIVER_NOT_LOADED = 9,
    KB_ERROR_HYPERVISOR_NOT_ACTIVE = 10,
    KB_ERROR_FIRMWARE_NOT_AVAILABLE = 11,
    KB_ERROR_HWID_SPOOF_FAILED = 12,
    KB_ERROR_CALLBACK_NOT_FOUND = 13,
    KB_ERROR_INSUFFICIENT_RESOURCES = 14,
} KB_STATUS;

// ============================================
// Memory Operation Structures
// ============================================

typedef struct _KB_READ_REQUEST {
    u32 ProcessId;
    u64 Address;
    u64 Buffer;         // Pointer to user buffer
    u64 Size;
    KB_STATUS Status;
} KB_READ_REQUEST, *PKB_READ_REQUEST;

typedef struct _KB_WRITE_REQUEST {
    u32 ProcessId;
    u64 Address;
    u64 Buffer;         // Pointer to data to write
    u64 Size;
    KB_STATUS Status;
} KB_WRITE_REQUEST, *PKB_WRITE_REQUEST;

// ============================================
// Process Operation Structures
// ============================================

typedef struct _KB_PROCESS_INFO {
    u32 ProcessId;
    u64 EprocessAddress;    // Kernel EPROCESS pointer
    u64 Cr3;                // DirectoryTableBase
    u64 BaseAddress;        // Main module base
    u32 IsWow64;            // 32-bit on 64-bit
    u32 IsProtected;        // Protected process
    char Name[64];
} KB_PROCESS_INFO, *PKB_PROCESS_INFO;

typedef struct _KB_MODULE_INFO {
    u64 BaseAddress;
    u64 Size;
    char Name[256];
    char Path[512];
} KB_MODULE_INFO, *PKB_MODULE_INFO;

// ============================================
// HWID Spoof Structures
// ============================================

typedef struct _KB_HWID_SPOOF {
    u32 Type;               // What to spoof (see HWID_TYPE)
    char Original[128];     // Original value (output)
    char Spoofed[128];      // New value (input)
    u32 Flags;
} KB_HWID_SPOOF, *PKB_HWID_SPOOF;

typedef enum _HWID_TYPE {
    HWID_DISK_SERIAL = 1,
    HWID_DISK_SMART = 2,
    HWID_MAC_ADDRESS = 3,
    HWID_SMBIOS = 4,
    HWID_GPU_SERIAL = 5,
    HWID_CPU_ID = 6,
    HWID_WINDOWS_PRODUCT_ID = 7,
    HWID_MONITOR_SERIAL = 8,
    HWID_ALL = 0xFF,
} HWID_TYPE;

// ============================================
// Callback Manipulation
// ============================================

typedef struct _KB_CALLBACK_INFO {
    u64 Address;            // Callback function address
    u64 ContextAddress;     // Associated context
    char ModuleName[64];    // Owning module
    u32 Type;               // Callback type
    u32 Index;              // Index in array
} KB_CALLBACK_INFO, *PKB_CALLBACK_INFO;

typedef enum _CALLBACK_TYPE {
    CALLBACK_PROCESS_NOTIFY = 1,    // PsSetCreateProcessNotifyRoutine
    CALLBACK_THREAD_NOTIFY = 2,     // PsSetCreateThreadNotifyRoutine
    CALLBACK_IMAGE_NOTIFY = 3,      // PsSetLoadImageNotifyRoutine
    CALLBACK_REGISTRY = 4,          // CmRegisterCallback
    CALLBACK_OBJECT = 5,            // ObRegisterCallbacks
    CALLBACK_MINIFILTER = 6,        // FltRegisterFilter
} CALLBACK_TYPE;

// ============================================
// Hypervisor Structures
// ============================================

typedef struct _KB_VMCALL_REQUEST {
    u64 VmcallNumber;       // Hypercall ID
    u64 Param1;
    u64 Param2;
    u64 Param3;
    u64 Param4;
    u64 ReturnValue;
    KB_STATUS Status;
} KB_VMCALL_REQUEST, *PKB_VMCALL_REQUEST;

typedef struct _KB_EPT_HOOK {
    u64 TargetAddress;      // Address to hook
    u64 HookFunction;       // Replacement function
    u64 OriginalBytes[2];   // Saved original code
    u32 Active;
} KB_EPT_HOOK, *PKB_EPT_HOOK;

// ============================================
// Firmware Structures
// ============================================

typedef struct _KB_FIRMWARE_INFO {
    u32 Vendor;             // Intel, AMD, etc.
    u32 MeVersion[4];       // ME firmware version
    u64 SmmBase;            // SMRAM base address
    u64 SmmSize;            // SMRAM size
    u32 SecureBootEnabled;
    u32 UefiSecurityLevel;
} KB_FIRMWARE_INFO, *PKB_FIRMWARE_INFO;

// ============================================
// Macros
// ============================================

#define KB_DEVICE_NAME      L"\\Device\\KernelBypass"
#define KB_SYMBOLIC_LINK    L"\\DosDevices\\KernelBypass"
#define KB_DRIVER_NAME      L"\\Driver\\KernelBypass"

#define KB_ALIGN(x, a)      (((x) + ((a) - 1)) & ~((a) - 1))
#define KB_PAGE_SIZE        0x1000
#define KB_PAGE_ALIGN(x)    KB_ALIGN(x, KB_PAGE_SIZE)

#define KB_MAX_PROCESSES    1024
#define KB_MAX_MODULES      512
#define KB_MAX_CALLBACKS    256

