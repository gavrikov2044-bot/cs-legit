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
                        _pid: pid,
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
        static mut FIRST_ENEMY_LOGGED: bool = false;
        loop {
            if let Ok(mut st) = state_clone.lock() {
                // Read Matrix
                if let Some(mat) = mem_clone.read::<[[f32; 4]; 4]>(mem_clone.client_base + offsets_clone.dw_view_matrix) {
                    st.view_matrix = mat;
                }

                // Read Local Player
                let mut local_team = 0;
                let local_ctrl: usize = mem_clone.read(mem_clone.client_base + offsets_clone.dw_local_player_controller).unwrap_or(0);
                const STRIDE: usize = 112; // 0x70 - from working C++ code
                
                if local_ctrl != 0 && local_ctrl < 0x7FF000000000 {
                     let pawn_h: u32 = mem_clone.read(local_ctrl + game::offsets::netvars::M_H_PLAYER_PAWN).unwrap_or(0);
                     let ent_list: usize = mem_clone.read(mem_clone.client_base + offsets_clone.dw_entity_list).unwrap_or(0);
                     
                     if ent_list != 0 && pawn_h != 0 && pawn_h != 0xFFFFFFFF {
                         let chunk_idx = (pawn_h as usize & 0x7FFF) >> 9;
                         let entry_idx = pawn_h as usize & 0x1FF;
                         let entry: usize = mem_clone.read(ent_list + 8 * chunk_idx + 0x10).unwrap_or(0);
                         if entry != 0 {
                             let pawn: usize = mem_clone.read(entry + entry_idx * STRIDE).unwrap_or(0);
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
                
                // Logging with diagnostics
                let should_log = last_debug.elapsed().as_secs() >= 3;
                if should_log {
                    last_debug = std::time::Instant::now();
                }
                
                let mut debug_stats = (0u32, 0u32, 0u32, 0u32, 0u32); // ctrl_found, pawn_h_ok, pawn_ok, health_ok, added
                
                if ent_list != 0 {
                    // Stride = 112 bytes (0x70) for CEntityIdentity - from working C++ code
                    const STRIDE: usize = 112;
                    
                    // Iterate through player slots (1-64, skip 0 which is world)
                    for i in 1..=64 {
                        // Get chunk for this index: entityList + 8 * (i >> 9) + 0x10
                        // For i < 512, chunk_idx = 0, so we read entityList + 0x10
                        let chunk_idx = (i & 0x7FFF) >> 9;
                        let list_entry: usize = mem_clone.read(ent_list + 8 * chunk_idx + 0x10).unwrap_or(0);
                        if list_entry == 0 { continue; }
                        
                        // Controller = entity pointer at list_entry + (i & 0x1FF) * stride
                        let entry_idx = i & 0x1FF;
                        let controller: usize = mem_clone.read(list_entry + entry_idx * STRIDE).unwrap_or(0);
                        if controller == 0 || controller > 0x7FF000000000 { continue; }
                        debug_stats.0 += 1;
                        
                        // Read pawn handle from controller
                        let pawn_h: u32 = mem_clone.read(controller + game::offsets::netvars::M_H_PLAYER_PAWN).unwrap_or(0);
                        
                        // Debug first valid controller with pawn (ONLY ONCE)
                        if should_log && debug_stats.0 == 1 && unsafe { !FIRST_ENEMY_LOGGED } {
                            unsafe { FIRST_ENEMY_LOGGED = true; }
                            info!("First ctrl[{}]: 0x{:X}, pawn_h=0x{:X}", i, controller, pawn_h);
                        }
                        
                        if pawn_h == 0 || pawn_h == 0xFFFFFFFF { continue; }
                        debug_stats.1 += 1;
                        
                        // Get pawn from entity list using pawn handle
                        let pawn_chunk_idx = (pawn_h as usize & 0x7FFF) >> 9;
                        let pawn_entry_idx = pawn_h as usize & 0x1FF;
                        let list_entry2: usize = mem_clone.read(ent_list + 8 * pawn_chunk_idx + 0x10).unwrap_or(0);
                        if list_entry2 == 0 { continue; }
                        
                        let pawn: usize = mem_clone.read(list_entry2 + pawn_entry_idx * STRIDE).unwrap_or(0);
                        if pawn == 0 || pawn > 0x7FF000000000 { continue; }
                        debug_stats.2 += 1;

                        // Health check
                        let health: i32 = mem_clone.read(pawn + game::offsets::netvars::M_I_HEALTH).unwrap_or(0);
                        
                        if health <= 0 || health > 100 { continue; }
                        debug_stats.3 += 1;
                        
                        let team: i32 = mem_clone.read(pawn + game::offsets::netvars::M_I_TEAM_NUM).unwrap_or(0);
                        
                        // Pos - read DIRECTLY from Pawn (m_vOldOrigin)
                        let pos: Vec3 = mem_clone.read(pawn + game::offsets::netvars::M_V_OLD_ORIGIN).unwrap_or(Vec3::ZERO);
                        
                        // Debug first pawn with position (ONLY ONCE)
                        if should_log && debug_stats.4 == 0 && unsafe { !FIRST_ENEMY_LOGGED } {
                            info!("First player: pawn=0x{:X} health={} team={} pos=({:.0},{:.0},{:.0})", 
                                  pawn, health, team, pos.x, pos.y, pos.z);
                        }
                        
                        // Skip if position is zero (invalid)
                        if pos.x == 0.0 && pos.y == 0.0 && pos.z == 0.0 { continue; }
                        
                        // Bones (disabled for now)
                        let bones = [Vec3::ZERO; 30];

                        st.entities.push(game::entity::Entity {
                            _pawn: pawn, _controller: controller, pos, _health: health, team, _bones: bones
                        });
                        debug_stats.4 += 1;
                    }
                }
                        
                        // Log stats every 3 seconds
                        if should_log {
                            info!("Entities: {} | LocalTeam: {}", debug_stats.4, st.local_team);
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
            let mut drawn = 0;
            let mut skipped_team = 0;
            let mut skipped_w2s = 0;
            let mut skipped_size = 0;
            
            for ent in &st.entities {
                // Count teammates but DON'T skip for now (debug)
                let is_teammate = ent.team == st.local_team;
                if is_teammate {
                    skipped_team += 1;
                    continue; // Re-enable this after testing
                }
                
                let is_enemy = !is_teammate;

                // Box - use pos directly with height offset
                let feet_pos = ent.pos;
                let head_pos = Vec3::new(ent.pos.x, ent.pos.y, ent.pos.z + 72.0);

                match (
                    game::math::w2s(&st.view_matrix, feet_pos, overlay.width as f32, overlay.height as f32),
                    game::math::w2s(&st.view_matrix, head_pos, overlay.width as f32, overlay.height as f32)
                ) {
                    (Some(s_feet), Some(s_head)) => {
                        let h = s_feet.y - s_head.y;
                        let w = h * 0.4;
                        let x = s_head.x - w / 2.0;
                        let y = s_head.y;
                        
                        // W2S debug (ONLY ONCE) - using static flag from memory thread
                        static mut W2S_LOGGED: bool = false;
                        if unsafe { !W2S_LOGGED } {
                            unsafe { W2S_LOGGED = true; }
                            // Calculate clip coords for debug
                            let clip_x = st.view_matrix[0][0] * ent.pos.x + st.view_matrix[0][1] * ent.pos.y + st.view_matrix[0][2] * ent.pos.z + st.view_matrix[0][3];
                            let clip_y = st.view_matrix[1][0] * ent.pos.x + st.view_matrix[1][1] * ent.pos.y + st.view_matrix[1][2] * ent.pos.z + st.view_matrix[1][3];
                            let clip_w = st.view_matrix[3][0] * ent.pos.x + st.view_matrix[3][1] * ent.pos.y + st.view_matrix[3][2] * ent.pos.z + st.view_matrix[3][3];
                            
                            info!("W2S: pos=({:.0},{:.0},{:.0}) clip_x={:.1} clip_y={:.1} clip_w={:.1} -> screen=({:.0},{:.0})", 
                                  ent.pos.x, ent.pos.y, ent.pos.z, clip_x, clip_y, clip_w, s_head.x, s_head.y);
                            info!("Matrix[3] (W row): [{:.3},{:.3},{:.3},{:.3}]", 
                                  st.view_matrix[3][0], st.view_matrix[3][1], st.view_matrix[3][2], st.view_matrix[3][3]);
                        }
                        
                        if h > 5.0 && h < 500.0 {
                            overlay.draw_box(x, y, w, h, is_enemy);
                            drawn += 1;
                        } else {
                            skipped_size += 1;
                        }
                    },
                    _ => {
                        skipped_w2s += 1;
                    }
                }
            }
            
            // Debug render stats occasionally (only if something interesting)
            static mut LAST_LOG: u64 = 0;
            unsafe {
                let now = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().as_secs();
                if now > LAST_LOG + 5 && (skipped_w2s > 0 || skipped_size > 0) {
                    LAST_LOG = now;
                    info!("Render issues: skip_w2s={} skip_size={} (ents={} drawn={})", 
                          skipped_w2s, skipped_size, st.entities.len(), drawn);
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
