//! Memory operations
//! Direct kernel memory access

#![allow(dead_code)]

use crate::ntdef::*;

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
    
    let current_process = PsGetCurrentProcess();
    let mut bytes_copied: SIZE_T = 0;
    
    let status = MmCopyVirtualMemory(
        target_process,
        source_address as PVOID,
        current_process,
        buffer as PVOID,
        size,
        0, // KernelMode
        &mut bytes_copied,
    );
    
    if status == STATUS_SUCCESS {
        Ok(bytes_copied)
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
    
    let current_process = PsGetCurrentProcess();
    let mut bytes_copied: SIZE_T = 0;
    
    let status = MmCopyVirtualMemory(
        current_process,
        buffer as PVOID,
        target_process,
        target_address as PVOID,
        size,
        0, // KernelMode
        &mut bytes_copied,
    );
    
    if status == STATUS_SUCCESS {
        Ok(bytes_copied)
    } else {
        Err(status)
    }
}
