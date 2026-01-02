mod memory;
mod offsets;
mod overlay;

use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;
use glam::{Vec3, Vec2};
use log::{info, error};

use memory::Memory;
use overlay::D3D11Overlay;
use windows::Win32::Graphics::Direct2D::Common::D2D1_COLOR_F;
use windows::Win32::Graphics::Direct2D::ID2D1RenderTarget;

// Game structures
struct GameState {
    view_matrix: [[f32; 4]; 4],
    entities: Vec<Entity>,
}

#[derive(Clone, Copy)]
struct Entity {
    pos: Vec3,
    health: i32,
    team: i32,
}

fn main() -> anyhow::Result<()> {
    env_logger::Builder::from_default_env()
        .filter_level(log::LevelFilter::Debug)
        .init();
    
    info!("Starting Externa Rust V2...");

    // 1. Initialize Memory
    info!("Waiting for cs2.exe...");
    let mem = loop {
        if let Some(m) = Memory::new("cs2.exe") {
            info!("Attached to CS2! Base: 0x{:X}", m.client_base);
            break Arc::new(m);
        }
        thread::sleep(Duration::from_secs(1));
    };

    let state = Arc::new(Mutex::new(GameState { 
        view_matrix: [[0.0; 4]; 4], 
        entities: Vec::new() 
    }));
    
    // 2. Memory Thread
    let mem_clone = mem.clone();
    let state_clone = state.clone();
    thread::spawn(move || {
        loop {
            if let Ok(mut state) = state_clone.lock() {
                // Read ViewMatrix
                state.view_matrix = mem_clone.read(mem_clone.client_base + offsets::client::DW_VIEW_MATRIX);
                
                state.entities.clear();
                
                let local_player: u64 = mem_clone.read(mem_clone.client_base + offsets::client::DW_LOCAL_PLAYER_CONTROLLER);
                if local_player == 0 { continue; }
                
                let entity_list: u64 = mem_clone.read(mem_clone.client_base + offsets::client::DW_ENTITY_LIST);
                if entity_list == 0 { continue; }

                // Iterate entities (Max 64)
                for i in 1..64 {
                    let list_entry: u64 = mem_clone.read(entity_list + 8 * ((i & 0x7FFF) >> 9) + 16);
                    if list_entry == 0 { continue; }
                    
                    let controller: u64 = mem_clone.read(list_entry + 120 * (i & 0x1FF));
                    if controller == 0 { continue; }
                    
                    let pawn_handle: u32 = mem_clone.read(controller + offsets::offsets::M_H_PLAYER_PAWN);
                    if pawn_handle == 0 { continue; }
                    
                    let list_entry2: u64 = mem_clone.read(entity_list + 0x8 * ((pawn_handle as u64 & 0x7FFF) >> 9) + 16);
                    let pawn: u64 = mem_clone.read(list_entry2 + 120 * (pawn_handle as u64 & 0x1FF));
                    if pawn == 0 { continue; }
                    
                    let health: i32 = mem_clone.read(pawn + offsets::offsets::M_I_HEALTH);
                    if health <= 0 || health > 100 { continue; }
                    
                    let team: i32 = mem_clone.read(pawn + offsets::offsets::M_I_TEAM_NUM);
                    
                    let scene_node: u64 = mem_clone.read(pawn + offsets::offsets::M_P_GAME_SCENE_NODE);
                    let origin: Vec3 = mem_clone.read(scene_node + offsets::offsets::M_VEC_ABS_ORIGIN);
                    
                    state.entities.push(Entity { pos: origin, health, team });
                }
            }
            thread::sleep(Duration::from_millis(2));
        }
    });

    // 3. Overlay
    let overlay = D3D11Overlay::new()?;
    info!("Overlay created!");

    overlay.render_loop(|target, brush| {
        let state = state.lock().unwrap();
        
        // Draw test box
        unsafe {
            let rect = windows::Win32::Graphics::Direct2D::Common::D2D_RECT_F {
                left: 10.0, top: 10.0, right: 110.0, bottom: 60.0
            };
            brush.SetColor(&D2D1_COLOR_F { r: 0.0, g: 1.0, b: 0.0, a: 1.0 });
            target.DrawRectangle(&rect, brush, 2.0, None);
        }

        // Draw ESP
        for entity in &state.entities {
            let feet_pos = entity.pos;
            let head_pos = Vec3::new(feet_pos.x, feet_pos.y, feet_pos.z + 72.0);
            
            if let (Some(feet), Some(head)) = (
                w2s(&state.view_matrix, feet_pos),
                w2s(&state.view_matrix, head_pos)
            ) {
                let height = feet.y - head.y;
                let width = height / 2.0;
                let x = head.x - width / 2.0;
                let y = head.y;
                
                unsafe {
                    let rect = windows::Win32::Graphics::Direct2D::Common::D2D_RECT_F {
                        left: x, top: y, right: x + width, bottom: y + height
                    };
                    
                    // Health color
                    let g = entity.health as f32 / 100.0;
                    let r = 1.0 - g;
                    brush.SetColor(&D2D1_COLOR_F { r, g, b: 0.0, a: 1.0 });
                    
                    target.DrawRectangle(&rect, brush, 1.5, None);
                }
            }
        }
    });

    Ok(())
}

fn w2s(matrix: &[[f32; 4]; 4], pos: Vec3) -> Option<Vec2> {
    let w = matrix[3][0] * pos.x + matrix[3][1] * pos.y + matrix[3][2] * pos.z + matrix[3][3];
    
    if w < 0.001 { return None; }
    
    let x = matrix[0][0] * pos.x + matrix[0][1] * pos.y + matrix[0][2] * pos.z + matrix[0][3];
    let y = matrix[1][0] * pos.x + matrix[1][1] * pos.y + matrix[1][2] * pos.z + matrix[1][3];
    
    let inv_w = 1.0 / w;
    let screen_x = (1920.0 / 2.0) * (1.0 + x * inv_w);
    let screen_y = (1080.0 / 2.0) * (1.0 - y * inv_w);
    
    Some(Vec2::new(screen_x, screen_y))
}
