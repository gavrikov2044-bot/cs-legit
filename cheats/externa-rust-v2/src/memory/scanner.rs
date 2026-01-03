//! Pattern Scanner for CS2 offset discovery
//! Scans client.dll memory for signature patterns and resolves RIP-relative addresses

use anyhow::{anyhow, Result};
use std::ffi::c_void;
use windows::Win32::Foundation::CloseHandle;
use windows::Win32::System::Threading::{OpenProcess, PROCESS_VM_READ, PROCESS_QUERY_INFORMATION};
use windows::Win32::System::Diagnostics::ToolHelp::{
    CreateToolhelp32Snapshot, Module32FirstW, Module32NextW,
    TH32CS_SNAPMODULE, TH32CS_SNAPMODULE32, MODULEENTRY32W,
};
use windows::Win32::System::Diagnostics::Debug::ReadProcessMemory;

/// Pattern definition for signature scanning
#[derive(Clone)]
pub struct Pattern {
    /// Raw pattern bytes (None = wildcard)
    bytes: Vec<Option<u8>>,
    /// Pattern name for logging
    pub name: &'static str,
    /// Offset to RIP-relative value within the pattern
    rip_offset: usize,
    /// Instruction length (added to RIP calculation)
    instruction_len: usize,
}

impl Pattern {
    /// Create pattern from string like "48 8B 05 ? ? ? ? 48 85 C0"
    /// `?` or `??` = wildcard
    pub fn new(name: &'static str, pattern: &str, rip_offset: usize, instruction_len: usize) -> Self {
        let bytes = pattern
            .split_whitespace()
            .map(|s| {
                if s == "?" || s == "??" {
                    None
                } else {
                    u8::from_str_radix(s, 16).ok()
                }
            })
            .collect();
        
        Self { bytes, name, rip_offset, instruction_len }
    }
}

/// CS2 offset patterns (updated for current version)
pub fn get_cs2_patterns() -> Vec<Pattern> {
    vec![
        // dwEntityList - "48 8B 0D ? ? ? ? 48 89 7C 24 ? 8B FA C1 EB"
        Pattern::new(
            "dwEntityList",
            "48 8B 0D ? ? ? ? 48 89 7C 24 ? 8B FA C1 EB",
            3,
            7
        ),
        
        // dwLocalPlayerController - "48 8B 05 ? ? ? ? 48 85 C0 74 ? 8B 88"
        Pattern::new(
            "dwLocalPlayerController",
            "48 8B 05 ? ? ? ? 48 85 C0 74 ? 8B 88",
            3,
            7
        ),
        
        // dwViewMatrix - "48 8D 0D ? ? ? ? 48 C1 E0 06"
        Pattern::new(
            "dwViewMatrix",
            "48 8D 0D ? ? ? ? 48 C1 E0 06",
            3,
            7
        ),
    ]
}

/// Alternative patterns if primary ones fail
pub fn get_cs2_patterns_alt() -> Vec<Pattern> {
    vec![
        // dwEntityList alternative
        Pattern::new(
            "dwEntityList",
            "48 8B 0D ? ? ? ? E8 ? ? ? ? 48 8B F8",
            3,
            7
        ),
        
        // dwLocalPlayerController alternative
        Pattern::new(
            "dwLocalPlayerController", 
            "48 89 05 ? ? ? ? 8B 9E",
            3,
            7
        ),
        
        // dwViewMatrix alternative
        Pattern::new(
            "dwViewMatrix",
            "48 8D 05 ? ? ? ? 4C 8D 05 ? ? ? ? 48 8D 0D",
            3,
            7
        ),
    ]
}

/// Scan result with offset and validation info
#[derive(Debug, Clone)]
pub struct ScanResult {
    pub name: String,
    pub offset: usize,
    pub pattern_offset: usize, // Where pattern was found in module
}

/// Module info for scanning
pub struct ModuleInfo {
    pub base: usize,
    pub size: usize,
    pub data: Vec<u8>,
}

impl ModuleInfo {
    /// Read module memory from process
    pub fn read(pid: u32, module_name: &str) -> Result<Self> {
        unsafe {
            let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)?;
            let mut entry = MODULEENTRY32W { 
                dwSize: std::mem::size_of::<MODULEENTRY32W>() as u32, 
                ..Default::default() 
            };
            
            let mut base: usize = 0;
            let mut size: usize = 0;
            
            if Module32FirstW(snapshot, &mut entry).is_ok() {
                loop {
                    let name = String::from_utf16_lossy(
                        &entry.szModule[..entry.szModule.iter().position(|&c| c == 0).unwrap_or(0)]
                    );
                    if name.eq_ignore_ascii_case(module_name) {
                        base = entry.modBaseAddr as usize;
                        size = entry.modBaseSize as usize;
                        break;
                    }
                    if Module32NextW(snapshot, &mut entry).is_err() { break; }
                }
            }
            let _ = CloseHandle(snapshot);
            
            if base == 0 {
                return Err(anyhow!("Module '{}' not found", module_name));
            }
            
            log::info!("[Scanner] {} base: 0x{:X}, size: 0x{:X} ({:.1} MB)", 
                      module_name, base, size, size as f64 / 1024.0 / 1024.0);
            
            let handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, false, pid)?;
            let mut data = vec![0u8; size];
            let mut bytes_read = 0;
            
            let result = ReadProcessMemory(
                handle, 
                base as *const c_void, 
                data.as_mut_ptr() as *mut c_void, 
                size, 
                Some(&mut bytes_read)
            );
            
            let _ = CloseHandle(handle);
            
            if result.is_err() {
                return Err(anyhow!("Failed to read module memory"));
            }
            
            log::info!("[Scanner] Read {} bytes from {}", bytes_read, module_name);
            
            Ok(Self { base, size, data })
        }
    }
}

/// Fast pattern matching using SIMD-friendly approach
fn find_pattern(data: &[u8], pattern: &[Option<u8>]) -> Option<usize> {
    if pattern.is_empty() || data.len() < pattern.len() {
        return None;
    }
    
    // Find first non-wildcard byte for quick skip
    let first_byte_idx = pattern.iter().position(|b| b.is_some())?;
    let first_byte = pattern[first_byte_idx].unwrap();
    
    let search_len = data.len() - pattern.len();
    
    for i in 0..=search_len {
        // Quick first-byte check
        if data[i + first_byte_idx] != first_byte {
            continue;
        }
        
        // Full pattern check
        let matches = pattern.iter().enumerate().all(|(j, &b)| {
            b.is_none() || b == Some(data[i + j])
        });
        
        if matches {
            return Some(i);
        }
    }
    
    None
}

/// Find all occurrences of pattern in data
fn find_all_patterns(data: &[u8], pattern: &[Option<u8>]) -> Vec<usize> {
    let mut results = Vec::new();
    let mut offset = 0;
    
    while offset < data.len() {
        if let Some(found) = find_pattern(&data[offset..], pattern) {
            results.push(offset + found);
            offset += found + 1;
        } else {
            break;
        }
    }
    
    results
}

/// Resolve RIP-relative address
fn resolve_rip(module: &ModuleInfo, pattern_offset: usize, rip_offset: usize, instruction_len: usize) -> Option<usize> {
    let rip_idx = pattern_offset + rip_offset;
    
    if rip_idx + 4 > module.data.len() {
        return None;
    }
    
    // Read RIP-relative offset (signed 32-bit)
    let rip_value = i32::from_le_bytes(
        module.data[rip_idx..rip_idx + 4].try_into().ok()?
    );
    
    // Calculate absolute address
    let absolute = module.base as isize 
        + pattern_offset as isize 
        + rip_value as isize 
        + instruction_len as isize;
    
    // Return offset from module base
    let offset = (absolute as usize).checked_sub(module.base)?;
    
    // Sanity check - offset should be within module
    if offset > module.size {
        return None;
    }
    
    Some(offset)
}

/// Scan for all patterns and return offsets
pub fn scan_offsets(pid: u32, module_name: &str) -> Result<crate::game::offsets::Offsets> {
    log::info!("[Scanner] Starting pattern scan for {}...", module_name);
    
    let module = ModuleInfo::read(pid, module_name)?;
    let patterns = get_cs2_patterns();
    
    let mut entity_list = None;
    let mut local_player = None;
    let mut view_matrix = None;
    
    for pattern in &patterns {
        let occurrences = find_all_patterns(&module.data, &pattern.bytes);
        
        log::info!("[Scanner] {} found {} occurrence(s)", pattern.name, occurrences.len());
        
        if occurrences.is_empty() {
            log::warn!("[Scanner] Pattern {} NOT FOUND, trying alternative...", pattern.name);
            continue;
        }
        
        // Use first occurrence
        let pattern_offset = occurrences[0];
        
        if let Some(offset) = resolve_rip(&module, pattern_offset, pattern.rip_offset, pattern.instruction_len) {
            log::info!("[Scanner] {} -> 0x{:X} (pattern at 0x{:X})", 
                      pattern.name, offset, pattern_offset);
            
            match pattern.name {
                "dwEntityList" => entity_list = Some(offset),
                "dwLocalPlayerController" => local_player = Some(offset),
                "dwViewMatrix" => view_matrix = Some(offset),
                _ => {}
            }
        } else {
            log::error!("[Scanner] Failed to resolve RIP for {}", pattern.name);
        }
    }
    
    // Try alternative patterns for missing offsets
    if entity_list.is_none() || local_player.is_none() || view_matrix.is_none() {
        log::info!("[Scanner] Trying alternative patterns...");
        
        for pattern in &get_cs2_patterns_alt() {
            let skip = match pattern.name {
                "dwEntityList" if entity_list.is_some() => true,
                "dwLocalPlayerController" if local_player.is_some() => true,
                "dwViewMatrix" if view_matrix.is_some() => true,
                _ => false,
            };
            
            if skip { continue; }
            
            if let Some(pattern_offset) = find_pattern(&module.data, &pattern.bytes) {
                if let Some(offset) = resolve_rip(&module, pattern_offset, pattern.rip_offset, pattern.instruction_len) {
                    log::info!("[Scanner] ALT {} -> 0x{:X}", pattern.name, offset);
                    
                    match pattern.name {
                        "dwEntityList" => entity_list = Some(offset),
                        "dwLocalPlayerController" => local_player = Some(offset),
                        "dwViewMatrix" => view_matrix = Some(offset),
                        _ => {}
                    }
                }
            }
        }
    }
    
    // Validate we found all offsets
    let entity_list = entity_list.ok_or_else(|| anyhow!("dwEntityList not found"))?;
    let local_player = local_player.ok_or_else(|| anyhow!("dwLocalPlayerController not found"))?;
    let view_matrix = view_matrix.ok_or_else(|| anyhow!("dwViewMatrix not found"))?;
    
    log::info!("[Scanner] ===== SCAN COMPLETE =====");
    log::info!("[Scanner] dwEntityList: 0x{:X}", entity_list);
    log::info!("[Scanner] dwLocalPlayerController: 0x{:X}", local_player);
    log::info!("[Scanner] dwViewMatrix: 0x{:X}", view_matrix);
    
    Ok(crate::game::offsets::Offsets {
        dw_entity_list: entity_list,
        dw_local_player_controller: local_player,
        dw_view_matrix: view_matrix,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_pattern_parsing() {
        let p = Pattern::new("test", "48 8B 05 ? ? ? ?", 3, 7);
        assert_eq!(p.bytes.len(), 7);
        assert_eq!(p.bytes[0], Some(0x48));
        assert_eq!(p.bytes[3], None); // wildcard
    }
    
    #[test]
    fn test_find_pattern() {
        let data = [0x00, 0x48, 0x8B, 0x05, 0x12, 0x34, 0x56, 0x78, 0x00];
        let pattern = vec![Some(0x48), Some(0x8B), Some(0x05), None, None, None, None];
        
        assert_eq!(find_pattern(&data, &pattern), Some(1));
    }
}
