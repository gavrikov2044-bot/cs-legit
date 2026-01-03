//! Kernel Driver Memory Reader
//! Uses laith-km-driver for ultra-fast memory reading
//! 
//! Device: \\.\laithdriver
//! IOCTL codes: ATTACH=0x800, READ=0x801, BATCH_READ=0x804

use std::ffi::c_void;
use std::mem;
use std::ptr;
use windows::Win32::Foundation::{HANDLE, CloseHandle, INVALID_HANDLE_VALUE};
use windows::Win32::Storage::FileSystem::{
    CreateFileW, FILE_SHARE_READ, FILE_SHARE_WRITE, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
};
use windows::Win32::System::IO::DeviceIoControl;
use windows::core::PCWSTR;

// IOCTL codes from driver
const IOCTL_ATTACH: u32 = ctl_code(0x800);
const IOCTL_READ: u32 = ctl_code(0x801);
const IOCTL_GET_MODULE: u32 = ctl_code(0x802);
#[allow(dead_code)]
const IOCTL_GET_PID: u32 = ctl_code(0x803);
const IOCTL_BATCH_READ: u32 = ctl_code(0x804);

const fn ctl_code(function: u32) -> u32 {
    // CTL_CODE(FILE_DEVICE_UNKNOWN=0x22, function, METHOD_BUFFERED=0, FILE_SPECIAL_ACCESS=0)
    (0x22 << 16) | (0 << 14) | (function << 2) | 0
}

/// Request structure for single memory read
#[repr(C)]
struct ReadRequest {
    process_id: usize,  // HANDLE
    target: usize,      // PVOID
    buffer: usize,      // PVOID  
    size: usize,        // SIZE_T
}

/// Module pack for getting module base
#[repr(C)]
struct ModulePack {
    pid: u32,
    base_address: u64,
    size: usize,
    module_name: [u16; 1024],
}

/// Batch read header
#[repr(C)]
struct BatchReadHeader {
    process_id: usize,       // HANDLE
    num_requests: u32,
    total_buffer_size: usize,
}

/// Individual batch read request
#[repr(C)]
struct BatchReadRequest {
    address: u64,
    size: usize,
    offset_in_buffer: usize,
}

/// Driver-based memory reader
pub struct DriverReader {
    handle: HANDLE,
    pid: u32,
    attached: bool,
}

impl DriverReader {
    /// Connect to the kernel driver
    pub fn connect() -> Option<Self> {
        unsafe {
            let device_name: Vec<u16> = "\\\\.\\laithdriver\0".encode_utf16().collect();
            
            let handle = CreateFileW(
                PCWSTR::from_raw(device_name.as_ptr()),
                0xC0000000, // GENERIC_READ | GENERIC_WRITE
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                None,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                None,
            ).ok()?;
            
            if handle == INVALID_HANDLE_VALUE {
                log::warn!("[Driver] Failed to open driver handle");
                return None;
            }
            
            log::info!("[Driver] Connected to laithdriver");
            
            Some(Self {
                handle,
                pid: 0,
                attached: false,
            })
        }
    }
    
    /// Get module base address using driver
    pub fn get_module_base(&self, pid: u32, module_name: &str) -> Option<usize> {
        if self.handle == INVALID_HANDLE_VALUE {
            return None;
        }
        
        unsafe {
            let mut pack = ModulePack {
                pid,
                base_address: 0,
                size: 0,
                module_name: [0u16; 1024],
            };
            
            // Convert module name to wide string
            for (i, c) in module_name.encode_utf16().enumerate() {
                if i >= 1023 { break; }
                pack.module_name[i] = c;
            }
            
            let mut bytes_returned = 0u32;
            let success = DeviceIoControl(
                self.handle,
                IOCTL_GET_MODULE,
                Some(&pack as *const _ as *const c_void),
                mem::size_of::<ModulePack>() as u32,
                Some(&mut pack as *mut _ as *mut c_void),
                mem::size_of::<ModulePack>() as u32,
                Some(&mut bytes_returned),
                None,
            );
            
            if success.is_ok() && pack.base_address != 0 {
                Some(pack.base_address as usize)
            } else {
                None
            }
        }
    }
    
    /// Attach to a process
    pub fn attach(&mut self, pid: u32) -> bool {
        if self.handle == INVALID_HANDLE_VALUE {
            return false;
        }
        
        unsafe {
            let mut request = ReadRequest {
                process_id: pid as usize,
                target: 0,
                buffer: 0,
                size: 0,
            };
            
            let mut bytes_returned = 0u32;
            let success = DeviceIoControl(
                self.handle,
                IOCTL_ATTACH,
                Some(&request as *const _ as *const c_void),
                mem::size_of::<ReadRequest>() as u32,
                Some(&mut request as *mut _ as *mut c_void),
                mem::size_of::<ReadRequest>() as u32,
                Some(&mut bytes_returned),
                None,
            );
            
            if success.is_ok() {
                self.pid = pid;
                self.attached = true;
                log::info!("[Driver] Attached to PID: {}", pid);
                true
            } else {
                log::warn!("[Driver] Failed to attach to PID: {}", pid);
                false
            }
        }
    }
    
    /// Read memory using driver (single read)
    pub fn read_raw(&self, address: usize, buffer: &mut [u8]) -> bool {
        if !self.attached || self.handle == INVALID_HANDLE_VALUE {
            return false;
        }
        
        unsafe {
            let mut request = ReadRequest {
                process_id: self.pid as usize,
                target: address,
                buffer: buffer.as_mut_ptr() as usize,
                size: buffer.len(),
            };
            
            let mut bytes_returned = 0u32;
            let success = DeviceIoControl(
                self.handle,
                IOCTL_READ,
                Some(&request as *const _ as *const c_void),
                mem::size_of::<ReadRequest>() as u32,
                Some(&mut request as *mut _ as *mut c_void),
                mem::size_of::<ReadRequest>() as u32,
                Some(&mut bytes_returned),
                None,
            );
            
            success.is_ok()
        }
    }
    
    /// Batch read multiple memory regions in ONE driver call
    /// This is MUCH faster than multiple single reads
    pub fn batch_read(&self, addresses: &[(usize, usize)], output: &mut [u8]) -> bool {
        if !self.attached || self.handle == INVALID_HANDLE_VALUE || addresses.is_empty() {
            return false;
        }
        
        let num_requests = addresses.len();
        
        // Calculate sizes
        let header_size = mem::size_of::<BatchReadHeader>();
        let requests_size = num_requests * mem::size_of::<BatchReadRequest>();
        let total_input_size = header_size + requests_size + output.len();
        
        // Allocate buffer for IOCTL
        let mut buffer = vec![0u8; total_input_size];
        
        // Fill header
        let header = unsafe { &mut *(buffer.as_mut_ptr() as *mut BatchReadHeader) };
        header.process_id = self.pid as usize;
        header.num_requests = num_requests as u32;
        header.total_buffer_size = output.len();
        
        // Fill requests
        let requests_ptr = unsafe { buffer.as_mut_ptr().add(header_size) as *mut BatchReadRequest };
        let mut current_offset = 0usize;
        
        for (i, &(addr, size)) in addresses.iter().enumerate() {
            unsafe {
                let req = &mut *requests_ptr.add(i);
                req.address = addr as u64;
                req.size = size;
                req.offset_in_buffer = current_offset;
            }
            current_offset += size;
        }
        
        unsafe {
            let mut bytes_returned = 0u32;
            let success = DeviceIoControl(
                self.handle,
                IOCTL_BATCH_READ,
                Some(buffer.as_ptr() as *const c_void),
                total_input_size as u32,
                Some(buffer.as_mut_ptr() as *mut c_void),
                total_input_size as u32,
                Some(&mut bytes_returned),
                None,
            );
            
            if success.is_ok() {
                // Copy output data
                let output_ptr = buffer.as_ptr().add(header_size + requests_size);
                ptr::copy_nonoverlapping(output_ptr, output.as_mut_ptr(), output.len());
                true
            } else {
                false
            }
        }
    }
    
    /// Generic read helper
    pub fn read<T: Copy>(&self, address: usize) -> Option<T> {
        let mut buffer = std::mem::MaybeUninit::<T>::zeroed();
        unsafe {
            if self.read_raw(address, std::slice::from_raw_parts_mut(
                buffer.as_mut_ptr() as *mut u8, 
                std::mem::size_of::<T>()
            )) {
                Some(buffer.assume_init())
            } else {
                None
            }
        }
    }
    
    pub fn is_connected(&self) -> bool {
        self.handle != INVALID_HANDLE_VALUE
    }
    
    pub fn is_attached(&self) -> bool {
        self.attached
    }
}

impl Drop for DriverReader {
    fn drop(&mut self) {
        if self.handle != INVALID_HANDLE_VALUE {
            unsafe { let _ = CloseHandle(self.handle); }
            log::info!("[Driver] Disconnected");
        }
    }
}

// Make it Send+Sync safe
unsafe impl Send for DriverReader {}
unsafe impl Sync for DriverReader {}

