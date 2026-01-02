use std::ffi::c_void;
use windows::Win32::Foundation::{HANDLE, CloseHandle};
use windows::Win32::System::Diagnostics::Debug::ReadProcessMemory;
use crate::memory::syscall;

// Wrapper for Send+Sync HANDLE
#[derive(Debug)]
pub struct SendHandle(pub HANDLE);
unsafe impl Send for SendHandle {}
unsafe impl Sync for SendHandle {}

impl Drop for SendHandle {
    fn drop(&mut self) {
        unsafe { let _ = CloseHandle(self.0); }
    }
}

pub trait ProcessReader {
    fn read_raw(&self, address: usize, buffer: &mut [u8]) -> bool;
    
    fn read<T: Copy>(&self, address: usize) -> Option<T> {
        let mut buffer = std::mem::MaybeUninit::<T>::zeroed();
        unsafe {
            if self.read_raw(address, std::slice::from_raw_parts_mut(buffer.as_mut_ptr() as *mut u8, std::mem::size_of::<T>())) {
                Some(buffer.assume_init())
            } else {
                None
            }
        }
    }
}

impl ProcessReader for super::Memory {
    fn read_raw(&self, address: usize, buffer: &mut [u8]) -> bool {
        unsafe {
            // Try Syscall first
            if syscall::is_active() {
                let status = syscall::nt_read(
                    self.handle.0.0 as _,
                    address as _,
                    buffer.as_mut_ptr() as *mut _,
                    buffer.len()
                );
                if status == 0 { return true; }
            }

            // Fallback to WinAPI
            ReadProcessMemory(
                self.handle.0,
                address as *const c_void,
                buffer.as_mut_ptr() as *mut c_void,
                buffer.len(),
                None
            ).is_ok()
        }
    }
}

