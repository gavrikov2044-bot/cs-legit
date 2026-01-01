/*
 * KERNEL BYPASS FRAMEWORK - Ring 0 Driver
 * Memory Operations
 */

#pragma once

#include <ntddk.h>
#include "../../common/types.h"

// ============================================
// Initialization
// ============================================

VOID KbMemoryInit(VOID);
VOID KbMemoryCleanup(VOID);

// ============================================
// Core Memory Operations
// ============================================

/*
 * Read memory from another process using MmCopyVirtualMemory
 * This is the most reliable kernel-level memory read method.
 */
NTSTATUS KbReadProcessMemory(
    _In_ ULONG ProcessId,
    _In_ PVOID SourceAddress,
    _Out_writes_bytes_(Size) PVOID DestinationBuffer,
    _In_ SIZE_T Size
);

/*
 * Write memory to another process using MmCopyVirtualMemory
 */
NTSTATUS KbWriteProcessMemory(
    _In_ ULONG ProcessId,
    _In_ PVOID DestinationAddress,
    _In_reads_bytes_(Size) PVOID SourceBuffer,
    _In_ SIZE_T Size
);

/*
 * Allocate memory in target process
 */
NTSTATUS KbAllocateMemory(
    _In_ ULONG ProcessId,
    _Out_ PVOID* BaseAddress,
    _In_ SIZE_T Size,
    _In_ ULONG AllocationType,
    _In_ ULONG Protect
);

/*
 * Free memory in target process
 */
NTSTATUS KbFreeMemory(
    _In_ ULONG ProcessId,
    _In_ PVOID BaseAddress
);

/*
 * Change memory protection in target process
 */
NTSTATUS KbProtectMemory(
    _In_ ULONG ProcessId,
    _In_ PVOID BaseAddress,
    _In_ SIZE_T Size,
    _In_ ULONG NewProtect,
    _Out_opt_ PULONG OldProtect
);

// ============================================
// Physical Memory Operations
// ============================================

/*
 * Read physical memory directly (CR3 translation)
 * Useful for bypassing certain protections
 */
NTSTATUS KbReadPhysicalMemory(
    _In_ ULONG64 PhysicalAddress,
    _Out_writes_bytes_(Size) PVOID Buffer,
    _In_ SIZE_T Size
);

/*
 * Write physical memory directly
 */
NTSTATUS KbWritePhysicalMemory(
    _In_ ULONG64 PhysicalAddress,
    _In_reads_bytes_(Size) PVOID Buffer,
    _In_ SIZE_T Size
);

/*
 * Translate virtual address to physical using CR3
 */
NTSTATUS KbVirtualToPhysical(
    _In_ ULONG64 DirectoryTableBase,  // CR3
    _In_ ULONG64 VirtualAddress,
    _Out_ PULONG64 PhysicalAddress
);

// ============================================
// Utility Functions
// ============================================

/*
 * Safe copy with SEH
 */
NTSTATUS KbSafeCopy(
    _Out_writes_bytes_(Size) PVOID Destination,
    _In_reads_bytes_(Size) PVOID Source,
    _In_ SIZE_T Size
);

/*
 * Pattern scan in kernel memory
 */
NTSTATUS KbPatternScan(
    _In_ PVOID StartAddress,
    _In_ SIZE_T Size,
    _In_ PCUCHAR Pattern,
    _In_ PCUCHAR Mask,
    _In_ SIZE_T PatternSize,
    _Out_ PVOID* FoundAddress
);

