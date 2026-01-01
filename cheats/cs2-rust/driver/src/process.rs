//! Process utilities
//! Find and attach to target process

use alloc::string::String;
use wdk_sys::{
    NTSTATUS, PEPROCESS, HANDLE,
    STATUS_SUCCESS, STATUS_NOT_FOUND,
};

extern "system" {
    /// Lookup process by PID
    fn PsLookupProcessByProcessId(
        process_id: HANDLE,
        process: *mut PEPROCESS,
    ) -> NTSTATUS;
    
    /// Dereference process object
    fn ObDereferenceObject(object: *mut core::ffi::c_void);
    
    /// Get process image name
    fn PsGetProcessImageFileName(process: PEPROCESS) -> *const u8;
}

/// Process handle wrapper with automatic cleanup
pub struct ProcessHandle {
    process: PEPROCESS,
}

impl ProcessHandle {
    /// Open process by PID
    pub fn open(pid: u32) -> Result<Self, NTSTATUS> {
        let mut process: PEPROCESS = core::ptr::null_mut();
        
        let status = unsafe {
            PsLookupProcessByProcessId(pid as HANDLE, &mut process)
        };
        
        if status == STATUS_SUCCESS && !process.is_null() {
            Ok(Self { process })
        } else {
            Err(status)
        }
    }
    
    /// Get raw PEPROCESS pointer
    pub fn as_ptr(&self) -> PEPROCESS {
        self.process
    }
    
    /// Get process name
    pub fn get_name(&self) -> String {
        if self.process.is_null() {
            return String::new();
        }
        
        unsafe {
            let name_ptr = PsGetProcessImageFileName(self.process);
            if name_ptr.is_null() {
                return String::new();
            }
            
            // Read up to 15 chars (EPROCESS limit)
            let mut name = String::new();
            for i in 0..15 {
                let c = *name_ptr.add(i);
                if c == 0 {
                    break;
                }
                name.push(c as char);
            }
            name
        }
    }
}

impl Drop for ProcessHandle {
    fn drop(&mut self) {
        if !self.process.is_null() {
            unsafe {
                ObDereferenceObject(self.process as *mut core::ffi::c_void);
            }
        }
    }
}

/// Find process by name
pub fn find_process_by_name(name: &str) -> Result<ProcessHandle, NTSTATUS> {
    // Iterate through PIDs (simplified approach)
    for pid in (4..=65535).step_by(4) {
        if let Ok(handle) = ProcessHandle::open(pid) {
            let proc_name = handle.get_name();
            if proc_name.to_lowercase() == name.to_lowercase() {
                return Ok(handle);
            }
        }
    }
    
    Err(STATUS_NOT_FOUND)
}

