use std::ffi::c_void;
use windows::Win32::System::LibraryLoader::{GetModuleHandleA, GetProcAddress};
use windows::core::PCSTR;
use std::arch::global_asm;

static mut SSN_NT_READ: u32 = 0;

pub fn init() {
    unsafe {
        if let Ok(ntdll) = GetModuleHandleA(PCSTR(b"ntdll.dll\0".as_ptr())) {
            if let Some(func) = GetProcAddress(ntdll, PCSTR(b"NtReadVirtualMemory\0".as_ptr())) {
                let addr = func as *const u8;
                
                // Direct check: 4C 8B D1 B8 ...
                if *addr == 0x4C && *addr.add(1) == 0x8B && *addr.add(2) == 0xD1 && *addr.add(3) == 0xB8 {
                    SSN_NT_READ = *(addr.add(4) as *const u32);
                    log::info!("[Syscall] Direct SSN found: {}", SSN_NT_READ);
                    return;
                }
                
                // Halo's Gate (Neighbor check)
                for i in 1..32 {
                    if let Some(ssn) = check_ssn(addr.add(i * 32)) {
                        SSN_NT_READ = ssn - i as u32;
                        log::info!("[Syscall] Neighbor (down) SSN found: {}", SSN_NT_READ);
                        return;
                    }
                    if let Some(ssn) = check_ssn(addr.sub(i * 32)) {
                        SSN_NT_READ = ssn + i as u32;
                        log::info!("[Syscall] Neighbor (up) SSN found: {}", SSN_NT_READ);
                        return;
                    }
                }
                log::warn!("[Syscall] Failed to find SSN!");
            }
        }
    }
}

unsafe fn check_ssn(addr: *const u8) -> Option<u32> {
    if *addr == 0x4C && *addr.add(1) == 0x8B && *addr.add(2) == 0xD1 && *addr.add(3) == 0xB8 {
        Some(*(addr.add(4) as *const u32))
    } else {
        None
    }
}

pub fn is_active() -> bool {
    unsafe { SSN_NT_READ != 0 }
}

pub unsafe fn nt_read(handle: *mut c_void, base: *const c_void, buffer: *mut c_void, size: usize) -> i32 {
    syscall_stub(handle, base, buffer, size, std::ptr::null_mut(), SSN_NT_READ)
}

extern "C" {
    fn syscall_stub(h: *mut c_void, b: *const c_void, buf: *mut c_void, s: usize, w: *mut c_void, ssn: u32) -> i32;
}

#[cfg(target_arch = "x86_64")]
global_asm!(
    ".section .text",
    ".global syscall_stub",
    "syscall_stub:",
    "mov r10, rcx",
    "mov eax, [rsp + 48]",
    "syscall",
    "ret"
);

