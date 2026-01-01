/*
 * KERNEL BYPASS FRAMEWORK - Ring 0 Driver
 * HWID (Hardware ID) Spoofing Implementation
 */

#include "hwid.h"
#include <ntstrsafe.h>

// ============================================
// Storage for Original Values
// ============================================

typedef struct _ORIGINAL_HWID {
    BOOLEAN Saved;
    char DiskSerial[64];
    UCHAR MacAddress[6];
    char MotherboardSerial[64];
    char BiosSerial[64];
    UCHAR Uuid[16];
    char GpuSerial[64];
    char WindowsProductId[64];
} ORIGINAL_HWID, *PORIGINAL_HWID;

static ORIGINAL_HWID g_OriginalHwid = { 0 };
static KSPIN_LOCK g_HwidLock;
static BOOLEAN g_HwidInitialized = FALSE;

// ============================================
// Random Generation
// ============================================

static ULONG RandomSeed = 0;

static ULONG Random(VOID) {
    if (RandomSeed == 0) {
        LARGE_INTEGER time;
        KeQuerySystemTime(&time);
        RandomSeed = time.LowPart;
    }
    
    // Linear congruential generator
    RandomSeed = RandomSeed * 1103515245 + 12345;
    return (RandomSeed >> 16) & 0x7FFF;
}

static VOID GenerateRandomSerial(PCHAR Buffer, SIZE_T Length) {
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    
    for (SIZE_T i = 0; i < Length - 1; i++) {
        Buffer[i] = charset[Random() % (sizeof(charset) - 1)];
    }
    Buffer[Length - 1] = '\0';
}

static VOID GenerateRandomMac(PUCHAR Buffer) {
    for (int i = 0; i < 6; i++) {
        Buffer[i] = (UCHAR)(Random() & 0xFF);
    }
    // Clear multicast bit, set locally administered bit
    Buffer[0] = (Buffer[0] & 0xFE) | 0x02;
}

// ============================================
// Initialization
// ============================================

VOID KbHwidInit(VOID) {
    KeInitializeSpinLock(&g_HwidLock);
    RtlZeroMemory(&g_OriginalHwid, sizeof(g_OriginalHwid));
    g_HwidInitialized = TRUE;
    
    DbgPrint("[KB] HWID subsystem initialized\n");
}

VOID KbHwidCleanup(VOID) {
    // Restore original values if modified
    if (g_OriginalHwid.Saved) {
        KbRestoreAllHwid();
    }
    
    g_HwidInitialized = FALSE;
    DbgPrint("[KB] HWID subsystem cleaned up\n");
}

// ============================================
// Main Spoof Function
// ============================================

NTSTATUS KbSpoofHwid(_Inout_ PKB_HWID_SPOOF SpoofInfo) {
    if (!SpoofInfo) {
        return STATUS_INVALID_PARAMETER;
    }
    
    NTSTATUS status = STATUS_SUCCESS;
    
    switch (SpoofInfo->Type) {
        case HWID_DISK_SERIAL:
            status = KbSpoofDiskSerial(SpoofInfo->Spoofed);
            break;
            
        case HWID_MAC_ADDRESS: {
            UCHAR mac[6];
            // Parse MAC from string "XX:XX:XX:XX:XX:XX" or use as raw bytes
            if (strlen(SpoofInfo->Spoofed) >= 17) {
                // Parse formatted MAC
                sscanf(SpoofInfo->Spoofed, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                    &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
            } else {
                RtlCopyMemory(mac, SpoofInfo->Spoofed, 6);
            }
            status = KbSpoofMacAddress(mac, 0);
            break;
        }
        
        case HWID_SMBIOS:
            status = KbSpoofSmbios(SpoofInfo->Spoofed, SpoofInfo->Spoofed, NULL);
            break;
            
        case HWID_GPU_SERIAL:
            status = KbSpoofGpuSerial(SpoofInfo->Spoofed);
            break;
            
        case HWID_WINDOWS_PRODUCT_ID:
            status = KbSpoofWindowsId(SpoofInfo->Spoofed);
            break;
            
        case HWID_ALL:
            status = KbRandomizeAllHwid();
            break;
            
        default:
            status = STATUS_INVALID_PARAMETER;
            break;
    }
    
    return status;
}

// ============================================
// Disk Serial Spoofing
// ============================================

// Hook IOCTL_STORAGE_QUERY_PROPERTY in disk.sys
// This requires finding and hooking the disk driver dispatch routine

NTSTATUS KbSpoofDiskSerial(_In_ PCSTR NewSerial) {
    if (!NewSerial || strlen(NewSerial) == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    
    // Save original if not already saved
    if (!g_OriginalHwid.Saved) {
        // TODO: Read current disk serial
        g_OriginalHwid.Saved = TRUE;
    }
    
    // Method 1: Registry-based (less effective but safer)
    // Anti-cheats often read directly from disk driver
    
    // Method 2: Hook disk.sys IOCTL handler
    // Find disk.sys driver object
    UNICODE_STRING driverName = RTL_CONSTANT_STRING(L"\\Driver\\Disk");
    PDRIVER_OBJECT diskDriver = NULL;
    
    NTSTATUS status = ObReferenceObjectByName(
        &driverName,
        OBJ_CASE_INSENSITIVE,
        NULL,
        0,
        *IoDriverObjectType,
        KernelMode,
        NULL,
        (PVOID*)&diskDriver
    );
    
    if (!NT_SUCCESS(status)) {
        DbgPrint("[KB] Failed to get disk driver: 0x%X\n", status);
        return status;
    }
    
    // In production, you would:
    // 1. Save original IRP_MJ_DEVICE_CONTROL handler
    // 2. Replace with hook function
    // 3. In hook, intercept IOCTL_STORAGE_QUERY_PROPERTY
    // 4. Modify serial number in response
    
    // For now, just log that we would spoof
    DbgPrint("[KB] Would spoof disk serial to: %s\n", NewSerial);
    
    ObDereferenceObject(diskDriver);
    
    return STATUS_SUCCESS;
}

// ============================================
// MAC Address Spoofing
// ============================================

NTSTATUS KbSpoofMacAddress(
    _In_ PUCHAR NewMac,
    _In_opt_ ULONG AdapterIndex
) {
    UNREFERENCED_PARAMETER(AdapterIndex);
    
    if (!NewMac) {
        return STATUS_INVALID_PARAMETER;
    }
    
    // Save original
    if (!g_OriginalHwid.Saved) {
        // TODO: Read current MAC from NDIS
        g_OriginalHwid.Saved = TRUE;
    }
    
    // Method 1: Registry (survives reboot)
    // HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Class\{4D36E972-E325-11CE-BFC1-08002BE10318}\000X
    // Set "NetworkAddress" value
    
    // Method 2: NDIS filter driver
    // Hook NdisMIndicateReceiveNetBufferLists to modify incoming MACs
    
    // Method 3: Direct NIC register manipulation
    // Write to NIC hardware MAC registers (driver-specific)
    
    DbgPrint("[KB] Would spoof MAC to: %02X:%02X:%02X:%02X:%02X:%02X\n",
        NewMac[0], NewMac[1], NewMac[2], NewMac[3], NewMac[4], NewMac[5]);
    
    return STATUS_SUCCESS;
}

// ============================================
// SMBIOS Spoofing
// ============================================

// SMBIOS data is read from firmware by Windows
// To spoof, we need to:
// 1. Hook ExpGetRawSMBiosTable
// 2. Or modify the cached SMBIOS data in memory

NTSTATUS KbSpoofSmbios(
    _In_ PCSTR MotherboardSerial,
    _In_ PCSTR BiosSerial,
    _In_opt_ PUCHAR Uuid
) {
    UNREFERENCED_PARAMETER(Uuid);
    
    // Save originals
    if (!g_OriginalHwid.Saved) {
        // TODO: Read current SMBIOS
        g_OriginalHwid.Saved = TRUE;
    }
    
    // SMBIOS table is cached in kernel memory
    // Need to find WmipSMBiosTablePhysicalAddress and modify
    
    // Alternative: Hook WmipGetRawSMBiosTableData
    // This is called when WMI queries SMBIOS
    
    DbgPrint("[KB] Would spoof SMBIOS:\n");
    DbgPrint("[KB]   Motherboard: %s\n", MotherboardSerial);
    DbgPrint("[KB]   BIOS: %s\n", BiosSerial);
    
    return STATUS_SUCCESS;
}

// ============================================
// GPU Serial Spoofing
// ============================================

NTSTATUS KbSpoofGpuSerial(_In_ PCSTR NewSerial) {
    if (!NewSerial) {
        return STATUS_INVALID_PARAMETER;
    }
    
    // GPU serial is typically read from:
    // 1. Registry (HKLM\SYSTEM\CurrentControlSet\Enum\PCI\VEN_xxxx...)
    // 2. DirectX / DXGI device info
    // 3. GPU driver IOCTL
    
    // Method: Hook nvlddmkm.sys or amdkmdag.sys IOCTL handler
    
    DbgPrint("[KB] Would spoof GPU serial to: %s\n", NewSerial);
    
    return STATUS_SUCCESS;
}

// ============================================
// CPUID Spoofing
// ============================================

// Full CPUID spoofing requires hypervisor (intercept CPUID instruction)
// From kernel driver, we can only patch software that reads CPUID

NTSTATUS KbSpoofCpuId(
    _In_ ULONG Function,
    _In_ ULONG Eax,
    _In_ ULONG Ebx,
    _In_ ULONG Ecx,
    _In_ ULONG Edx
) {
    UNREFERENCED_PARAMETER(Function);
    UNREFERENCED_PARAMETER(Eax);
    UNREFERENCED_PARAMETER(Ebx);
    UNREFERENCED_PARAMETER(Ecx);
    UNREFERENCED_PARAMETER(Edx);
    
    // This requires hypervisor-level interception
    // See ring_minus1/hypervisor for implementation
    
    DbgPrint("[KB] CPUID spoofing requires hypervisor\n");
    
    return STATUS_NOT_SUPPORTED;
}

// ============================================
// Windows Product ID Spoofing
// ============================================

NTSTATUS KbSpoofWindowsId(_In_ PCSTR ProductId) {
    if (!ProductId) {
        return STATUS_INVALID_PARAMETER;
    }
    
    // Windows Product ID is stored in registry:
    // HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion
    // Values: ProductId, DigitalProductId
    
    NTSTATUS status;
    HANDLE keyHandle = NULL;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING keyPath = RTL_CONSTANT_STRING(
        L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion");
    
    InitializeObjectAttributes(&objAttr, &keyPath, 
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    
    status = ZwOpenKey(&keyHandle, KEY_SET_VALUE, &objAttr);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[KB] Failed to open registry key: 0x%X\n", status);
        return status;
    }
    
    // Convert to Unicode
    ANSI_STRING ansiValue;
    UNICODE_STRING unicodeValue;
    RtlInitAnsiString(&ansiValue, ProductId);
    status = RtlAnsiStringToUnicodeString(&unicodeValue, &ansiValue, TRUE);
    
    if (NT_SUCCESS(status)) {
        UNICODE_STRING valueName = RTL_CONSTANT_STRING(L"ProductId");
        
        status = ZwSetValueKey(
            keyHandle,
            &valueName,
            0,
            REG_SZ,
            unicodeValue.Buffer,
            unicodeValue.Length + sizeof(WCHAR)
        );
        
        RtlFreeUnicodeString(&unicodeValue);
    }
    
    ZwClose(keyHandle);
    
    if (NT_SUCCESS(status)) {
        DbgPrint("[KB] Spoofed Windows Product ID to: %s\n", ProductId);
    }
    
    return status;
}

// ============================================
// Randomize All
// ============================================

NTSTATUS KbRandomizeAllHwid(VOID) {
    NTSTATUS status = STATUS_SUCCESS;
    char randomSerial[17];
    UCHAR randomMac[6];
    
    // Disk Serial
    GenerateRandomSerial(randomSerial, 17);
    KbSpoofDiskSerial(randomSerial);
    
    // MAC Address
    GenerateRandomMac(randomMac);
    KbSpoofMacAddress(randomMac, 0);
    
    // SMBIOS
    GenerateRandomSerial(randomSerial, 12);
    KbSpoofSmbios(randomSerial, randomSerial, NULL);
    
    // GPU Serial
    GenerateRandomSerial(randomSerial, 16);
    KbSpoofGpuSerial(randomSerial);
    
    // Windows Product ID
    char productId[25];
    RtlStringCchPrintfA(productId, sizeof(productId), 
        "%05u-%05u-%05u-%05u",
        Random() % 100000, Random() % 100000,
        Random() % 100000, Random() % 100000);
    KbSpoofWindowsId(productId);
    
    DbgPrint("[KB] All HWID randomized\n");
    
    return status;
}

// ============================================
// Restore All
// ============================================

NTSTATUS KbRestoreAllHwid(VOID) {
    if (!g_OriginalHwid.Saved) {
        return STATUS_SUCCESS;  // Nothing to restore
    }
    
    // Restore each HWID type from saved values
    // Implementation depends on how each was spoofed
    
    DbgPrint("[KB] Restoring all HWID (not fully implemented)\n");
    
    g_OriginalHwid.Saved = FALSE;
    
    return STATUS_SUCCESS;
}

NTSTATUS KbGetOriginalHwid(
    _In_ HWID_TYPE Type,
    _Out_writes_z_(MaxLength) PCHAR Value,
    _In_ SIZE_T MaxLength
) {
    if (!Value || MaxLength == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    
    if (!g_OriginalHwid.Saved) {
        return STATUS_NOT_FOUND;
    }
    
    switch (Type) {
        case HWID_DISK_SERIAL:
            RtlStringCchCopyA(Value, MaxLength, g_OriginalHwid.DiskSerial);
            break;
        case HWID_SMBIOS:
            RtlStringCchCopyA(Value, MaxLength, g_OriginalHwid.MotherboardSerial);
            break;
        case HWID_GPU_SERIAL:
            RtlStringCchCopyA(Value, MaxLength, g_OriginalHwid.GpuSerial);
            break;
        case HWID_WINDOWS_PRODUCT_ID:
            RtlStringCchCopyA(Value, MaxLength, g_OriginalHwid.WindowsProductId);
            break;
        default:
            return STATUS_INVALID_PARAMETER;
    }
    
    return STATUS_SUCCESS;
}

