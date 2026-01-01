//! Externa Kernel Driver
//! Based on Valthrun Zenith Architecture
//! 
//! Features:
//! - Direct memory read via MmCopyVirtualMemory
//! - CR3 Walking for stealth
//! - Process protection bypass

#![no_std]
#![deny(unsafe_op_in_unsafe_fn)]

extern crate alloc;

mod memory;
mod process;
mod communication;

use wdk::println;
use wdk_sys::{
    ntddk::KeGetCurrentIrql,
    DRIVER_OBJECT, NTSTATUS, PCUNICODE_STRING, PDRIVER_OBJECT,
    STATUS_SUCCESS,
};

/// Driver entry point
#[export_name = "DriverEntry"]
pub unsafe extern "system" fn driver_entry(
    driver: PDRIVER_OBJECT,
    registry_path: PCUNICODE_STRING,
) -> NTSTATUS {
    unsafe { driver_main(driver, registry_path) }
}

unsafe fn driver_main(
    driver: PDRIVER_OBJECT,
    _registry_path: PCUNICODE_STRING,
) -> NTSTATUS {
    println!("[Externa] Driver loading...");
    
    // Verify IRQL
    let irql = unsafe { KeGetCurrentIrql() };
    println!("[Externa] Current IRQL: {}", irql);
    
    // Initialize driver unload
    if !driver.is_null() {
        unsafe {
            (*driver).DriverUnload = Some(driver_unload);
        }
    }
    
    // Initialize communication
    if let Err(status) = communication::initialize(driver) {
        println!("[Externa] Failed to initialize communication: 0x{:X}", status);
        return status;
    }
    
    println!("[Externa] Driver loaded successfully!");
    STATUS_SUCCESS
}

/// Driver unload handler
unsafe extern "system" fn driver_unload(driver: PDRIVER_OBJECT) {
    println!("[Externa] Driver unloading...");
    communication::cleanup(driver);
    println!("[Externa] Driver unloaded!");
}

/// Panic handler for no_std
#[panic_handler]
fn panic(info: &core::panic::PanicInfo) -> ! {
    println!("[Externa] PANIC: {:?}", info);
    loop {}
}

/// Allocator for no_std (required by alloc crate)
#[global_allocator]
static ALLOCATOR: wdk::allocator::WdkAllocator = wdk::allocator::WdkAllocator;

