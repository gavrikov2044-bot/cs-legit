mod memory;
mod offsets;
mod overlay;
mod syscall;

use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;
use glam::{Vec3, Vec2};
use log::info;

use memory::Memory;
use overlay::{Overlay, RUNNING};
use std::sync::atomic::Ordering;

// --- Game State ---
struct GameState {
    view_matrix: [[f32; 4]; 4],
    entities: Vec<Entity>,
    local_team: i32,
}

#[derive(Clone, Copy)]
struct Entity {
    pos: Vec3,
    health: i32,
    team: i32,
}

fn main() -> anyhow::Result<()> {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info")).init();
    
    println!("╔═══════════════════════════════════════╗");
    println!("║     EXTERNA CS2 - Rust Edition        ║");
    println!("║         Press END to close            ║");
    println!("╚═══════════════════════════════════════╝");
    
    info!("Waiting for cs2.exe...");

    let mem = loop {
        if let Some(m) = Memory::new("cs2.exe") {
            info!("Attached! Client: 0x{:X}", m.client_base);
            break Arc::new(m);
        }
        thread::sleep(Duration::from_secs(1));
    };

    let state = Arc::new(Mutex::new(GameState {
        view_matrix: [[0.0; 4]; 4],
        entities: Vec::new(),
        local_team: 0,
    }));

    // Memory Thread
    let mem_clone = mem.clone();
    let state_clone = state.clone();

    thread::spawn(move || {
        info!("Memory thread started");
        loop {
            if !RUNNING.load(Ordering::Relaxed) { break; }
            
            if let Ok(mut st) = state_clone.lock() {
                st.view_matrix = mem_clone.read(mem_clone.client_base + offsets::client::DW_VIEW_MATRIX);

                let local: u64 = mem_clone.read(mem_clone.client_base + offsets::client::DW_LOCAL_PLAYER_CONTROLLER);
                if local != 0 {
                    let pawn_h: u32 = mem_clone.read(local + offsets::offsets::M_H_PLAYER_PAWN);
                    let ent_list: u64 = mem_clone.read(mem_clone.client_base + offsets::client::DW_ENTITY_LIST);

                    if ent_list != 0 && pawn_h != 0 {
                        let entry: u64 = mem_clone.read(ent_list + 8 * ((pawn_h as u64 & 0x7FFF) >> 9) + 16);
                        if entry != 0 {
                            let pawn: u64 = mem_clone.read(entry + 120 * (pawn_h as u64 & 0x1FF));
                            if pawn != 0 {
                                st.local_team = mem_clone.read(pawn + offsets::offsets::M_I_TEAM_NUM);
                            }
                        }
                    }

                    st.entities.clear();

                    if ent_list != 0 {
                        for i in 1u64..64 {
                            let le: u64 = mem_clone.read(ent_list + 8 * ((i & 0x7FFF) >> 9) + 16);
                            if le == 0 { continue; }

                            let ctrl: u64 = mem_clone.read(le + 120 * (i & 0x1FF));
                            if ctrl == 0 { continue; }

                            let ph: u32 = mem_clone.read(ctrl + offsets::offsets::M_H_PLAYER_PAWN);
                            if ph == 0 { continue; }

                            let le2: u64 = mem_clone.read(ent_list + 8 * ((ph as u64 & 0x7FFF) >> 9) + 16);
                            if le2 == 0 { continue; }
                            
                            let pawn: u64 = mem_clone.read(le2 + 120 * (ph as u64 & 0x1FF));
                            if pawn == 0 { continue; }

                            let hp: i32 = mem_clone.read(pawn + offsets::offsets::M_I_HEALTH);
                            if hp <= 0 || hp > 100 { continue; }

                            let team: i32 = mem_clone.read(pawn + offsets::offsets::M_I_TEAM_NUM);

                            let sn: u64 = mem_clone.read(pawn + offsets::offsets::M_P_GAME_SCENE_NODE);
                            if sn == 0 { continue; }
                            
                            let pos: Vec3 = mem_clone.read(sn + offsets::offsets::M_VEC_ABS_ORIGIN);

                            st.entities.push(Entity { pos, health: hp, team });
                        }
                    }
                }
            }
            thread::sleep(Duration::from_millis(2));
        }
        info!("Memory thread stopped");
    });

    // Create Overlay
    let overlay = Overlay::new()?;
    info!("Overlay ready!");

    let mut frame_count = 0u64;

    // Main loop
    while RUNNING.load(Ordering::Relaxed) {
        // Process Windows messages (CRITICAL!)
        if !overlay.process_messages() {
            break;
        }

        // Clear previous frame
        overlay.clear();

        // Draw ESP
        if let Ok(state) = state.lock() {
            let enemy_count = state.entities.iter()
                .filter(|e| e.team != state.local_team && e.team != 0)
                .count();

            for ent in &state.entities {
                // Skip teammates
                if ent.team == state.local_team || ent.team == 0 { continue; }

                let feet_pos = ent.pos;
                let head_pos = Vec3::new(feet_pos.x, feet_pos.y, feet_pos.z + 72.0);

                if let (Some(feet), Some(head)) = (
                    w2s(&state.view_matrix, feet_pos),
                    w2s(&state.view_matrix, head_pos)
                ) {
                    let h = feet.y - head.y;
                    if h < 5.0 { continue; } // Too small
                    
                    let w = h / 2.0;
                    let x = head.x - w / 2.0;
                    let y = head.y;

                    // Team colors: CT = Blue, T = Yellow/Orange
                    let (r, g, b) = if ent.team == 2 { (255, 200, 0) } else { (50, 150, 255) };

                    // Draw outlined box
                    overlay.draw_outlined_box(x, y, w, h, r, g, b);

                    // Health bar
                    let hp_h = (h * ent.health as f32) / 100.0;
                    let hp_ratio = ent.health as f32 / 100.0;
                    let hp_r = ((1.0 - hp_ratio) * 255.0) as u8;
                    let hp_g = (hp_ratio * 255.0) as u8;
                    
                    // Health bar background (black)
                    overlay.draw_filled_rect(x - 6.0, y, 4.0, h, 0, 0, 0);
                    // Health bar fill
                    overlay.draw_filled_rect(x - 5.0, y + h - hp_h, 3.0, hp_h, hp_r, hp_g, 0);
                }
            }

            // Log every 100 frames
            frame_count += 1;
            if frame_count % 100 == 0 {
                info!("Frame {}: {} enemies visible", frame_count, enemy_count);
            }
        }

        // ~60 FPS
        thread::sleep(Duration::from_millis(16));
    }

    info!("Shutting down...");
    Ok(())
}

fn w2s(matrix: &[[f32; 4]; 4], pos: Vec3) -> Option<Vec2> {
    let w = matrix[3][0] * pos.x + matrix[3][1] * pos.y + matrix[3][2] * pos.z + matrix[3][3];
    if w < 0.001 { return None; }
    
    let x = matrix[0][0] * pos.x + matrix[0][1] * pos.y + matrix[0][2] * pos.z + matrix[0][3];
    let y = matrix[1][0] * pos.x + matrix[1][1] * pos.y + matrix[1][2] * pos.z + matrix[1][3];
    
    // 1920x1080 screen
    Some(Vec2::new(
        960.0 * (1.0 + x / w),
        540.0 * (1.0 - y / w)
    ))
}
