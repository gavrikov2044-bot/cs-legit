//! Memory reading module
//! Uses WinAPI ReadProcessMemory for maximum compatibility

use std::mem;
use std::ffi::c_void;
use std::sync::Arc;

use anyhow::{anyhow, Result};
use glam::Vec3;
use windows::Win32::Foundation::{HANDLE, CloseHandle};
use windows::Win32::System::Threading::{
    OpenProcess, PROCESS_VM_READ, PROCESS_QUERY_INFORMATION,
};
use windows::Win32::System::Diagnostics::ToolHelp::{
    CreateToolhelp32Snapshot, Process32FirstW, Process32NextW,
    Module32FirstW, Module32NextW,
    TH32CS_SNAPPROCESS, TH32CS_SNAPMODULE, TH32CS_SNAPMODULE32,
    PROCESSENTRY32W, MODULEENTRY32W,
};
use windows::Win32::System::Diagnostics::Debug::ReadProcessMemory;

use crate::offsets;
use crate::esp::EspData;

/// Wrapper for HANDLE to make it Send
struct SendableHandle(HANDLE);
unsafe impl Send for SendableHandle {}
unsafe impl Sync for SendableHandle {}

/// Game memory handle
#[derive(Clone)]
pub struct GameMemory {
    handle: Arc<SendableHandle>,
    client_base: usize,
}

impl GameMemory {
    /// Attach to CS2 process
    pub fn attach() -> Result<Self> {
        let pid = Self::find_process("cs2.exe")?;
        
        let handle = unsafe {
            OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, false, pid)?
        };
        
        let client_base = Self::find_module(pid, "client.dll")?;
        
        Ok(Self { 
            handle: Arc::new(SendableHandle(handle)), 
            client_base 
        })
    }
    
    /// Get client.dll base address
    pub fn get_client_base(&self) -> usize {
        self.client_base
    }
    
    /// Find process by name
    fn find_process(name: &str) -> Result<u32> {
        unsafe {
            let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)?;
            
            let mut entry = PROCESSENTRY32W {
                dwSize: mem::size_of::<PROCESSENTRY32W>() as u32,
                ..Default::default()
            };
            
            if Process32FirstW(snapshot, &mut entry).is_ok() {
                loop {
                    let proc_name = String::from_utf16_lossy(
                        &entry.szExeFile[..entry.szExeFile.iter().position(|&c| c == 0).unwrap_or(0)]
                    );
                    
                    if proc_name.to_lowercase() == name.to_lowercase() {
                        let _ = CloseHandle(snapshot);
                        return Ok(entry.th32ProcessID);
                    }
                    
                    if Process32NextW(snapshot, &mut entry).is_err() {
                        break;
                    }
                }
            }
            
            let _ = CloseHandle(snapshot);
            Err(anyhow!("Process not found"))
        }
    }
    
    /// Find module base address
    fn find_module(pid: u32, name: &str) -> Result<usize> {
        unsafe {
            let snapshot = CreateToolhelp32Snapshot(
                TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, 
                pid
            )?;
            
            let mut entry = MODULEENTRY32W {
                dwSize: mem::size_of::<MODULEENTRY32W>() as u32,
                ..Default::default()
            };
            
            if Module32FirstW(snapshot, &mut entry).is_ok() {
                loop {
                    let mod_name = String::from_utf16_lossy(
                        &entry.szModule[..entry.szModule.iter().position(|&c| c == 0).unwrap_or(0)]
                    );
                    
                    if mod_name.to_lowercase() == name.to_lowercase() {
                        let _ = CloseHandle(snapshot);
                        return Ok(entry.modBaseAddr as usize);
                    }
                    
                    if Module32NextW(snapshot, &mut entry).is_err() {
                        break;
                    }
                }
            }
            
            let _ = CloseHandle(snapshot);
            Err(anyhow!("Module not found"))
        }
    }
    
    /// Read memory
    pub fn read<T: Copy + Default>(&self, addr: usize) -> T {
        let mut buffer = T::default();
        unsafe {
            let _ = ReadProcessMemory(
                self.handle.0,
                addr as *const c_void,
                &mut buffer as *mut T as *mut c_void,
                mem::size_of::<T>(),
                None,
            );
        }
        buffer
    }
    
    /// Read all entities and return ESP data
    pub fn read_entities(&self) -> Result<EspData> {
        let mut data = EspData::default();
        
        // Read view matrix
        data.view_matrix = self.read(self.client_base + offsets::client::DW_VIEW_MATRIX);
        
        // Read local player
        let local_controller: usize = self.read(
            self.client_base + offsets::client::DW_LOCAL_PLAYER_CONTROLLER
        );
        
        // Debug log (only once per second)
        static mut LOG_COUNTER: u32 = 0;
        unsafe {
            LOG_COUNTER += 1;
            if LOG_COUNTER % 500 == 1 {
                log::debug!("client_base: 0x{:X}, local_controller: 0x{:X}", 
                    self.client_base, local_controller);
            }
        }
        
        if local_controller == 0 {
            return Ok(data);
        }
        
        let local_pawn_handle: u32 = self.read(
            local_controller + offsets::controller::M_H_PLAYER_PAWN
        );
        
        let entity_list: usize = self.read(
            self.client_base + offsets::client::DW_ENTITY_LIST
        );
        
        if entity_list == 0 {
            return Ok(data);
        }
        
        // Get local pawn
        let list_entry: usize = self.read(
            entity_list + 0x8 * ((local_pawn_handle & 0x7FFF) >> 9) as usize + 16
        );
        let local_pawn: usize = self.read(
            list_entry + 120 * (local_pawn_handle & 0x1FF) as usize
        );
        let local_team: i32 = self.read(local_pawn + offsets::entity::M_I_TEAM_NUM);
        
        // Read all players
        for i in 1..=64 {
            let list_entry: usize = self.read(
                entity_list + 8 * ((i & 0x7FFF) >> 9) + 16
            );
            if list_entry == 0 { continue; }
            
            let controller: usize = self.read(list_entry + 120 * (i & 0x1FF));
            if controller == 0 { continue; }
            
            let pawn_handle: u32 = self.read(
                controller + offsets::controller::M_H_PLAYER_PAWN
            );
            if pawn_handle == 0 { continue; }
            
            let list_entry2: usize = self.read(
                entity_list + 0x8 * ((pawn_handle & 0x7FFF) >> 9) as usize + 16
            );
            let pawn: usize = self.read(
                list_entry2 + 120 * (pawn_handle & 0x1FF) as usize
            );
            
            if pawn == 0 || pawn == local_pawn { continue; }
            
            let health: i32 = self.read(pawn + offsets::entity::M_I_HEALTH);
            if health <= 0 || health > 100 { continue; }
            
            let team: i32 = self.read(pawn + offsets::entity::M_I_TEAM_NUM);
            if team == local_team { continue; }
            
            let scene_node: usize = self.read(pawn + offsets::entity::M_P_GAME_SCENE_NODE);
            let origin: [f32; 3] = self.read(scene_node + offsets::entity::M_VEC_ABS_ORIGIN);
            let armor: i32 = self.read(pawn + offsets::pawn::M_ARMOR_VALUE);
            
            data.entities.push(crate::esp::EntityData {
                origin: Vec3::from_array(origin),
                health,
                armor,
            });
        }
        
        Ok(data)
    }
}

impl Drop for GameMemory {
    fn drop(&mut self) {
        // Only close if we're the last reference
        if Arc::strong_count(&self.handle) == 1 {
            unsafe {
                let _ = CloseHandle(self.handle.0);
            }
        }
    }
}
