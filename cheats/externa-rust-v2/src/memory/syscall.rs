//! Direct syscall implementation for NtReadVirtualMemory
//! Uses Halo's Gate technique to find SSN even if hooked

use std::ffi::c_void;
use std::sync::atomic::{AtomicU32, Ordering};
use std::arch::global_asm;
use windows::Win32::System::LibraryLoader::{GetModuleHandleA, GetProcAddress};
use windows::core::PCSTR;

/// Syscall Service Number for NtReadVirtualMemory
static SSN_NT_READ: AtomicU32 = AtomicU32::new(0);

/// Initialize syscall by finding SSN
pub fn init() {
    unsafe {
        let Ok(ntdll) = GetModuleHandleA(PCSTR(b"ntdll.dll\0".as_ptr())) else {
            log::error!("[Syscall] Failed to get ntdll.dll handle");
            return;
        };
        
        let Some(func) = GetProcAddress(ntdll, PCSTR(b"NtReadVirtualMemory\0".as_ptr())) else {
            log::error!("[Syscall] Failed to find NtReadVirtualMemory");
            return;
        };
        
        let addr = func as *const u8;
        
        // Method 1: Direct extraction (function not hooked)
        // Pattern: 4C 8B D1 B8 XX XX XX XX (mov r10, rcx; mov eax, SSN)
        if check_syscall_pattern(addr) {
            let ssn = *(addr.add(4) as *const u32);
            SSN_NT_READ.store(ssn, Ordering::SeqCst);
            log::info!("[Syscall] Direct SSN: {} (0x{:X})", ssn, ssn);
            return;
        }
        
        // Method 2: Halo's Gate - check neighboring syscalls
        // Syscall stubs are typically 32 bytes apart
        const STUB_SIZE: usize = 32;
        
        for i in 1..=32u32 {
            // Check downward (higher SSN)
            let down_addr = addr.add(i as usize * STUB_SIZE);
            if check_syscall_pattern(down_addr) {
                let neighbor_ssn = *(down_addr.add(4) as *const u32);
                let ssn = neighbor_ssn.saturating_sub(i);
                SSN_NT_READ.store(ssn, Ordering::SeqCst);
                log::info!("[Syscall] Halo's Gate (down): SSN {} from neighbor {}", ssn, neighbor_ssn);
                return;
            }
            
            // Check upward (lower SSN)
            if i as usize * STUB_SIZE <= addr as usize {
                let up_addr = addr.sub(i as usize * STUB_SIZE);
                if check_syscall_pattern(up_addr) {
                    let neighbor_ssn = *(up_addr.add(4) as *const u32);
                    let ssn = neighbor_ssn.saturating_add(i);
                    SSN_NT_READ.store(ssn, Ordering::SeqCst);
                    log::info!("[Syscall] Halo's Gate (up): SSN {} from neighbor {}", ssn, neighbor_ssn);
                    return;
                }
            }
        }
        
        log::warn!("[Syscall] Failed to find SSN, falling back to WinAPI");
    }
}

/// Check if address contains syscall stub pattern
#[inline]
unsafe fn check_syscall_pattern(addr: *const u8) -> bool {
    // Pattern: 4C 8B D1 B8 (mov r10, rcx; mov eax, imm32)
    *addr == 0x4C 
        && *addr.add(1) == 0x8B 
        && *addr.add(2) == 0xD1 
        && *addr.add(3) == 0xB8
}

/// Check if syscall is available
#[inline]
pub fn is_active() -> bool {
    SSN_NT_READ.load(Ordering::Relaxed) != 0
}

/// Get current SSN (for debugging)
#[inline]
#[allow(dead_code)]
pub fn get_ssn() -> u32 {
    SSN_NT_READ.load(Ordering::Relaxed)
}

/// Direct syscall to NtReadVirtualMemory
/// 
/// # Safety
/// Caller must ensure valid handle and memory regions
#[inline]
pub unsafe fn nt_read(
    handle: *mut c_void, 
    base: *const c_void, 
    buffer: *mut c_void, 
    size: usize
) -> i32 {
    let ssn = SSN_NT_READ.load(Ordering::Relaxed);
    if ssn == 0 {
        return -1; // STATUS_UNSUCCESSFUL
    }
    syscall_stub(handle, base, buffer, size, std::ptr::null_mut(), ssn)
}

extern "C" {
    fn syscall_stub(
        handle: *mut c_void, 
        base: *const c_void, 
        buffer: *mut c_void, 
        size: usize, 
        bytes_read: *mut usize,
        ssn: u32
    ) -> i32;
}

// x86_64 syscall stub
// Arguments: RCX, RDX, R8, R9, [RSP+0x28], [RSP+0x30]
// NtReadVirtualMemory(Handle, BaseAddress, Buffer, Size, BytesRead)
#[cfg(target_arch = "x86_64")]
global_asm!(
    ".section .text",
    ".global syscall_stub",
    "syscall_stub:",
    "mov r10, rcx",           // Handle -> r10 (syscall convention)
    "mov eax, dword ptr [rsp + 48]",  // SSN from stack (6th arg)
    "syscall",
    "ret"
);
