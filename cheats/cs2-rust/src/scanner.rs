//! Pattern Scanner for automatic offset detection
//! Based on Valthrun signature scanning

use std::ffi::c_void;
use std::mem;

use anyhow::{anyhow, Result};
use windows::Win32::Foundation::CloseHandle;
use windows::Win32::System::Threading::{OpenProcess, PROCESS_VM_READ, PROCESS_QUERY_INFORMATION};
use windows::Win32::System::Diagnostics::ToolHelp::{
    CreateToolhelp32Snapshot, Module32FirstW, Module32NextW,
    TH32CS_SNAPMODULE, TH32CS_SNAPMODULE32, MODULEENTRY32W,
};
use windows::Win32::System::Diagnostics::Debug::ReadProcessMemory;

/// Pattern with wildcards (? = any byte)
pub struct Pattern {
    bytes: Vec<Option<u8>>,
    name: &'static str,
}

impl Pattern {
    /// Parse pattern string like "48 8B 05 ? ? ? ? 8B 48"
    pub fn new(name: &'static str, pattern: &str) -> Self {
        let bytes = pattern
            .split_whitespace()
            .map(|s| {
                if s == "?" || s == "??" {
                    None
                } else {
                    Some(u8::from_str_radix(s, 16).unwrap_or(0))
                }
            })
            .collect();
        
        Self { bytes, name }
    }
    
    /// Search for pattern in memory
    pub fn find(&self, data: &[u8]) -> Option<usize> {
        'outer: for i in 0..data.len().saturating_sub(self.bytes.len()) {
            for (j, &pat) in self.bytes.iter().enumerate() {
                if let Some(b) = pat {
                    if data[i + j] != b {
                        continue 'outer;
                    }
                }
            }
            return Some(i);
        }
        None
    }
    
    /// Resolve relative address (RIP-relative)
    pub fn resolve_rip(&self, data: &[u8], offset: usize, instr_len: usize) -> Option<usize> {
        if offset + 4 > data.len() {
            return None;
        }
        
        let rip_offset = i32::from_le_bytes([
            data[offset],
            data[offset + 1],
            data[offset + 2],
            data[offset + 3],
        ]);
        
        Some((offset as i64 + rip_offset as i64 + instr_len as i64) as usize)
    }
}

/// CS2 Signature definitions (from Valthrun)
pub struct Signatures;

impl Signatures {
    /// Entity List: "4C 8B 0D ? ? ? ? 8B 97"
    pub fn entity_list() -> Pattern {
        Pattern::new("EntityList", "4C 8B 0D ? ? ? ? 8B 97")
    }
    
    /// Local Player Controller: "48 83 3D ? ? ? ? ? 0F 95"
    pub fn local_controller() -> Pattern {
        Pattern::new("LocalController", "48 83 3D ? ? ? ? ? 0F 95")
    }
    
    /// View Matrix: "48 8D 0D ? ? ? ? 48 C1 E0 06"
    pub fn view_matrix() -> Pattern {
        Pattern::new("ViewMatrix", "48 8D 0D ? ? ? ? 48 C1 E0 06")
    }
}

/// Scan module for patterns
pub fn scan_module(pid: u32, module_name: &str, pattern: &Pattern) -> Result<usize> {
    unsafe {
        let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)?;
        
        let mut entry = MODULEENTRY32W {
            dwSize: mem::size_of::<MODULEENTRY32W>() as u32,
            ..Default::default()
        };
        
        let mut base: usize = 0;
        let mut size: usize = 0;
        
        if Module32FirstW(snapshot, &mut entry).is_ok() {
            loop {
                let name = String::from_utf16_lossy(
                    &entry.szModule[..entry.szModule.iter().position(|&c| c == 0).unwrap_or(0)]
                );
                
                if name.to_lowercase() == module_name.to_lowercase() {
                    base = entry.modBaseAddr as usize;
                    size = entry.modBaseSize as usize;
                    break;
                }
                
                if Module32NextW(snapshot, &mut entry).is_err() {
                    break;
                }
            }
        }
        
        let _ = CloseHandle(snapshot);
        
        if base == 0 {
            return Err(anyhow!("Module {} not found", module_name));
        }
        
        // Read module memory
        let handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, false, pid)?;
        
        let mut buffer = vec![0u8; size];
        let mut bytes_read = 0usize;
        
        let result = ReadProcessMemory(
            handle,
            base as *const c_void,
            buffer.as_mut_ptr() as *mut c_void,
            size,
            Some(&mut bytes_read),
        );
        
        let _ = CloseHandle(handle);
        
        if result.is_err() || bytes_read == 0 {
            return Err(anyhow!("Failed to read module memory"));
        }
        
        // Find pattern
        match pattern.find(&buffer) {
            Some(offset) => {
                log::info!("[Scanner] Found {} at offset 0x{:X}", pattern.name, offset);
                
                // Resolve RIP-relative address
                if let Some(resolved) = pattern.resolve_rip(&buffer, offset + 3, 7) {
                    log::info!("[Scanner] Resolved {} to 0x{:X}", pattern.name, resolved);
                    Ok(resolved)
                } else {
                    Ok(offset)
                }
            }
            None => Err(anyhow!("Pattern {} not found", pattern.name)),
        }
    }
}

