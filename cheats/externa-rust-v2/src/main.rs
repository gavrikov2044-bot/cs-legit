mod memory;
mod offsets;
mod overlay;

use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;
use glam::Vec3;

use memory::Memory;
use overlay::D3D11Overlay;

struct GameState {
    entities: Vec<Entity>,
}

#[derive(Clone, Copy)]
struct Entity {
    pos: Vec3,
    health: i32,
}

fn main() -> anyhow::Result<()> {
    env_logger::init();
    
    // 1. Initialize Memory
    println!("Waiting for cs2.exe...");
    let mem = loop {
        if let Some(m) = Memory::new("cs2.exe") {
            println!("Attached to CS2!");
            break Arc::new(m);
        }
        thread::sleep(Duration::from_secs(1));
    };

    let state = Arc::new(Mutex::new(GameState { entities: Vec::new() }));
    
    // 2. Memory Thread
    let mem_clone = mem.clone();
    let state_clone = state.clone();
    thread::spawn(move || {
        loop {
            if let Ok(mut state) = state_clone.lock() {
                state.entities.clear();
                
                let local_player: u64 = mem_clone.read(mem_clone.client_base + offsets::client::DW_LOCAL_PLAYER_CONTROLLER);
                if local_player != 0 {
                     // Read entities (Simplified for demo)
                     // In real code: Iterate entity list -> check team -> get position
                     // This part needs the full entity iteration logic from Valthrun/ProExt
                }
            }
            thread::sleep(Duration::from_millis(10));
        }
    });

    // 3. Overlay
    let overlay = D3D11Overlay::new()?;
    println!("Overlay created!");

    overlay.render_loop(|ctx, rtv| {
        // Drawing logic here
        // Direct3D 11 raw drawing is verbose (vertex buffers etc).
        // For simplicity in this "from scratch" version, we need a 2D drawer.
        // I will add a simple Line/Rect drawer helper next.
    });

    Ok(())
}

