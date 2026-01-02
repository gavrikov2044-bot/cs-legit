mod game;
mod memory;
mod overlay;

use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;
use anyhow::Result;
use glam::Vec3;
use log::info;
use crate::memory::handle::ProcessReader; // This is now correctly imported from the trait definition

// Global Game State
struct GameState {
    view_matrix: [[f32; 4]; 4],
    entities: Vec<game::entity::Entity>,
    local_team: i32,
}

fn main() -> Result<()> {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info")).init();
    
    info!("Initializing Externa Rust V2...");

    // 1. Initialize Memory
    memory::syscall::init(); // Init syscalls
    
    // 2. Attach to CS2
    let mem = loop {
        if let Ok(pid) = get_pid("cs2.exe") {
            if let Ok(handle) = open_process(pid) {
                if let Ok(base) = get_module_base(pid, "client.dll") {
                    info!("Attached to CS2! PID: {}, Client: 0x{:X}", pid, base);
                    break Arc::new(memory::Memory {
                        handle: Arc::new(memory::handle::SendHandle(handle)),
                        pid,
                        client_base: base,
                    });
                }
            }
        }
        thread::sleep(Duration::from_secs(1));
    };

    // 3. Offsets (From Dump)
    info!("Using hardcoded offsets from dump (2026-01-02)...");
    let offsets = Arc::new(game::offsets::Offsets {
        dw_entity_list: game::offsets::DW_ENTITY_LIST,
        dw_local_player_controller: game::offsets::DW_LOCAL_PLAYER_CONTROLLER,
        dw_view_matrix: game::offsets::DW_VIEW_MATRIX,
    });

    // 4. Shared State
    let state = Arc::new(Mutex::new(GameState {
        view_matrix: [[0.0; 4]; 4],
        entities: Vec::with_capacity(64),
        local_team: 0,
    }));

    // 5. Memory Loop
    let mem_clone = mem.clone();
    let state_clone = state.clone();
    let offsets_clone = offsets.clone();
    
    thread::spawn(move || {
        info!("Memory thread started.");
        let mut last_debug = std::time::Instant::now();
        loop {
            if let Ok(mut st) = state_clone.lock() {
                // Read Matrix
                if let Some(mat) = mem_clone.read::<[[f32; 4]; 4]>(mem_clone.client_base + offsets_clone.dw_view_matrix) {
                    st.view_matrix = mat;
                }

                // Read Local Player
                let mut local_team = 0;
                let local_ctrl: usize = mem_clone.read(mem_clone.client_base + offsets_clone.dw_local_player_controller).unwrap_or(0);
                if local_ctrl != 0 {
                     let pawn_h: u32 = mem_clone.read(local_ctrl + game::offsets::netvars::M_H_PLAYER_PAWN).unwrap_or(0);
                     let ent_list: usize = mem_clone.read(mem_clone.client_base + offsets_clone.dw_entity_list).unwrap_or(0);
                     
                     if ent_list != 0 && pawn_h != 0 {
                         let entry: usize = mem_clone.read(ent_list + 8 * ((pawn_h as usize & 0x7FFF) >> 9) + 16).unwrap_or(0);
                         if entry != 0 {
                             let pawn: usize = mem_clone.read(entry + 120 * (pawn_h as usize & 0x1FF)).unwrap_or(0);
                             if pawn != 0 {
                                 local_team = mem_clone.read(pawn + game::offsets::netvars::M_I_TEAM_NUM).unwrap_or(0);
                             }
                         }
                     }
                }
                st.local_team = local_team;

                // Read Entities
                st.entities.clear();
                let ent_list: usize = mem_clone.read(mem_clone.client_base + offsets_clone.dw_entity_list).unwrap_or(0);
                
                let mut should_log = false;
                if last_debug.elapsed().as_secs() >= 5 {
                    should_log = true;
                    let local_ptr = mem_clone.read(mem_clone.client_base + offsets_clone.dw_local_player_controller).unwrap_or(0);
                    info!("DEBUG: EntityListPtr=0x{:X} LocalCtrlPtr=0x{:X} Entities={}", ent_list, local_ptr, st.entities.len());
                    last_debug = std::time::Instant::now();
                }

                if ent_list != 0 {
                    for i in 1..64 {
                        let list_entry: usize = mem_clone.read(ent_list + 8 * ((i & 0x7FFF) >> 9) + 16).unwrap_or(0);
                        if list_entry == 0 { continue; }
                        
                        let controller: usize = mem_clone.read(list_entry + 120 * (i & 0x1FF)).unwrap_or(0);
                        if controller == 0 { continue; }
                        
                        let pawn_h: u32 = mem_clone.read(controller + game::offsets::netvars::M_H_PLAYER_PAWN).unwrap_or(0);
                        
                        if should_log && i == 1 {
                             info!("DEBUG Loop i=1: ListEntry=0x{:X} Controller=0x{:X} PawnHandle=0x{:X}", list_entry, controller, pawn_h);
                        }

                        if pawn_h == 0 { continue; }
                        
                        let list_entry2: usize = mem_clone.read(ent_list + 8 * ((pawn_h as usize & 0x7FFF) >> 9) + 16).unwrap_or(0);
                        if list_entry2 == 0 { continue; }
                        
                        let pawn: usize = mem_clone.read(list_entry2 + 120 * (pawn_h as usize & 0x1FF)).unwrap_or(0);
                        if pawn == 0 { continue; }

                        // Health check
                        let health: i32 = mem_clone.read(pawn + game::offsets::netvars::M_I_HEALTH).unwrap_or(0);
                        if health <= 0 || health > 100 { continue; }
                        
                        let team: i32 = mem_clone.read(pawn + game::offsets::netvars::M_I_TEAM_NUM).unwrap_or(0);
                        
                        // Pos
                        let node: usize = mem_clone.read(pawn + game::offsets::netvars::M_P_GAME_SCENE_NODE).unwrap_or(0);
                        let pos: Vec3 = mem_clone.read(node + game::offsets::netvars::M_VEC_ABS_ORIGIN).unwrap_or(Vec3::ZERO);
                        
                        // Bones
                        let mut bones = [Vec3::ZERO; 30];
                        let model_state: usize = mem_clone.read(node + game::offsets::netvars::M_MODEL_STATE).unwrap_or(0);
                        if model_state != 0 {
                            let bone_array: usize = mem_clone.read(model_state + game::offsets::netvars::M_BONE_ARRAY).unwrap_or(0);
                            if bone_array != 0 {
                                // Important bones indices
                                let indices = [6, 5, 4, 2, 0, 8, 9, 10, 13, 14, 15, 22, 23, 24, 25, 26, 27];
                                for &idx in &indices {
                                    if idx < 30 {
                                        bones[idx] = mem_clone.read(bone_array + idx * 32).unwrap_or(Vec3::ZERO);
                                    }
                                }
                            }
                        }

                        st.entities.push(game::entity::Entity {
                            pawn, controller, pos, health, team, bones
                        });
                    }
                }
            }
            thread::sleep(Duration::from_millis(2));
        }
    });

    // 6. Overlay Loop
    let overlay = overlay::renderer::Direct2DOverlay::new()?;
    info!("Overlay initialized (Direct2D). Window size: {}x{} | [ESP ONLY - NO MENU]", overlay.width, overlay.height);

    loop {
        if !overlay.handle_message() { break; }
        
        overlay.begin_scene();
        
        if let Ok(st) = state.lock() {
            for ent in &st.entities {
                if ent.team == st.local_team { continue; } // Skip teammates
                
                let is_enemy = true; // For color selection

                // Box
                let head_pos = ent.bones[6]; // Head bone
                let head_pos_visual = Vec3::new(ent.pos.x, ent.pos.y, ent.pos.z + 75.0); // Fallback
                
                let target_head = if head_pos.length() > 0.0 { head_pos + Vec3::new(0.0, 0.0, 8.0) } else { head_pos_visual };

                if let (Some(s_feet), Some(s_head)) = (
                    game::math::w2s(&st.view_matrix, ent.pos, overlay.width as f32, overlay.height as f32),
                    game::math::w2s(&st.view_matrix, target_head, overlay.width as f32, overlay.height as f32)
                ) {
                    let h = s_feet.y - s_head.y;
                    let w = h * 0.4;
                    let x = s_head.x - w / 2.0;
                    let y = s_head.y;
                    
                    overlay.draw_box(x, y, w, h, is_enemy);

                    // Skeleton
                    let bones = &ent.bones;
                    let pairs = [
                        (6, 5), (5, 4), (4, 2), (2, 0), // Spine
                        (5, 8), (8, 9), (9, 10), // R Arm
                        (5, 13), (13, 14), (14, 15), // L Arm
                        (0, 22), (22, 23), (23, 24), // R Leg
                        (0, 25), (25, 26), (26, 27), // L Leg
                    ];

                    for (i1, i2) in pairs {
                        let b1 = bones[i1];
                        let b2 = bones[i2];
                        if b1.length() > 0.0 && b2.length() > 0.0 {
                             if let (Some(s1), Some(s2)) = (
                                 game::math::w2s(&st.view_matrix, b1, overlay.width as f32, overlay.height as f32),
                                 game::math::w2s(&st.view_matrix, b2, overlay.width as f32, overlay.height as f32)
                             ) {
                                 overlay.draw_line(s1.x, s1.y, s2.x, s2.y, is_enemy);
                             }
                        }
                    }
                }
            }
        }
        
        overlay.end_scene();
    }

    Ok(())
}

// Helpers
fn get_pid(name: &str) -> Result<u32> {
    use windows::Win32::System::Diagnostics::ToolHelp::*;
    unsafe {
        let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)?;
        let mut entry = PROCESSENTRY32W { dwSize: std::mem::size_of::<PROCESSENTRY32W>() as u32, ..Default::default() };
        if Process32FirstW(snapshot, &mut entry).is_ok() {
            loop {
                let proc_name = String::from_utf16_lossy(&entry.szExeFile[..entry.szExeFile.iter().position(|&c| c == 0).unwrap_or(0)]);
                if proc_name == name {
                    let _ = windows::Win32::Foundation::CloseHandle(snapshot);
                    return Ok(entry.th32ProcessID);
                }
                if Process32NextW(snapshot, &mut entry).is_err() { break; }
            }
        }
        let _ = windows::Win32::Foundation::CloseHandle(snapshot);
        Err(anyhow::anyhow!("Process not found"))
    }
}

fn open_process(pid: u32) -> Result<windows::Win32::Foundation::HANDLE> {
    use windows::Win32::System::Threading::*;
    unsafe { Ok(OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, false, pid)?) }
}

fn get_module_base(pid: u32, name: &str) -> Result<usize> {
    use windows::Win32::System::Diagnostics::ToolHelp::*;
    unsafe {
        let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)?;
        let mut entry = MODULEENTRY32W { dwSize: std::mem::size_of::<MODULEENTRY32W>() as u32, ..Default::default() };
        if Module32FirstW(snapshot, &mut entry).is_ok() {
            loop {
                let mod_name = String::from_utf16_lossy(&entry.szModule[..entry.szModule.iter().position(|&c| c == 0).unwrap_or(0)]);
                if mod_name == name {
                    let _ = windows::Win32::Foundation::CloseHandle(snapshot);
                    return Ok(entry.modBaseAddr as usize);
                }
                if Module32NextW(snapshot, &mut entry).is_err() { break; }
            }
        }
        let _ = windows::Win32::Foundation::CloseHandle(snapshot);
        Err(anyhow::anyhow!("Module not found"))
    }
}
