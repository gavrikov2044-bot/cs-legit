use std::ffi::c_void;
use std::mem::size_of;
use std::sync::Arc;
use windows::Win32::Foundation::{HANDLE, CloseHandle};
use windows::Win32::System::Diagnostics::Debug::ReadProcessMemory;
use windows::Win32::System::Threading::{OpenProcess, PROCESS_VM_READ, PROCESS_QUERY_INFORMATION};
use windows::Win32::System::Diagnostics::ToolHelp::{
    CreateToolhelp32Snapshot, Process32FirstW, Process32NextW,
    Module32FirstW, Module32NextW,
    TH32CS_SNAPPROCESS, TH32CS_SNAPMODULE, PROCESSENTRY32W, MODULEENTRY32W,
};

use crate::syscall;

// Wrapper for Send+Sync
struct SendHandle(HANDLE);
unsafe impl Send for SendHandle {}
unsafe impl Sync for SendHandle {}

pub struct Memory {
    handle: Arc<SendHandle>,
    pub client_base: u64,
}

impl Memory {
    pub fn new(process_name: &str) -> Option<Self> {
        syscall::init();

        let pid = Self::get_pid(process_name)?;
        let handle = unsafe { OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, false, pid).ok()? };
        let client_base = Self::get_module_base(pid, "client.dll")?;

        Some(Self {
            handle: Arc::new(SendHandle(handle)),
            client_base
        })
    }

    fn get_pid(name: &str) -> Option<u32> {
        unsafe {
            let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0).ok()?;
            let mut entry = PROCESSENTRY32W { dwSize: size_of::<PROCESSENTRY32W>() as u32, ..Default::default() };
            if Process32FirstW(snapshot, &mut entry).is_ok() {
                loop {
                    let process_name = String::from_utf16_lossy(&entry.szExeFile).trim_matches('\0').to_string();
                    if process_name == name { let _ = CloseHandle(snapshot); return Some(entry.th32ProcessID); }
                    if Process32NextW(snapshot, &mut entry).is_err() { break; }
                }
            }
            let _ = CloseHandle(snapshot);
            None
        }
    }

    fn get_module_base(pid: u32, name: &str) -> Option<u64> {
        unsafe {
            let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE, pid).ok()?;
            let mut entry = MODULEENTRY32W { dwSize: size_of::<MODULEENTRY32W>() as u32, ..Default::default() };
            if Module32FirstW(snapshot, &mut entry).is_ok() {
                loop {
                    let module_name = String::from_utf16_lossy(&entry.szModule).trim_matches('\0').to_string();
                    if module_name == name { let _ = CloseHandle(snapshot); return Some(entry.modBaseAddr as u64); }
                    if Module32NextW(snapshot, &mut entry).is_err() { break; }
                }
            }
            let _ = CloseHandle(snapshot);
            None
        }
    }

    pub fn read<T: Copy>(&self, address: u64) -> T {
        unsafe {
            let mut buffer = std::mem::zeroed::<T>();

            // Use WinAPI directly for now (syscall disabled for testing)
            let result = ReadProcessMemory(
                self.handle.0,
                address as *const c_void,
                &mut buffer as *mut _ as *mut c_void,
                size_of::<T>(),
                None
            );
            
            if result.is_err() && address != 0 {
                // Silent fail - return zeroed
            }
            
            buffer
        }
    }
}

impl Drop for Memory {
    fn drop(&mut self) {
        // Only drop if last reference
        if Arc::strong_count(&self.handle) == 1 {
            unsafe { let _ = CloseHandle(self.handle.0); }
        }
    }
}
