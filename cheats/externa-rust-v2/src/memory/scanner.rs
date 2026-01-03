use anyhow::{anyhow, Result};
use std::mem;
use std::ffi::c_void;
use windows::Win32::Foundation::CloseHandle;
use windows::Win32::System::Threading::{OpenProcess, PROCESS_VM_READ, PROCESS_QUERY_INFORMATION};
use windows::Win32::System::Diagnostics::ToolHelp::{
    CreateToolhelp32Snapshot, Module32FirstW, Module32NextW,
    TH32CS_SNAPMODULE, TH32CS_SNAPMODULE32, MODULEENTRY32W,
};
use windows::Win32::System::Diagnostics::Debug::ReadProcessMemory;

// Pattern struct for signature scanning (not used with hardcoded offsets)
#[allow(dead_code)]
pub struct Pattern {
    bytes: Vec<Option<u8>>,
    name: &'static str,
    offset: usize, // Offset to read (e.g., 3 for "48 8B 05 ? ? ? ?")
    extra: usize,  // Extra to add (instruction length, usually 7)
}

#[allow(dead_code)]
impl Pattern {
    pub fn new(name: &'static str, pattern: &str, offset: usize, extra: usize) -> Self {
        let bytes = pattern.split_whitespace()
            .map(|s| if s == "?" { None } else { Some(u8::from_str_radix(s, 16).unwrap_or(0)) })
            .collect();
        Self { bytes, name, offset, extra }
    }
}

// Scan module for patterns (disabled - using hardcoded offsets)
#[allow(dead_code)]
pub fn scan_module(pid: u32, module_name: &str, patterns: &[Pattern]) -> Result<Vec<usize>> {
    let mut results = vec![0; patterns.len()];
    
    unsafe {
        let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)?;
        let mut entry = MODULEENTRY32W { dwSize: mem::size_of::<MODULEENTRY32W>() as u32, ..Default::default() };
        
        let mut base: usize = 0;
        let mut size: usize = 0;
        
        if Module32FirstW(snapshot, &mut entry).is_ok() {
            loop {
                let name = String::from_utf16_lossy(&entry.szModule[..entry.szModule.iter().position(|&c| c == 0).unwrap_or(0)]);
                if name.eq_ignore_ascii_case(module_name) {
                    base = entry.modBaseAddr as usize;
                    size = entry.modBaseSize as usize;
                    break;
                }
                if Module32NextW(snapshot, &mut entry).is_err() { break; }
            }
        }
        let _ = CloseHandle(snapshot);
        
        if base == 0 { return Err(anyhow!("Module {} not found", module_name)); }
        
        let handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, false, pid)?;
        let mut buffer = vec![0u8; size];
        let mut bytes_read = 0;
        
        if ReadProcessMemory(handle, base as *const c_void, buffer.as_mut_ptr() as *mut c_void, size, Some(&mut bytes_read)).is_err() {
            let _ = CloseHandle(handle);
            return Err(anyhow!("Failed to read module memory"));
        }
        let _ = CloseHandle(handle);

        // Scan
        for (idx, pat) in patterns.iter().enumerate() {
            if let Some(found_offset) = find_pattern(&buffer, &pat.bytes) {
                // Resolve RIP
                let rip_offset_idx = found_offset + pat.offset;
                if rip_offset_idx + 4 <= buffer.len() {
                    let rip_val = i32::from_le_bytes(buffer[rip_offset_idx..rip_offset_idx+4].try_into().unwrap());
                    let resolved = (base as isize + found_offset as isize + rip_val as isize + pat.extra as isize) as usize;
                    results[idx] = resolved - base;
                    log::info!("[Scanner] Found {} -> Offset: 0x{:X}", pat.name, results[idx]);
                }
            } else {
                log::error!("[Scanner] Pattern {} NOT found!", pat.name);
            }
        }
    }
    Ok(results)
}

#[allow(dead_code)]
fn find_pattern(data: &[u8], pattern: &[Option<u8>]) -> Option<usize> {
    for i in 0..data.len().saturating_sub(pattern.len()) {
        if pattern.iter().enumerate().all(|(j, &b)| b.is_none() || b == Some(data[i + j])) {
            return Some(i);
        }
    }
    None
}

