//! Windows NT Kernel Definitions
//! Pure FFI - no external dependencies

#![allow(dead_code)]

use core::ffi::c_void;

// Basic types
pub type NTSTATUS = i32;
pub type HANDLE = *mut c_void;
pub type PVOID = *mut c_void;
pub type SIZE_T = usize;
pub type ULONG = u32;
pub type USHORT = u16;
pub type UCHAR = u8;
pub type BOOLEAN = u8;
pub type KPROCESSOR_MODE = i8;

// Status codes
pub const STATUS_SUCCESS: NTSTATUS = 0;
pub const STATUS_UNSUCCESSFUL: NTSTATUS = 0xC0000001_u32 as i32;
pub const STATUS_ACCESS_DENIED: NTSTATUS = 0xC0000022_u32 as i32;

// Process types
pub type PEPROCESS = *mut c_void;

// UNICODE_STRING
#[repr(C)]
pub struct UNICODE_STRING {
    pub Length: USHORT,
    pub MaximumLength: USHORT,
    pub Buffer: *mut u16,
}

pub type PUNICODE_STRING = *mut UNICODE_STRING;
pub type PCUNICODE_STRING = *const UNICODE_STRING;

// Driver object
#[repr(C)]
pub struct DRIVER_OBJECT {
    pub Type: i16,
    pub Size: i16,
    pub DeviceObject: *mut c_void,
    pub Flags: ULONG,
    pub DriverStart: PVOID,
    pub DriverSize: ULONG,
    pub DriverSection: PVOID,
    pub DriverExtension: *mut c_void,
    pub DriverName: UNICODE_STRING,
    pub HardwareDatabase: PUNICODE_STRING,
    pub FastIoDispatch: *mut c_void,
    pub DriverInit: *mut c_void,
    pub DriverStartIo: *mut c_void,
    pub DriverUnload: Option<unsafe extern "system" fn(PDRIVER_OBJECT)>,
    pub MajorFunction: [*mut c_void; 28],
}

pub type PDRIVER_OBJECT = *mut DRIVER_OBJECT;

// Kernel functions (imported from ntoskrnl.lib)
extern "system" {
    pub fn PsLookupProcessByProcessId(
        ProcessId: HANDLE,
        Process: *mut PEPROCESS,
    ) -> NTSTATUS;
    
    pub fn ObDereferenceObject(Object: PVOID);
    
    pub fn MmCopyVirtualMemory(
        SourceProcess: PEPROCESS,
        SourceAddress: PVOID,
        TargetProcess: PEPROCESS,
        TargetAddress: PVOID,
        BufferSize: SIZE_T,
        PreviousMode: KPROCESSOR_MODE,
        ReturnSize: *mut SIZE_T,
    ) -> NTSTATUS;
    
    pub fn PsGetCurrentProcess() -> PEPROCESS;
    
    pub fn KeGetCurrentIrql() -> UCHAR;
}

