/*
 * KERNEL BYPASS FRAMEWORK - Ring 0 Driver
 * Anti-Cheat Callback Manipulation
 */

#pragma once

#include <ntddk.h>
#include "../../common/types.h"

// ============================================
// Initialization
// ============================================

VOID KbCallbacksInit(VOID);
VOID KbCallbacksCleanup(VOID);

// ============================================
// Callback Enumeration
// ============================================

/*
 * Enumerate registered callbacks of specified type
 * Returns list of callbacks with their addresses and owning modules
 */
NTSTATUS KbEnumCallbacks(
    _In_ CALLBACK_TYPE Type,
    _Out_writes_(Count) PKB_CALLBACK_INFO Callbacks,
    _Inout_ PULONG Count
);

/*
 * Get callback count for specified type
 */
NTSTATUS KbGetCallbackCount(
    _In_ CALLBACK_TYPE Type,
    _Out_ PULONG Count
);

// ============================================
// Callback Manipulation
// ============================================

/*
 * Remove (disable) a specific callback
 * The callback is not actually removed, but replaced with a stub
 */
NTSTATUS KbRemoveCallback(
    _In_ PKB_CALLBACK_INFO CallbackInfo
);

/*
 * Restore a previously removed callback
 */
NTSTATUS KbRestoreCallback(
    _In_ PKB_CALLBACK_INFO CallbackInfo
);

/*
 * Disable all callbacks of specified type
 */
NTSTATUS KbDisableAllCallbacks(
    _In_ CALLBACK_TYPE Type
);

/*
 * Restore all callbacks of specified type
 */
NTSTATUS KbRestoreAllCallbacks(
    _In_ CALLBACK_TYPE Type
);

// ============================================
// Anti-Cheat Specific
// ============================================

/*
 * Remove all EasyAntiCheat (EAC) callbacks
 */
NTSTATUS KbDisableEAC(VOID);

/*
 * Remove all BattlEye (BE) callbacks
 */
NTSTATUS KbDisableBattlEye(VOID);

/*
 * Remove all Vanguard (Riot) callbacks
 */
NTSTATUS KbDisableVanguard(VOID);

/*
 * Generic: Remove callbacks by module name pattern
 */
NTSTATUS KbDisableCallbacksByModule(
    _In_ PCSTR ModulePattern
);

