//! Memory operations module
//! Direct kernel memory access via MmCopyVirtualMemory

use wdk_sys::{
    NTSTATUS, PEPROCESS, PVOID, SIZE_T, KPROCESSOR_MODE,
    STATUS_SUCCESS, STATUS_ACCESS_DENIED,
};

extern "system" {
    /// Kernel function for cross-process memory copy
    fn MmCopyVirtualMemory(
        source_process: PEPROCESS,
        source_address: PVOID,
        target_process: PEPROCESS,
        target_address: PVOID,
        buffer_size: SIZE_T,
        previous_mode: KPROCESSOR_MODE,
        return_size: *mut SIZE_T,
    ) -> NTSTATUS;
    
    /// Get current process
    fn PsGetCurrentProcess() -> PEPROCESS;
}

/// Read memory from target process
pub unsafe fn read_process_memory(
    target_process: PEPROCESS,
    source_address: u64,
    buffer: *mut u8,
    size: usize,
) -> Result<usize, NTSTATUS> {
    if target_process.is_null() || buffer.is_null() || size == 0 {
        return Err(STATUS_ACCESS_DENIED);
    }
    
    let current_process = unsafe { PsGetCurrentProcess() };
    let mut bytes_copied: SIZE_T = 0;
    
    let status = unsafe {
        MmCopyVirtualMemory(
            target_process,
            source_address as PVOID,
            current_process,
            buffer as PVOID,
            size as SIZE_T,
            0, // KernelMode
            &mut bytes_copied,
        )
    };
    
    if status == STATUS_SUCCESS {
        Ok(bytes_copied as usize)
    } else {
        Err(status)
    }
}

/// Write memory to target process
pub unsafe fn write_process_memory(
    target_process: PEPROCESS,
    target_address: u64,
    buffer: *const u8,
    size: usize,
) -> Result<usize, NTSTATUS> {
    if target_process.is_null() || buffer.is_null() || size == 0 {
        return Err(STATUS_ACCESS_DENIED);
    }
    
    let current_process = unsafe { PsGetCurrentProcess() };
    let mut bytes_copied: SIZE_T = 0;
    
    let status = unsafe {
        MmCopyVirtualMemory(
            current_process,
            buffer as PVOID,
            target_process,
            target_address as PVOID,
            size as SIZE_T,
            0, // KernelMode
            &mut bytes_copied,
        )
    };
    
    if status == STATUS_SUCCESS {
        Ok(bytes_copied as usize)
    } else {
        Err(status)
    }
}

