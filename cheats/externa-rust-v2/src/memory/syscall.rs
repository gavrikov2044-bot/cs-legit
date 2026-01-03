//! Ultra-optimized Direct syscall implementation for NtReadVirtualMemory
//! Uses Halo's Gate technique to find SSN even if hooked
//! 
//! OPTIMIZATIONS:
//! - SSN cached in regular static (no atomic load per call)
//! - Inline assembly to avoid function call overhead
//! - No runtime checks after init

use std::ffi::c_void;
use std::sync::atomic::{AtomicU32, Ordering};
use std::arch::global_asm;
use windows::Win32::System::LibraryLoader::{GetModuleHandleA, GetProcAddress};
use windows::core::PCSTR;

/// Syscall Service Number for NtReadVirtualMemory (atomic for init)
static SSN_NT_READ: AtomicU32 = AtomicU32::new(0);

/// Cached SSN for ultra-fast access (no atomic overhead)
/// Set once during init(), never changes after
static mut CACHED_SSN: u32 = 0;
static mut SYSCALL_READY: bool = false;

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
        let mut found_ssn: Option<u32> = None;
        
        // Method 1: Direct extraction (function not hooked)
        // Pattern: 4C 8B D1 B8 XX XX XX XX (mov r10, rcx; mov eax, SSN)
        if check_syscall_pattern(addr) {
            let ssn = *(addr.add(4) as *const u32);
            found_ssn = Some(ssn);
            log::info!("[Syscall] Direct SSN: {} (0x{:X})", ssn, ssn);
        }
        
        // Method 2: Halo's Gate - check neighboring syscalls
        if found_ssn.is_none() {
            const STUB_SIZE: usize = 32;
            
            'search: for i in 1..=32u32 {
                // Check downward (higher SSN)
                let down_addr = addr.add(i as usize * STUB_SIZE);
                if check_syscall_pattern(down_addr) {
                    let neighbor_ssn = *(down_addr.add(4) as *const u32);
                    let ssn = neighbor_ssn.saturating_sub(i);
                    found_ssn = Some(ssn);
                    log::info!("[Syscall] Halo's Gate (down): SSN {} from neighbor {}", ssn, neighbor_ssn);
                    break 'search;
                }
                
                // Check upward (lower SSN)
                if i as usize * STUB_SIZE <= addr as usize {
                    let up_addr = addr.sub(i as usize * STUB_SIZE);
                    if check_syscall_pattern(up_addr) {
                        let neighbor_ssn = *(up_addr.add(4) as *const u32);
                        let ssn = neighbor_ssn.saturating_add(i);
                        found_ssn = Some(ssn);
                        log::info!("[Syscall] Halo's Gate (up): SSN {} from neighbor {}", ssn, neighbor_ssn);
                        break 'search;
                    }
                }
            }
        }
        
        // Store SSN in both atomic (for is_active check) and cached (for fast path)
        if let Some(ssn) = found_ssn {
            SSN_NT_READ.store(ssn, Ordering::SeqCst);
            CACHED_SSN = ssn;
            SYSCALL_READY = true;
        } else {
            log::warn!("[Syscall] Failed to find SSN, falling back to WinAPI");
        }
    }
}

/// Check if address contains syscall stub pattern
#[inline(always)]
unsafe fn check_syscall_pattern(addr: *const u8) -> bool {
    // Read as u32 for single memory access instead of 4 separate reads
    let pattern = *(addr as *const u32);
    // Pattern: 4C 8B D1 B8 (little endian: 0xB8D18B4C)
    pattern == 0xB8D18B4C
}

/// Check if syscall is available (called once at startup)
#[inline(always)]
pub fn is_active() -> bool {
    unsafe { SYSCALL_READY }
}

/// Get current SSN (for debugging)
#[inline]
#[allow(dead_code)]
pub fn get_ssn() -> u32 {
    SSN_NT_READ.load(Ordering::Relaxed)
}

/// Ultra-fast direct syscall to NtReadVirtualMemory
/// 
/// # Safety
/// - Caller must ensure valid handle and memory regions
/// - Must call init() before using
/// 
/// # Performance
/// - No atomic loads
/// - No runtime checks (assumes init() was called)
/// - Direct register setup + syscall instruction
#[inline(always)]
pub unsafe fn nt_read(
    handle: *mut c_void, 
    base: *const c_void, 
    buffer: *mut c_void, 
    size: usize
) -> i32 {
    // Fast path: use cached SSN (no atomic load!)
    syscall_nt_read_fast(handle, base, buffer, size, CACHED_SSN)
}

/// Ultra-fast syscall with SSN as parameter
/// Inline assembly to minimize overhead
#[inline(always)]
unsafe fn syscall_nt_read_fast(
    handle: *mut c_void,
    base: *const c_void,
    buffer: *mut c_void,
    size: usize,
    ssn: u32,
) -> i32 {
    let status: i32;
    
    // NtReadVirtualMemory(Handle, BaseAddress, Buffer, Size, BytesRead)
    // We pass null for BytesRead (5th arg)
    std::arch::asm!(
        "mov r10, rcx",           // Handle -> r10 (syscall convention)
        "xor r9d, r9d",           // BytesRead = NULL (we don't need it)
        "push r9",                // Push NULL BytesRead to stack
        "sub rsp, 32",            // Shadow space
        "syscall",
        "add rsp, 40",            // Cleanup stack (32 shadow + 8 BytesRead)
        in("eax") ssn,
        in("rcx") handle,
        in("rdx") base,
        in("r8") buffer,
        in("r9") size,            // This gets moved, then zeroed above
        lateout("rax") status,
        lateout("rcx") _,
        lateout("rdx") _,
        lateout("r8") _,
        lateout("r9") _,
        lateout("r10") _,
        lateout("r11") _,
        options(nostack)
    );
    
    status
}

// Keep the old stub for compatibility (unused but kept for reference)
#[allow(dead_code)]
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

// Old assembly stub (kept for reference)
#[cfg(target_arch = "x86_64")]
global_asm!(
    ".section .text",
    ".global syscall_stub",
    "syscall_stub:",
    "mov r10, rcx",
    "mov eax, dword ptr [rsp + 48]",
    "syscall",
    "ret"
);
