/*
 * KERNEL BYPASS FRAMEWORK - Ring 0 Driver
 * Process Operations
 */

#pragma once

#include <ntddk.h>
#include "../../common/types.h"

// ============================================
// Initialization
// ============================================

VOID KbProcessInit(VOID);
VOID KbProcessCleanup(VOID);

// ============================================
// Process Information
// ============================================

/*
 * Get detailed process information
 */
NTSTATUS KbGetProcessInfo(
    _In_ ULONG ProcessId,
    _Out_ PKB_PROCESS_INFO Info
);

/*
 * Get process main module base address
 */
NTSTATUS KbGetProcessBase(
    _In_ ULONG ProcessId,
    _Out_ PULONG64 BaseAddress
);

/*
 * Get module info by name
 */
NTSTATUS KbGetModuleInfo(
    _In_ ULONG ProcessId,
    _In_ PCSTR ModuleName,
    _Out_ PKB_MODULE_INFO Info
);

/*
 * Enumerate all modules in process
 */
NTSTATUS KbEnumModules(
    _In_ ULONG ProcessId,
    _Out_ PKB_MODULE_INFO Modules,
    _Inout_ PULONG Count
);

// ============================================
// Process Manipulation (DKOM)
// ============================================

/*
 * Hide process from Task Manager / Process Explorer
 * Uses DKOM (Direct Kernel Object Manipulation)
 * WARNING: PatchGuard may detect this!
 */
NTSTATUS KbHideProcess(
    _In_ ULONG ProcessId
);

/*
 * Unhide previously hidden process
 */
NTSTATUS KbUnhideProcess(
    _In_ ULONG ProcessId
);

/*
 * Protect process from termination
 */
NTSTATUS KbProtectProcess(
    _In_ ULONG ProcessId,
    _In_ BOOLEAN Enable
);

/*
 * Elevate handle access rights
 * Grants PROCESS_ALL_ACCESS to any handle
 */
NTSTATUS KbElevateHandle(
    _In_ ULONG ProcessId,
    _In_ HANDLE TargetHandle,
    _In_ ACCESS_MASK DesiredAccess
);

// ============================================
// Process Utility
// ============================================

/*
 * Get EPROCESS by PID
 */
NTSTATUS KbGetEprocess(
    _In_ ULONG ProcessId,
    _Out_ PEPROCESS* Process
);

/*
 * Get CR3 (DirectoryTableBase) for process
 */
NTSTATUS KbGetProcessCr3(
    _In_ ULONG ProcessId,
    _Out_ PULONG64 Cr3
);

/*
 * Attach to process context
 */
NTSTATUS KbAttachProcess(
    _In_ ULONG ProcessId,
    _Out_ PRKAPC_STATE ApcState
);

/*
 * Detach from process context
 */
VOID KbDetachProcess(
    _In_ PRKAPC_STATE ApcState
);

