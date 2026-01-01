//! Driver-User communication
//! Uses DeviceIoControl for requests

use core::ptr;

use wdk::println;
use wdk_sys::{
    ntddk::{
        IoCreateDevice, IoDeleteDevice, IoCreateSymbolicLink, IoDeleteSymbolicLink,
        IofCompleteRequest,
    },
    DEVICE_OBJECT, DRIVER_OBJECT, IRP, NTSTATUS, PDEVICE_OBJECT, PDRIVER_OBJECT,
    PIRP, UNICODE_STRING,
    STATUS_SUCCESS, STATUS_INVALID_PARAMETER, STATUS_BUFFER_TOO_SMALL,
    IO_NO_INCREMENT, FILE_DEVICE_UNKNOWN, METHOD_BUFFERED,
    IRP_MJ_CREATE, IRP_MJ_CLOSE, IRP_MJ_DEVICE_CONTROL,
};

use crate::memory;
use crate::process::ProcessHandle;

/// Device name
const DEVICE_NAME: &[u16] = &[
    b'\\' as u16, b'D' as u16, b'e' as u16, b'v' as u16, b'i' as u16,
    b'c' as u16, b'e' as u16, b'\\' as u16,
    b'E' as u16, b'x' as u16, b't' as u16, b'e' as u16, b'r' as u16,
    b'n' as u16, b'a' as u16, b'D' as u16, b'r' as u16, b'v' as u16,
    0,
];

/// Symbolic link
const SYMLINK_NAME: &[u16] = &[
    b'\\' as u16, b'?' as u16, b'?' as u16, b'\\' as u16,
    b'E' as u16, b'x' as u16, b't' as u16, b'e' as u16, b'r' as u16,
    b'n' as u16, b'a' as u16, b'D' as u16, b'r' as u16, b'v' as u16,
    0,
];

/// IOCTL codes
const IOCTL_READ_MEMORY: u32 = ctl_code(0x800);
const IOCTL_WRITE_MEMORY: u32 = ctl_code(0x801);
const IOCTL_GET_MODULE: u32 = ctl_code(0x802);

const fn ctl_code(function: u32) -> u32 {
    (FILE_DEVICE_UNKNOWN << 16) | (0 << 14) | (function << 2) | METHOD_BUFFERED
}

/// Memory request structure
#[repr(C)]
pub struct MemoryRequest {
    pub pid: u32,
    pub address: u64,
    pub buffer: u64,
    pub size: u64,
}

/// Global device object
static mut DEVICE_OBJECT: PDEVICE_OBJECT = ptr::null_mut();

/// Initialize communication
pub fn initialize(driver: PDRIVER_OBJECT) -> Result<(), NTSTATUS> {
    if driver.is_null() {
        return Err(STATUS_INVALID_PARAMETER);
    }
    
    unsafe {
        // Create device
        let device_name = create_unicode_string(DEVICE_NAME);
        let mut device: PDEVICE_OBJECT = ptr::null_mut();
        
        let status = IoCreateDevice(
            driver,
            0,
            &device_name as *const UNICODE_STRING,
            FILE_DEVICE_UNKNOWN,
            0,
            0,
            &mut device,
        );
        
        if status != STATUS_SUCCESS {
            return Err(status);
        }
        
        DEVICE_OBJECT = device;
        
        // Create symbolic link
        let symlink = create_unicode_string(SYMLINK_NAME);
        let status = IoCreateSymbolicLink(&symlink, &device_name);
        
        if status != STATUS_SUCCESS {
            IoDeleteDevice(device);
            DEVICE_OBJECT = ptr::null_mut();
            return Err(status);
        }
        
        // Set dispatch routines
        (*driver).MajorFunction[IRP_MJ_CREATE as usize] = Some(dispatch_create_close);
        (*driver).MajorFunction[IRP_MJ_CLOSE as usize] = Some(dispatch_create_close);
        (*driver).MajorFunction[IRP_MJ_DEVICE_CONTROL as usize] = Some(dispatch_ioctl);
        
        println!("[Externa] Device created: \\Device\\ExternaDrv");
    }
    
    Ok(())
}

/// Cleanup communication
pub fn cleanup(driver: PDRIVER_OBJECT) {
    unsafe {
        let symlink = create_unicode_string(SYMLINK_NAME);
        let _ = IoDeleteSymbolicLink(&symlink);
        
        if !DEVICE_OBJECT.is_null() {
            IoDeleteDevice(DEVICE_OBJECT);
            DEVICE_OBJECT = ptr::null_mut();
        }
    }
    let _ = driver;
}

/// Create UNICODE_STRING from u16 slice
unsafe fn create_unicode_string(s: &[u16]) -> UNICODE_STRING {
    let len = s.iter().take_while(|&&c| c != 0).count();
    UNICODE_STRING {
        Length: (len * 2) as u16,
        MaximumLength: (s.len() * 2) as u16,
        Buffer: s.as_ptr() as *mut u16,
    }
}

/// Dispatch for CREATE/CLOSE
unsafe extern "system" fn dispatch_create_close(
    _device: PDEVICE_OBJECT,
    irp: PIRP,
) -> NTSTATUS {
    unsafe {
        (*irp).IoStatus.Anonymous.Status = STATUS_SUCCESS;
        (*irp).IoStatus.Information = 0;
        IofCompleteRequest(irp, IO_NO_INCREMENT as i8);
    }
    STATUS_SUCCESS
}

/// Dispatch for DEVICE_CONTROL
unsafe extern "system" fn dispatch_ioctl(
    _device: PDEVICE_OBJECT,
    irp: PIRP,
) -> NTSTATUS {
    let stack = unsafe { (*irp).Tail.Overlay.Anonymous2.Anonymous.CurrentStackLocation };
    let ioctl_code = unsafe { (*stack).Parameters.DeviceIoControl.IoControlCode };
    let input_buffer = unsafe { (*irp).AssociatedIrp.SystemBuffer };
    let input_size = unsafe { (*stack).Parameters.DeviceIoControl.InputBufferLength };
    
    let status = match ioctl_code {
        IOCTL_READ_MEMORY => {
            if input_size < core::mem::size_of::<MemoryRequest>() as u32 {
                STATUS_BUFFER_TOO_SMALL
            } else {
                handle_read_memory(input_buffer as *const MemoryRequest)
            }
        }
        IOCTL_WRITE_MEMORY => {
            if input_size < core::mem::size_of::<MemoryRequest>() as u32 {
                STATUS_BUFFER_TOO_SMALL
            } else {
                handle_write_memory(input_buffer as *const MemoryRequest)
            }
        }
        _ => STATUS_INVALID_PARAMETER,
    };
    
    unsafe {
        (*irp).IoStatus.Anonymous.Status = status;
        (*irp).IoStatus.Information = 0;
        IofCompleteRequest(irp, IO_NO_INCREMENT as i8);
    }
    
    status
}

/// Handle read memory request
fn handle_read_memory(request: *const MemoryRequest) -> NTSTATUS {
    if request.is_null() {
        return STATUS_INVALID_PARAMETER;
    }
    
    let req = unsafe { &*request };
    
    let process = match ProcessHandle::open(req.pid) {
        Ok(p) => p,
        Err(status) => return status,
    };
    
    match unsafe {
        memory::read_process_memory(
            process.as_ptr(),
            req.address,
            req.buffer as *mut u8,
            req.size as usize,
        )
    } {
        Ok(_) => STATUS_SUCCESS,
        Err(status) => status,
    }
}

/// Handle write memory request
fn handle_write_memory(request: *const MemoryRequest) -> NTSTATUS {
    if request.is_null() {
        return STATUS_INVALID_PARAMETER;
    }
    
    let req = unsafe { &*request };
    
    let process = match ProcessHandle::open(req.pid) {
        Ok(p) => p,
        Err(status) => return status,
    };
    
    match unsafe {
        memory::write_process_memory(
            process.as_ptr(),
            req.address,
            req.buffer as *const u8,
            req.size as usize,
        )
    } {
        Ok(_) => STATUS_SUCCESS,
        Err(status) => status,
    }
}

