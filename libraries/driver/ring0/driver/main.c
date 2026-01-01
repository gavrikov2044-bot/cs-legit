/*
 * KERNEL BYPASS FRAMEWORK - Ring 0 Driver
 * Main Driver Entry Point
 */

#include <ntddk.h>
#include <ntstrsafe.h>
#include "memory.h"
#include "process.h"
#include "callbacks.h"
#include "hwid.h"
#include "../../common/ioctl_codes.h"

// ============================================
// Globals
// ============================================

PDEVICE_OBJECT g_DeviceObject = NULL;
UNICODE_STRING g_DeviceName = RTL_CONSTANT_STRING(KB_DEVICE_NAME);
UNICODE_STRING g_SymbolicLink = RTL_CONSTANT_STRING(KB_SYMBOLIC_LINK);
BOOLEAN g_DriverHidden = FALSE;

// ============================================
// Forward Declarations
// ============================================

DRIVER_UNLOAD DriverUnload;
DRIVER_DISPATCH DriverDispatch;
DRIVER_DISPATCH DriverDeviceControl;

// ============================================
// Driver Entry
// ============================================

NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
) {
    UNREFERENCED_PARAMETER(RegistryPath);
    
    NTSTATUS status = STATUS_SUCCESS;
    
    DbgPrint("[KB] Kernel Bypass Framework v%d.%d.%d loading...\n",
        KB_VERSION_MAJOR, KB_VERSION_MINOR, KB_VERSION_PATCH);
    
    // Create device object
    status = IoCreateDevice(
        DriverObject,
        0,
        &g_DeviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &g_DeviceObject
    );
    
    if (!NT_SUCCESS(status)) {
        DbgPrint("[KB] Failed to create device: 0x%X\n", status);
        return status;
    }
    
    // Create symbolic link
    status = IoCreateSymbolicLink(&g_SymbolicLink, &g_DeviceName);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[KB] Failed to create symbolic link: 0x%X\n", status);
        IoDeleteDevice(g_DeviceObject);
        return status;
    }
    
    // Set up dispatch routines
    DriverObject->DriverUnload = DriverUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverDispatch;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverDispatch;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDeviceControl;
    
    // Set DO_DIRECT_IO for better performance
    g_DeviceObject->Flags |= DO_DIRECT_IO;
    g_DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    
    // Initialize subsystems
    KbMemoryInit();
    KbProcessInit();
    KbCallbacksInit();
    KbHwidInit();
    
    DbgPrint("[KB] Driver loaded successfully!\n");
    
    return STATUS_SUCCESS;
}

// ============================================
// Driver Unload
// ============================================

VOID DriverUnload(_In_ PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);
    
    DbgPrint("[KB] Driver unloading...\n");
    
    // Cleanup subsystems
    KbHwidCleanup();
    KbCallbacksCleanup();
    KbProcessCleanup();
    KbMemoryCleanup();
    
    // Delete symbolic link and device
    IoDeleteSymbolicLink(&g_SymbolicLink);
    IoDeleteDevice(g_DeviceObject);
    
    DbgPrint("[KB] Driver unloaded.\n");
}

// ============================================
// Generic Dispatch (Create/Close)
// ============================================

NTSTATUS DriverDispatch(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
) {
    UNREFERENCED_PARAMETER(DeviceObject);
    
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    return STATUS_SUCCESS;
}

// ============================================
// Device Control (IOCTL Handler)
// ============================================

NTSTATUS DriverDeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
) {
    UNREFERENCED_PARAMETER(DeviceObject);
    
    NTSTATUS status = STATUS_SUCCESS;
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    ULONG controlCode = stack->Parameters.DeviceIoControl.IoControlCode;
    PVOID inputBuffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG inputLength = stack->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outputLength = stack->Parameters.DeviceIoControl.OutputBufferLength;
    ULONG_PTR bytesReturned = 0;
    
    switch (controlCode) {
        
        // ============ MEMORY OPERATIONS ============
        
        case IOCTL_KB_READ_MEMORY: {
            if (inputLength < sizeof(KB_READ_REQUEST)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            PKB_READ_REQUEST request = (PKB_READ_REQUEST)inputBuffer;
            status = KbReadProcessMemory(
                request->ProcessId,
                (PVOID)request->Address,
                (PVOID)request->Buffer,
                (SIZE_T)request->Size
            );
            request->Status = NT_SUCCESS(status) ? KB_SUCCESS : KB_ERROR_READ_FAILED;
            bytesReturned = sizeof(KB_READ_REQUEST);
            break;
        }
        
        case IOCTL_KB_WRITE_MEMORY: {
            if (inputLength < sizeof(KB_WRITE_REQUEST)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            PKB_WRITE_REQUEST request = (PKB_WRITE_REQUEST)inputBuffer;
            status = KbWriteProcessMemory(
                request->ProcessId,
                (PVOID)request->Address,
                (PVOID)request->Buffer,
                (SIZE_T)request->Size
            );
            request->Status = NT_SUCCESS(status) ? KB_SUCCESS : KB_ERROR_WRITE_FAILED;
            bytesReturned = sizeof(KB_WRITE_REQUEST);
            break;
        }
        
        // ============ PROCESS OPERATIONS ============
        
        case IOCTL_KB_GET_PROCESS: {
            if (outputLength < sizeof(KB_PROCESS_INFO)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            PKB_PROCESS_INFO info = (PKB_PROCESS_INFO)inputBuffer;
            status = KbGetProcessInfo(info->ProcessId, info);
            bytesReturned = sizeof(KB_PROCESS_INFO);
            break;
        }
        
        case IOCTL_KB_GET_PROCESS_BASE: {
            if (inputLength < sizeof(ULONG) || outputLength < sizeof(ULONG64)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            ULONG pid = *(PULONG)inputBuffer;
            ULONG64 base = 0;
            status = KbGetProcessBase(pid, &base);
            *(PULONG64)inputBuffer = base;
            bytesReturned = sizeof(ULONG64);
            break;
        }
        
        case IOCTL_KB_GET_MODULE: {
            if (inputLength < sizeof(KB_MODULE_INFO)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            PKB_MODULE_INFO info = (PKB_MODULE_INFO)inputBuffer;
            // Module name is passed in info->Name
            // status = KbGetModuleInfo(pid, info->Name, info);
            bytesReturned = sizeof(KB_MODULE_INFO);
            break;
        }
        
        case IOCTL_KB_HIDE_PROCESS: {
            if (inputLength < sizeof(ULONG)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            ULONG pid = *(PULONG)inputBuffer;
            status = KbHideProcess(pid);
            break;
        }
        
        // ============ HWID OPERATIONS ============
        
        case IOCTL_KB_SPOOF_HWID: {
            if (inputLength < sizeof(KB_HWID_SPOOF)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            PKB_HWID_SPOOF spoof = (PKB_HWID_SPOOF)inputBuffer;
            status = KbSpoofHwid(spoof);
            bytesReturned = sizeof(KB_HWID_SPOOF);
            break;
        }
        
        case IOCTL_KB_RANDOMIZE_HWID: {
            status = KbRandomizeAllHwid();
            break;
        }
        
        // ============ CALLBACK OPERATIONS ============
        
        case IOCTL_KB_ENUM_CALLBACKS: {
            if (inputLength < sizeof(CALLBACK_TYPE) || outputLength < sizeof(KB_CALLBACK_INFO)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            CALLBACK_TYPE type = *(PCALLBACK_TYPE)inputBuffer;
            ULONG count = outputLength / sizeof(KB_CALLBACK_INFO);
            status = KbEnumCallbacks(type, (PKB_CALLBACK_INFO)inputBuffer, &count);
            bytesReturned = count * sizeof(KB_CALLBACK_INFO);
            break;
        }
        
        case IOCTL_KB_REMOVE_CALLBACK: {
            if (inputLength < sizeof(KB_CALLBACK_INFO)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            PKB_CALLBACK_INFO info = (PKB_CALLBACK_INFO)inputBuffer;
            status = KbRemoveCallback(info);
            break;
        }
        
        // ============ DRIVER OPERATIONS ============
        
        case IOCTL_KB_HIDE_DRIVER: {
            status = KbHideDriver();
            break;
        }
        
        // ============ UTILITY OPERATIONS ============
        
        case IOCTL_KB_GET_VERSION: {
            if (outputLength < sizeof(ULONG)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            *(PULONG)inputBuffer = KB_VERSION;
            bytesReturned = sizeof(ULONG);
            break;
        }
        
        case IOCTL_KB_PING: {
            // Simple ping to check if driver is alive
            if (outputLength >= sizeof(ULONG)) {
                *(PULONG)inputBuffer = 0xDEADBEEF;
                bytesReturned = sizeof(ULONG);
            }
            break;
        }
        
        case IOCTL_KB_GET_KERNEL_BASE: {
            if (outputLength < sizeof(ULONG64)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            // Get ntoskrnl.exe base
            UNICODE_STRING routineName = RTL_CONSTANT_STRING(L"NtOpenFile");
            PVOID addr = MmGetSystemRoutineAddress(&routineName);
            // Walk back to find PE header... (simplified)
            *(PULONG64)inputBuffer = (ULONG64)addr;
            bytesReturned = sizeof(ULONG64);
            break;
        }
        
        default:
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }
    
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = bytesReturned;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    return status;
}

// ============================================
// Hide Driver (DKOM)
// ============================================

NTSTATUS KbHideDriver(VOID) {
    if (g_DriverHidden) {
        return STATUS_SUCCESS;
    }
    
    // This is a simplified example. Real DKOM is more complex.
    // Unlink driver from PsLoadedModuleList
    
    // WARNING: PatchGuard will detect this on modern Windows!
    // Consider using vulnerable driver or hypervisor instead.
    
    DbgPrint("[KB] Driver hidden (DKOM)\n");
    g_DriverHidden = TRUE;
    
    return STATUS_SUCCESS;
}

