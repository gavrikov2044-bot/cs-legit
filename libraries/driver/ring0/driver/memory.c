/*
 * KERNEL BYPASS FRAMEWORK - Ring 0 Driver
 * Memory Operations Implementation
 */

#include "memory.h"
#include "process.h"

// ============================================
// Undocumented Functions (ntoskrnl.exe)
// ============================================

// MmCopyVirtualMemory - THE key function for cross-process memory access
NTKERNELAPI NTSTATUS NTAPI MmCopyVirtualMemory(
    _In_ PEPROCESS SourceProcess,
    _In_ PVOID SourceAddress,
    _In_ PEPROCESS TargetProcess,
    _Out_ PVOID TargetAddress,
    _In_ SIZE_T BufferSize,
    _In_ KPROCESSOR_MODE PreviousMode,
    _Out_ PSIZE_T ReturnSize
);

// ZwProtectVirtualMemory - Memory protection
NTSYSAPI NTSTATUS NTAPI ZwProtectVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _Inout_ PVOID* BaseAddress,
    _Inout_ PSIZE_T RegionSize,
    _In_ ULONG NewProtect,
    _Out_ PULONG OldProtect
);

// ZwAllocateVirtualMemory
NTSYSAPI NTSTATUS NTAPI ZwAllocateVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _Inout_ PVOID* BaseAddress,
    _In_ ULONG_PTR ZeroBits,
    _Inout_ PSIZE_T RegionSize,
    _In_ ULONG AllocationType,
    _In_ ULONG Protect
);

// ZwFreeVirtualMemory
NTSYSAPI NTSTATUS NTAPI ZwFreeVirtualMemory(
    _In_ HANDLE ProcessHandle,
    _Inout_ PVOID* BaseAddress,
    _Inout_ PSIZE_T RegionSize,
    _In_ ULONG FreeType
);

// ============================================
// Globals
// ============================================

static PEPROCESS g_CurrentProcess = NULL;

// ============================================
// Initialization
// ============================================

VOID KbMemoryInit(VOID) {
    g_CurrentProcess = PsGetCurrentProcess();
    DbgPrint("[KB] Memory subsystem initialized\n");
}

VOID KbMemoryCleanup(VOID) {
    g_CurrentProcess = NULL;
    DbgPrint("[KB] Memory subsystem cleaned up\n");
}

// ============================================
// Core Memory Operations
// ============================================

NTSTATUS KbReadProcessMemory(
    _In_ ULONG ProcessId,
    _In_ PVOID SourceAddress,
    _Out_writes_bytes_(Size) PVOID DestinationBuffer,
    _In_ SIZE_T Size
) {
    NTSTATUS status = STATUS_SUCCESS;
    PEPROCESS targetProcess = NULL;
    SIZE_T bytesRead = 0;
    
    // Validate parameters
    if (!SourceAddress || !DestinationBuffer || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    
    // Get target process EPROCESS
    status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)ProcessId, &targetProcess);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[KB] Failed to lookup process %lu: 0x%X\n", ProcessId, status);
        return status;
    }
    
    // Copy memory from target to our buffer
    // SourceProcess = target (where we read from)
    // TargetProcess = current process (where we write to)
    status = MmCopyVirtualMemory(
        targetProcess,          // Source process
        SourceAddress,          // Source address
        g_CurrentProcess,       // Target process (us)
        DestinationBuffer,      // Destination buffer
        Size,                   // Size
        KernelMode,             // Previous mode
        &bytesRead              // Bytes copied
    );
    
    // Dereference process
    ObDereferenceObject(targetProcess);
    
    if (!NT_SUCCESS(status)) {
        // Don't spam debug log for normal failures
        // DbgPrint("[KB] MmCopyVirtualMemory read failed: 0x%X\n", status);
    }
    
    return status;
}

NTSTATUS KbWriteProcessMemory(
    _In_ ULONG ProcessId,
    _In_ PVOID DestinationAddress,
    _In_reads_bytes_(Size) PVOID SourceBuffer,
    _In_ SIZE_T Size
) {
    NTSTATUS status = STATUS_SUCCESS;
    PEPROCESS targetProcess = NULL;
    SIZE_T bytesWritten = 0;
    
    // Validate parameters
    if (!DestinationAddress || !SourceBuffer || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    
    // Get target process EPROCESS
    status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)ProcessId, &targetProcess);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[KB] Failed to lookup process %lu: 0x%X\n", ProcessId, status);
        return status;
    }
    
    // Copy memory from our buffer to target
    // SourceProcess = current process (us)
    // TargetProcess = target (where we write to)
    status = MmCopyVirtualMemory(
        g_CurrentProcess,       // Source process (us)
        SourceBuffer,           // Source buffer
        targetProcess,          // Target process
        DestinationAddress,     // Destination address
        Size,                   // Size
        KernelMode,             // Previous mode
        &bytesWritten           // Bytes copied
    );
    
    // Dereference process
    ObDereferenceObject(targetProcess);
    
    if (!NT_SUCCESS(status)) {
        DbgPrint("[KB] MmCopyVirtualMemory write failed: 0x%X\n", status);
    }
    
    return status;
}

NTSTATUS KbAllocateMemory(
    _In_ ULONG ProcessId,
    _Out_ PVOID* BaseAddress,
    _In_ SIZE_T Size,
    _In_ ULONG AllocationType,
    _In_ ULONG Protect
) {
    NTSTATUS status = STATUS_SUCCESS;
    PEPROCESS targetProcess = NULL;
    KAPC_STATE apcState;
    
    if (!BaseAddress || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    
    status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)ProcessId, &targetProcess);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    // Attach to target process
    KeStackAttachProcess(targetProcess, &apcState);
    
    *BaseAddress = NULL;
    SIZE_T regionSize = Size;
    
    status = ZwAllocateVirtualMemory(
        ZwCurrentProcess(),
        BaseAddress,
        0,
        &regionSize,
        AllocationType,
        Protect
    );
    
    // Detach from target process
    KeUnstackDetachProcess(&apcState);
    ObDereferenceObject(targetProcess);
    
    return status;
}

NTSTATUS KbFreeMemory(
    _In_ ULONG ProcessId,
    _In_ PVOID BaseAddress
) {
    NTSTATUS status = STATUS_SUCCESS;
    PEPROCESS targetProcess = NULL;
    KAPC_STATE apcState;
    
    if (!BaseAddress) {
        return STATUS_INVALID_PARAMETER;
    }
    
    status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)ProcessId, &targetProcess);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    // Attach to target process
    KeStackAttachProcess(targetProcess, &apcState);
    
    SIZE_T regionSize = 0;
    
    status = ZwFreeVirtualMemory(
        ZwCurrentProcess(),
        &BaseAddress,
        &regionSize,
        MEM_RELEASE
    );
    
    // Detach from target process
    KeUnstackDetachProcess(&apcState);
    ObDereferenceObject(targetProcess);
    
    return status;
}

NTSTATUS KbProtectMemory(
    _In_ ULONG ProcessId,
    _In_ PVOID BaseAddress,
    _In_ SIZE_T Size,
    _In_ ULONG NewProtect,
    _Out_opt_ PULONG OldProtect
) {
    NTSTATUS status = STATUS_SUCCESS;
    PEPROCESS targetProcess = NULL;
    KAPC_STATE apcState;
    
    if (!BaseAddress || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    
    status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)ProcessId, &targetProcess);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    // Attach to target process
    KeStackAttachProcess(targetProcess, &apcState);
    
    ULONG oldProtect = 0;
    SIZE_T regionSize = Size;
    
    status = ZwProtectVirtualMemory(
        ZwCurrentProcess(),
        &BaseAddress,
        &regionSize,
        NewProtect,
        &oldProtect
    );
    
    // Detach from target process
    KeUnstackDetachProcess(&apcState);
    ObDereferenceObject(targetProcess);
    
    if (OldProtect) {
        *OldProtect = oldProtect;
    }
    
    return status;
}

// ============================================
// Physical Memory Operations
// ============================================

NTSTATUS KbReadPhysicalMemory(
    _In_ ULONG64 PhysicalAddress,
    _Out_writes_bytes_(Size) PVOID Buffer,
    _In_ SIZE_T Size
) {
    if (!Buffer || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    
    // Map physical memory to virtual
    PHYSICAL_ADDRESS physAddr;
    physAddr.QuadPart = PhysicalAddress;
    
    PVOID mappedAddress = MmMapIoSpace(physAddr, Size, MmNonCached);
    if (!mappedAddress) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    // Copy data
    __try {
        RtlCopyMemory(Buffer, mappedAddress, Size);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        MmUnmapIoSpace(mappedAddress, Size);
        return STATUS_ACCESS_VIOLATION;
    }
    
    // Unmap
    MmUnmapIoSpace(mappedAddress, Size);
    
    return STATUS_SUCCESS;
}

NTSTATUS KbWritePhysicalMemory(
    _In_ ULONG64 PhysicalAddress,
    _In_reads_bytes_(Size) PVOID Buffer,
    _In_ SIZE_T Size
) {
    if (!Buffer || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    
    // Map physical memory to virtual
    PHYSICAL_ADDRESS physAddr;
    physAddr.QuadPart = PhysicalAddress;
    
    PVOID mappedAddress = MmMapIoSpace(physAddr, Size, MmNonCached);
    if (!mappedAddress) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    // Copy data
    __try {
        RtlCopyMemory(mappedAddress, Buffer, Size);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        MmUnmapIoSpace(mappedAddress, Size);
        return STATUS_ACCESS_VIOLATION;
    }
    
    // Unmap
    MmUnmapIoSpace(mappedAddress, Size);
    
    return STATUS_SUCCESS;
}

// Page table structure for x64
#define PML4E_INDEX(va)  (((va) >> 39) & 0x1FF)
#define PDPTE_INDEX(va)  (((va) >> 30) & 0x1FF)
#define PDE_INDEX(va)    (((va) >> 21) & 0x1FF)
#define PTE_INDEX(va)    (((va) >> 12) & 0x1FF)
#define PAGE_OFFSET(va)  ((va) & 0xFFF)

NTSTATUS KbVirtualToPhysical(
    _In_ ULONG64 DirectoryTableBase,
    _In_ ULONG64 VirtualAddress,
    _Out_ PULONG64 PhysicalAddress
) {
    if (!PhysicalAddress) {
        return STATUS_INVALID_PARAMETER;
    }
    
    *PhysicalAddress = 0;
    
    // CR3 contains physical address of PML4
    ULONG64 pml4Base = DirectoryTableBase & ~0xFFF;
    
    // Read PML4 entry
    ULONG64 pml4e = 0;
    NTSTATUS status = KbReadPhysicalMemory(
        pml4Base + PML4E_INDEX(VirtualAddress) * sizeof(ULONG64),
        &pml4e,
        sizeof(pml4e)
    );
    if (!NT_SUCCESS(status) || !(pml4e & 1)) {
        return STATUS_INVALID_ADDRESS;
    }
    
    // Read PDPT entry
    ULONG64 pdptBase = pml4e & 0xFFFFFFFFFF000ULL;
    ULONG64 pdpte = 0;
    status = KbReadPhysicalMemory(
        pdptBase + PDPTE_INDEX(VirtualAddress) * sizeof(ULONG64),
        &pdpte,
        sizeof(pdpte)
    );
    if (!NT_SUCCESS(status) || !(pdpte & 1)) {
        return STATUS_INVALID_ADDRESS;
    }
    
    // Check for 1GB page
    if (pdpte & 0x80) {
        *PhysicalAddress = (pdpte & 0xFFFFFC0000000ULL) + (VirtualAddress & 0x3FFFFFFF);
        return STATUS_SUCCESS;
    }
    
    // Read PD entry
    ULONG64 pdBase = pdpte & 0xFFFFFFFFFF000ULL;
    ULONG64 pde = 0;
    status = KbReadPhysicalMemory(
        pdBase + PDE_INDEX(VirtualAddress) * sizeof(ULONG64),
        &pde,
        sizeof(pde)
    );
    if (!NT_SUCCESS(status) || !(pde & 1)) {
        return STATUS_INVALID_ADDRESS;
    }
    
    // Check for 2MB page
    if (pde & 0x80) {
        *PhysicalAddress = (pde & 0xFFFFFFFE00000ULL) + (VirtualAddress & 0x1FFFFF);
        return STATUS_SUCCESS;
    }
    
    // Read PT entry
    ULONG64 ptBase = pde & 0xFFFFFFFFFF000ULL;
    ULONG64 pte = 0;
    status = KbReadPhysicalMemory(
        ptBase + PTE_INDEX(VirtualAddress) * sizeof(ULONG64),
        &pte,
        sizeof(pte)
    );
    if (!NT_SUCCESS(status) || !(pte & 1)) {
        return STATUS_INVALID_ADDRESS;
    }
    
    // Calculate final physical address
    *PhysicalAddress = (pte & 0xFFFFFFFFFF000ULL) + PAGE_OFFSET(VirtualAddress);
    
    return STATUS_SUCCESS;
}

// ============================================
// Utility Functions
// ============================================

NTSTATUS KbSafeCopy(
    _Out_writes_bytes_(Size) PVOID Destination,
    _In_reads_bytes_(Size) PVOID Source,
    _In_ SIZE_T Size
) {
    __try {
        RtlCopyMemory(Destination, Source, Size);
        return STATUS_SUCCESS;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }
}

NTSTATUS KbPatternScan(
    _In_ PVOID StartAddress,
    _In_ SIZE_T Size,
    _In_ PCUCHAR Pattern,
    _In_ PCUCHAR Mask,
    _In_ SIZE_T PatternSize,
    _Out_ PVOID* FoundAddress
) {
    if (!StartAddress || !Pattern || !Mask || !FoundAddress || PatternSize == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    
    *FoundAddress = NULL;
    
    PUCHAR data = (PUCHAR)StartAddress;
    
    for (SIZE_T i = 0; i < Size - PatternSize; i++) {
        BOOLEAN found = TRUE;
        
        for (SIZE_T j = 0; j < PatternSize; j++) {
            if (Mask[j] == 'x' && data[i + j] != Pattern[j]) {
                found = FALSE;
                break;
            }
        }
        
        if (found) {
            *FoundAddress = (PVOID)(data + i);
            return STATUS_SUCCESS;
        }
    }
    
    return STATUS_NOT_FOUND;
}

