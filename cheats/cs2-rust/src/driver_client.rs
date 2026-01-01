//! Driver client module
//! Communicates with kernel driver via DeviceIoControl

use std::ffi::c_void;
use std::mem;

use anyhow::{anyhow, Result};
use windows::core::PCWSTR;
use windows::Win32::Foundation::{HANDLE, CloseHandle, GENERIC_READ, GENERIC_WRITE};
use windows::Win32::Storage::FileSystem::{
    CreateFileW, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ, FILE_SHARE_WRITE, OPEN_EXISTING,
};
use windows::Win32::System::IO::DeviceIoControl;

/// IOCTL codes (must match driver)
const IOCTL_READ_MEMORY: u32 = 0x00222000;  // CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, 0)
const IOCTL_WRITE_MEMORY: u32 = 0x00222004; // CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, 0)

/// Memory request structure (must match driver)
#[repr(C)]
#[derive(Default)]
struct MemoryRequest {
    pid: u32,
    address: u64,
    buffer: u64,
    size: u64,
}

/// Driver client for kernel-mode memory operations
pub struct DriverClient {
    handle: HANDLE,
    pid: u32,
}

impl DriverClient {
    /// Connect to driver
    pub fn connect() -> Result<Self> {
        let device_path: Vec<u16> = "\\\\.\\ExternaDrv\0".encode_utf16().collect();
        
        let handle = unsafe {
            CreateFileW(
                PCWSTR(device_path.as_ptr()),
                (GENERIC_READ | GENERIC_WRITE).0,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                None,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                None,
            )?
        };
        
        log::info!("[Driver] Connected to kernel driver");
        
        Ok(Self { handle, pid: 0 })
    }
    
    /// Set target process PID
    pub fn set_target(&mut self, pid: u32) {
        self.pid = pid;
        log::info!("[Driver] Target PID: {}", pid);
    }
    
    /// Read memory from target process
    pub fn read<T: Copy + Default>(&self, address: u64) -> T {
        let mut buffer = T::default();
        let _ = self.read_raw(address, &mut buffer as *mut T as *mut u8, mem::size_of::<T>());
        buffer
    }
    
    /// Read raw bytes
    pub fn read_raw(&self, address: u64, buffer: *mut u8, size: usize) -> Result<usize> {
        let request = MemoryRequest {
            pid: self.pid,
            address,
            buffer: buffer as u64,
            size: size as u64,
        };
        
        let mut bytes_returned: u32 = 0;
        
        let success = unsafe {
            DeviceIoControl(
                self.handle,
                IOCTL_READ_MEMORY,
                Some(&request as *const MemoryRequest as *const c_void),
                mem::size_of::<MemoryRequest>() as u32,
                None,
                0,
                Some(&mut bytes_returned),
                None,
            )
        };
        
        if success.is_ok() {
            Ok(size)
        } else {
            Err(anyhow!("DeviceIoControl failed"))
        }
    }
    
    /// Write memory to target process
    pub fn write_raw(&self, address: u64, buffer: *const u8, size: usize) -> Result<usize> {
        let request = MemoryRequest {
            pid: self.pid,
            address,
            buffer: buffer as u64,
            size: size as u64,
        };
        
        let mut bytes_returned: u32 = 0;
        
        let success = unsafe {
            DeviceIoControl(
                self.handle,
                IOCTL_WRITE_MEMORY,
                Some(&request as *const MemoryRequest as *const c_void),
                mem::size_of::<MemoryRequest>() as u32,
                None,
                0,
                Some(&mut bytes_returned),
                None,
            )
        };
        
        if success.is_ok() {
            Ok(size)
        } else {
            Err(anyhow!("DeviceIoControl write failed"))
        }
    }
    
    /// Check if driver is available
    pub fn is_available() -> bool {
        Self::connect().is_ok()
    }
}

impl Drop for DriverClient {
    fn drop(&mut self) {
        unsafe {
            let _ = CloseHandle(self.handle);
        }
        log::info!("[Driver] Disconnected");
    }
}

