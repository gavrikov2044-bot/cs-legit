use std::ffi::c_void;
use windows::Win32::System::LibraryLoader::{GetModuleHandleA, GetProcAddress};
use windows::core::PCSTR;

static mut SSN_NT_READ: u32 = 0;

pub fn init() {
    unsafe {
        if let Ok(ntdll) = GetModuleHandleA(PCSTR(b"ntdll.dll\0".as_ptr())) {
            // Find address of NtReadVirtualMemory
            if let Some(func) = GetProcAddress(ntdll, PCSTR(b"NtReadVirtualMemory\0".as_ptr())) {
                let addr = func as *const u8;
                
                // 1. Direct check
                if let Some(ssn) = check_ssn(addr) {
                    SSN_NT_READ = ssn;
                    log::info!("Syscall found direct: {}", ssn);
                    return;
                }

                // 2. Halo's Gate (Neighbor check)
                // Scan 32 bytes up and down to find a clean syscall stub
                for i in 1..32 {
                    // Down
                    if let Some(ssn) = check_ssn(addr.add(i * 32)) {
                        SSN_NT_READ = ssn - i as u32; // Adjust index
                        log::info!("Syscall found via neighbor (down): {}", SSN_NT_READ);
                        return;
                    }
                    // Up
                    if let Some(ssn) = check_ssn(addr.sub(i * 32)) {
                        SSN_NT_READ = ssn + i as u32; // Adjust index
                        log::info!("Syscall found via neighbor (up): {}", SSN_NT_READ);
                        return;
                    }
                }
                
                log::error!("Failed to find SSN for NtReadVirtualMemory!");
            }
        }
    }
}

unsafe fn check_ssn(addr: *const u8) -> Option<u32> {
    // Check for "mov r10, rcx; mov eax, SSN" pattern
    // 4C 8B D1 B8 ...
    if *addr == 0x4C && *addr.add(1) == 0x8B && *addr.add(2) == 0xD1 && *addr.add(3) == 0xB8 {
        let ssn = *(addr.add(4) as *const u32);
        return Some(ssn);
    }
    None
}

pub fn is_active() -> bool {
    unsafe { SSN_NT_READ != 0 }
}

// Assembly stub
use std::arch::global_asm;

// Wrapper to call the asm stub
pub unsafe fn nt_read(
    handle: *mut c_void,
    base: *const c_void,
    buffer: *mut c_void,
    size: usize,
) -> i32 {
    syscall_stub(handle, base, buffer, size, std::ptr::null_mut(), SSN_NT_READ)
}

extern "C" {
    fn syscall_stub(
        handle: *mut c_void,
        base: *const c_void,
        buffer: *mut c_void,
        size: usize,
        written: *mut c_void,
        ssn: u32
    ) -> i32;
}

global_asm!(
    ".section .text",
    ".global syscall_stub",
    "syscall_stub:",
    "mov r10, rcx",         // Prepare syscall: r10 = handle
    "mov eax, [rsp + 48]",  // Load SSN from 6th arg slot
    "syscall",
    "ret"
);
