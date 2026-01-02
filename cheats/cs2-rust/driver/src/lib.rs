//! Externa Kernel Driver
//! Pure FFI implementation - No WDK crate dependency
//! 
//! Build with:
//! cargo +nightly build --release --target x86_64-pc-windows-msvc

#![no_std]
#![no_main]
#![allow(non_snake_case)]
#![allow(non_camel_case_types)]

mod ntdef;
mod memory;

use core::panic::PanicInfo;
use ntdef::*;

/// Driver entry point
#[no_mangle]
pub unsafe extern "system" fn DriverEntry(
    driver: PDRIVER_OBJECT,
    _registry_path: PUNICODE_STRING,
) -> NTSTATUS {
    // Set unload routine
    if !driver.is_null() {
        (*driver).DriverUnload = Some(DriverUnload);
    }
    
    // Log success (via DbgPrint)
    DbgPrint(b"[Externa] Driver loaded!\0".as_ptr() as *const i8);
    
    STATUS_SUCCESS
}

/// Driver unload
unsafe extern "system" fn DriverUnload(_driver: PDRIVER_OBJECT) {
    DbgPrint(b"[Externa] Driver unloaded!\0".as_ptr() as *const i8);
}

/// Panic handler
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

// Kernel functions
extern "system" {
    fn DbgPrint(format: *const i8, ...) -> NTSTATUS;
}
