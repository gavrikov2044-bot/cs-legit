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
    env_logger::init();
    info!("Waiting for cs2.exe...");

    let mem = loop {
        if let Some(m) = Memory::new("cs2.exe") {
            info!("Attached! Base: 0x{:X}", m.client_base);
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
        loop {
            if let Ok(mut st) = state_clone.lock() {
                st.view_matrix = mem_clone.read(mem_clone.client_base + offsets::client::DW_VIEW_MATRIX);

                let local: u64 = mem_clone.read(mem_clone.client_base + offsets::client::DW_LOCAL_PLAYER_CONTROLLER);
                if local != 0 {
                    let pawn_h: u32 = mem_clone.read(local + offsets::offsets::M_H_PLAYER_PAWN);
                    let ent_list: u64 = mem_clone.read(mem_clone.client_base + offsets::client::DW_ENTITY_LIST);

                    if ent_list != 0 {
                        let entry: u64 = mem_clone.read(ent_list + 0x8 * ((pawn_h as u64 & 0x7FFF) >> 9) + 16);
                        let pawn: u64 = mem_clone.read(entry + 120 * (pawn_h as u64 & 0x1FF));
                        st.local_team = mem_clone.read(pawn + offsets::offsets::M_I_TEAM_NUM);
                    }

                    st.entities.clear();

                    if ent_list != 0 {
                        for i in 1..64 {
                            let le: u64 = mem_clone.read(ent_list + 8 * ((i & 0x7FFF) >> 9) + 16);
                            if le == 0 { continue; }

                            let ctrl: u64 = mem_clone.read(le + 120 * (i & 0x1FF));
                            if ctrl == 0 { continue; }

                            let ph: u32 = mem_clone.read(ctrl + offsets::offsets::M_H_PLAYER_PAWN);
                            if ph == 0 { continue; }

                            let le2: u64 = mem_clone.read(ent_list + 0x8 * ((ph as u64 & 0x7FFF) >> 9) + 16);
                            let pawn: u64 = mem_clone.read(le2 + 120 * (ph as u64 & 0x1FF));
                            if pawn == 0 { continue; }

                            let hp: i32 = mem_clone.read(pawn + offsets::offsets::M_I_HEALTH);
                            if hp <= 0 || hp > 100 { continue; }

                            let team: i32 = mem_clone.read(pawn + offsets::offsets::M_I_TEAM_NUM);

                            let sn: u64 = mem_clone.read(pawn + offsets::offsets::M_P_GAME_SCENE_NODE);
                            let pos: Vec3 = mem_clone.read(sn + offsets::offsets::M_VEC_ABS_ORIGIN);

                            st.entities.push(Entity { pos, health: hp, team });
                        }
                    }
                }
            }
            thread::sleep(Duration::from_millis(2));
        }
    });

    // Overlay
    let overlay = Overlay::new()?;
    info!("Overlay ready! Press END to close.");

    while RUNNING.load(Ordering::Relaxed) {
        let state = state.lock().unwrap();

        for ent in &state.entities {
            if ent.team == state.local_team { continue; }

            let feet_pos = ent.pos;
            let head_pos = Vec3::new(feet_pos.x, feet_pos.y, feet_pos.z + 72.0);

            if let (Some(feet), Some(head)) = (
                w2s(&state.view_matrix, feet_pos),
                w2s(&state.view_matrix, head_pos)
            ) {
                let h = feet.y - head.y;
                let w = h / 2.0;
                let x = head.x - w / 2.0;
                let y = head.y;

                let (r, g, b) = if ent.team == 2 { (255, 200, 0) } else { (50, 150, 255) };

                overlay.draw_box(x, y, w, h, r, g, b);

                let hp_h = (h * ent.health as f32) / 100.0;
                let hp_r = 255 - (ent.health as u8 * 255 / 100);
                let hp_g = ent.health as u8 * 255 / 100;
                overlay.draw_filled_rect(x - 5.0, y + h - hp_h, 3.0, hp_h, hp_r, hp_g, 0);
            }
        }

        thread::sleep(Duration::from_millis(8));
    }

    Ok(())
}

fn w2s(matrix: &[[f32; 4]; 4], pos: Vec3) -> Option<Vec2> {
    let w = matrix[3][0] * pos.x + matrix[3][1] * pos.y + matrix[3][2] * pos.z + matrix[3][3];
    if w < 0.001 { return None; }
    let x = matrix[0][0] * pos.x + matrix[0][1] * pos.y + matrix[0][2] * pos.z + matrix[0][3];
    let y = matrix[1][0] * pos.x + matrix[1][1] * pos.y + matrix[1][2] * pos.z + matrix[1][3];
    Some(Vec2::new(960.0 * (1.0 + x / w), 540.0 * (1.0 - y / w)))
}
