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

    // 3. Offsets - Use hardcoded from dump (scanner gave wrong results)
    // Pattern scanner disabled - Set 3 gave wrong offsets (0x20256C0 vs 0x1D13CE8)
    info!("Using HARDCODED offsets from dump (scanner disabled)...");
    info!("  dwEntityList: 0x{:X}", game::offsets::DW_ENTITY_LIST);
    info!("  dwLocalPlayerController: 0x{:X}", game::offsets::DW_LOCAL_PLAYER_CONTROLLER);
    info!("  dwViewMatrix: 0x{:X}", game::offsets::DW_VIEW_MATRIX);
    
    let offsets = Arc::new(game::offsets::Offsets {
        dw_entity_list: game::offsets::DW_ENTITY_LIST,
        dw_local_player_controller: game::offsets::DW_LOCAL_PLAYER_CONTROLLER,
        dw_view_matrix: game::offsets::DW_VIEW_MATRIX,
    });
    
    // Verify offsets by reading and checking if values look valid
    let test_ent_list: usize = mem.read(mem.client_base + offsets.dw_entity_list).unwrap_or(0);
    let test_local: usize = mem.read(mem.client_base + offsets.dw_local_player_controller).unwrap_or(0);
    info!("Verification:");
    info!("  EntityList ptr: 0x{:X} (should be heap: {})", test_ent_list, test_ent_list > 0x10000000000);
    info!("  LocalCtrl ptr: 0x{:X} (should be heap: {})", test_local, test_local > 0x10000000000);
    
    // If EntityList is not a heap pointer, offsets are wrong!
    if test_ent_list < 0x10000000000 {
        log::error!("WARNING: EntityList pointer looks WRONG! Offsets may be outdated.");
        log::error!("Please update offsets in src/game/offsets.rs with fresh dump!");
    }
    
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
                if local_ctrl != 0 && local_ctrl < 0x7FF000000000 {
                     let pawn_h: u32 = mem_clone.read(local_ctrl + game::offsets::netvars::M_H_PLAYER_PAWN).unwrap_or(0);
                     let ent_list: usize = mem_clone.read(mem_clone.client_base + offsets_clone.dw_entity_list).unwrap_or(0);
                     
                     if ent_list != 0 && pawn_h != 0 && pawn_h != 0xFFFFFFFF {
                         let entry: usize = mem_clone.read(ent_list + 8 * ((pawn_h as usize & 0x7FFF) >> 9) + 16).unwrap_or(0);
                         if entry != 0 {
                             // CEntityIdentity: stride=120, entity pointer at +0x08
                             let pawn: usize = mem_clone.read(entry + 120 * (pawn_h as usize & 0x1FF) + 8).unwrap_or(0);
                             if pawn != 0 && pawn < 0x7FF000000000 {
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
                if last_debug.elapsed().as_secs() >= 3 {
                    should_log = true;
                    last_debug = std::time::Instant::now();
                    
                    // Детальные логи адресов
                    let base = mem_clone.client_base;
                    info!("=== DEBUG FRAME ===");
                    info!("client_base = 0x{:X}", base);
                    info!("dwEntityList offset = 0x{:X}, read addr = 0x{:X}", offsets_clone.dw_entity_list, base + offsets_clone.dw_entity_list);
                    info!("dwLocalPlayerController offset = 0x{:X}, read addr = 0x{:X}", offsets_clone.dw_local_player_controller, base + offsets_clone.dw_local_player_controller);
                    info!("dwViewMatrix offset = 0x{:X}, read addr = 0x{:X}", offsets_clone.dw_view_matrix, base + offsets_clone.dw_view_matrix);
                    info!("EntityListPtr = 0x{:X} (is_heap: {})", ent_list, ent_list > 0x10000000000);
                    
                    let local_ptr: usize = mem_clone.read(base + offsets_clone.dw_local_player_controller).unwrap_or(0);
                    info!("LocalCtrlPtr = 0x{:X} (is_heap: {})", local_ptr, local_ptr > 0x10000000000);
                    
                    // Dump first 64 bytes of EntityList to understand structure
                    if ent_list > 0x10000000000 {
                        info!("--- EntityList structure (first 64 bytes) ---");
                        for off in (0..64).step_by(8) {
                            let val: usize = mem_clone.read(ent_list + off).unwrap_or(0);
                            info!("  +0x{:02X}: 0x{:016X}", off, val);
                        }
                        
                        // Try reading chunk at offset +16 (standard)
                        let chunk0: usize = mem_clone.read(ent_list + 16).unwrap_or(0);
                        info!("--- Chunk[0] at ent_list+16 = 0x{:X} ---", chunk0);
                        
                        if chunk0 > 0x10000000000 {
                            // CEntityIdentity structure is 120 bytes:
                            // +0x00: vtable (module address)
                            // +0x08: entity pointer (heap address) ← we need this!
                            // +0x10: handle, flags, etc.
                            info!("--- Testing CEntityIdentity structure (stride=120, entity at +0x08) ---");
                            for i in 1..6 {
                                // Read first field (vtable) and second field (entity ptr)
                                let vtable: usize = mem_clone.read(chunk0 + 120 * i).unwrap_or(0);
                                let entity_ptr: usize = mem_clone.read(chunk0 + 120 * i + 8).unwrap_or(0);
                                
                                let is_valid_entity = entity_ptr > 0x10000000000 && entity_ptr < 0x7FF000000000;
                                
                                info!("  Entity[{}]: vtable=0x{:X}, entity_ptr=0x{:X} (valid: {})", 
                                      i, vtable, entity_ptr, is_valid_entity);
                                
                                if is_valid_entity {
                                    // Try to read pawn handle from entity
                                    let pawn_h: u32 = mem_clone.read(entity_ptr + game::offsets::netvars::M_H_PLAYER_PAWN).unwrap_or(0);
                                    let health: i32 = mem_clone.read(entity_ptr + game::offsets::netvars::M_I_HEALTH).unwrap_or(0);
                                    info!("    -> pawn_handle=0x{:X}, health={}", pawn_h, health);
                                    
                                    if pawn_h != 0 && pawn_h != 0xFFFFFFFF {
                                        info!("    *** VALID PLAYER FOUND! ***");
                                    }
                                }
                            }
                        } else {
                            info!("  Chunk[0] is NOT a heap pointer - structure may be different!");
                        }
                    } else {
                        info!("EntityListPtr is NOT a heap pointer! Offsets are WRONG!");
                    }
                    info!("Entities found this frame: {}", st.entities.len());
                    info!("===================");
                }

                if ent_list != 0 {
                    for i in 1..64 {
                        let list_entry: usize = mem_clone.read(ent_list + 8 * ((i & 0x7FFF) >> 9) + 16).unwrap_or(0);
                        if list_entry == 0 { continue; }
                        
                        // CEntityIdentity: stride=120, entity pointer at +0x08
                        let controller: usize = mem_clone.read(list_entry + 120 * (i & 0x1FF) + 8).unwrap_or(0);
                        if controller == 0 || controller > 0x7FF000000000 { continue; } // Skip module addresses
                        
                        let pawn_h: u32 = mem_clone.read(controller + game::offsets::netvars::M_H_PLAYER_PAWN).unwrap_or(0);

                        if pawn_h == 0 || pawn_h == 0xFFFFFFFF { continue; }
                        
                        let list_entry2: usize = mem_clone.read(ent_list + 8 * ((pawn_h as usize & 0x7FFF) >> 9) + 16).unwrap_or(0);
                        if list_entry2 == 0 { continue; }
                        
                        // CEntityIdentity: stride=120, entity pointer at +0x08
                        let pawn: usize = mem_clone.read(list_entry2 + 120 * (pawn_h as usize & 0x1FF) + 8).unwrap_or(0);
                        if pawn == 0 || pawn > 0x7FF000000000 { continue; }

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
