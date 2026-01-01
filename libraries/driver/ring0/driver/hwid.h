/*
 * KERNEL BYPASS FRAMEWORK - Ring 0 Driver
 * HWID (Hardware ID) Spoofing
 */

#pragma once

#include <ntddk.h>
#include "../../common/types.h"

// ============================================
// Initialization
// ============================================

VOID KbHwidInit(VOID);
VOID KbHwidCleanup(VOID);

// ============================================
// HWID Spoofing Operations
// ============================================

/*
 * Spoof a specific HWID type
 */
NTSTATUS KbSpoofHwid(
    _Inout_ PKB_HWID_SPOOF SpoofInfo
);

/*
 * Get original HWID value
 */
NTSTATUS KbGetOriginalHwid(
    _In_ HWID_TYPE Type,
    _Out_writes_z_(MaxLength) PCHAR Value,
    _In_ SIZE_T MaxLength
);

/*
 * Randomize all HWID values
 */
NTSTATUS KbRandomizeAllHwid(VOID);

/*
 * Restore all original HWID values
 */
NTSTATUS KbRestoreAllHwid(VOID);

// ============================================
// Specific HWID Operations
// ============================================

/*
 * Spoof disk serial number
 * Affects: SMART data, disk.sys
 */
NTSTATUS KbSpoofDiskSerial(
    _In_ PCSTR NewSerial
);

/*
 * Spoof MAC address
 * Affects: NIC hardware address
 */
NTSTATUS KbSpoofMacAddress(
    _In_ PUCHAR NewMac,  // 6 bytes
    _In_opt_ ULONG AdapterIndex  // 0 for all
);

/*
 * Spoof SMBIOS data
 * Affects: Motherboard/BIOS serial, UUID
 */
NTSTATUS KbSpoofSmbios(
    _In_ PCSTR MotherboardSerial,
    _In_ PCSTR BiosSerial,
    _In_opt_ PUCHAR Uuid  // 16 bytes
);

/*
 * Spoof GPU serial
 * Affects: GPU identifier in registry/driver
 */
NTSTATUS KbSpoofGpuSerial(
    _In_ PCSTR NewSerial
);

/*
 * Spoof CPU ID
 * Affects: CPUID instruction results
 * NOTE: Requires hypervisor for full spoofing
 */
NTSTATUS KbSpoofCpuId(
    _In_ ULONG Function,
    _In_ ULONG Eax,
    _In_ ULONG Ebx,
    _In_ ULONG Ecx,
    _In_ ULONG Edx
);

/*
 * Spoof Windows Product ID
 * Affects: Registry values
 */
NTSTATUS KbSpoofWindowsId(
    _In_ PCSTR ProductId
);

