/*
 * EXTERNA CS2 - Rust Edition v1.0
 * Based on Valthrun architecture
 * 
 * Features:
 * - Box ESP with Health & Armor bars
 * - Stream-Proof overlay (hidden from screen capture)
 * - Multi-threaded (Memory/Render split)
 * - WinAPI + GDI overlay (D3D11 coming soon)
 */

#![windows_subsystem = "windows"]

mod memory;
mod overlay;
mod esp;
mod offsets;

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::Duration;

use anyhow::Result;
use parking_lot::RwLock;
use log::{info, warn, error};

use crate::memory::GameMemory;
use crate::overlay::Overlay;
use crate::esp::EspData;

/// Global running state
pub static RUNNING: AtomicBool = AtomicBool::new(true);

/// Shared ESP data between threads
pub struct SharedState {
    pub esp_data: RwLock<EspData>,
    pub settings: RwLock<Settings>,
}

/// Cheat settings
#[derive(Clone)]
pub struct Settings {
    pub esp_enabled: bool,
    pub box_esp: bool,
    pub health_bar: bool,
    pub armor_bar: bool,
    pub menu_open: bool,
}

impl Default for Settings {
    fn default() -> Self {
        Self {
            esp_enabled: true,
            box_esp: true,
            health_bar: true,
            armor_bar: true,
            menu_open: false,
        }
    }
}

fn main() -> Result<()> {
    // Allocate console for logging
    unsafe {
        let _ = windows::Win32::System::Console::AllocConsole();
    }
    
    // Initialize logger
    env_logger::Builder::from_default_env()
        .filter_level(log::LevelFilter::Info)
        .format_timestamp(None)
        .init();
    
    println!();
    println!("╔═══════════════════════════════════════════╗");
    println!("║   EXTERNA CS2 - Rust Edition v1.0        ║");
    println!("║   Based on Valthrun Architecture         ║");
    println!("║                                           ║");
    println!("║   [INSERT] Open/Close Menu               ║");
    println!("║   [Stream-Proof] Enabled by default      ║");
    println!("╚═══════════════════════════════════════════╝");
    println!();

    // Initialize shared state
    let state = Arc::new(SharedState {
        esp_data: RwLock::new(EspData::default()),
        settings: RwLock::new(Settings::default()),
    });

    // Wait for game
    info!("Waiting for cs2.exe...");
    let game = loop {
        match GameMemory::attach() {
            Ok(g) => break g,
            Err(_) => {
                print!(".");
                thread::sleep(Duration::from_secs(1));
            }
        }
    };
    println!();
    info!("Attached to CS2!");

    // Memory thread
    let state_clone = Arc::clone(&state);
    let game_clone = game.clone();
    let memory_thread = thread::spawn(move || {
        info!("Memory thread started (2ms tick)");
        while RUNNING.load(Ordering::Relaxed) {
            if let Ok(data) = game_clone.read_entities() {
                *state_clone.esp_data.write() = data;
            }
            thread::sleep(Duration::from_millis(2));
        }
        info!("Memory thread stopped");
    });

    // Overlay (main thread)
    info!("Creating overlay...");
    let mut overlay = match Overlay::create(Arc::clone(&state)) {
        Ok(o) => o,
        Err(e) => {
            error!("Failed to create overlay: {}", e);
            return Err(e);
        }
    };
    
    info!("Overlay ready! Press INSERT to open menu.");
    println!();
    
    // Main loop
    if let Err(e) = overlay.run() {
        error!("Overlay error: {}", e);
    }

    // Cleanup
    RUNNING.store(false, Ordering::Relaxed);
    let _ = memory_thread.join();
    
    info!("Goodbye!");
    Ok(())
}
