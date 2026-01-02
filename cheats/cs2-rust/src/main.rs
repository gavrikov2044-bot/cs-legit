/*
 * EXTERNA CS2 - Rust Edition v1.0
 * Based on Valthrun Architecture
 * 
 * Features:
 * - Box ESP with Health & Armor bars
 * - Stream-Proof overlay (hidden from screen capture)
 * - Pattern Scanner for automatic offsets
 * - Config file (JSON)
 * - Multi-threaded architecture
 */

#![windows_subsystem = "windows"]

mod memory;
mod overlay;
mod esp;
mod offsets;
mod scanner;
mod config;
mod driver_client;

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::Duration;

use anyhow::Result;
use parking_lot::RwLock;
use log::{info, error};

use crate::memory::GameMemory;
use crate::overlay::Overlay;
use crate::esp::EspData;
use crate::config::Config;
use crate::driver_client::DriverClient;

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

impl From<Config> for Settings {
    fn from(config: Config) -> Self {
        Self {
            esp_enabled: config.esp_enabled,
            box_esp: config.box_esp,
            health_bar: config.health_bar,
            armor_bar: config.armor_bar,
            menu_open: false,
        }
    }
}

fn main() -> Result<()> {
    // Allocate console
    unsafe {
        let _ = windows::Win32::System::Console::AllocConsole();
    }
    
    // Initialize logger with DEBUG level
    env_logger::Builder::from_default_env()
        .filter_level(log::LevelFilter::Debug)
        .format_timestamp(None)
        .init();
    
    print_banner();
    
    // Load config
    let config = Config::load();
    info!("Config loaded");
    
    // Try to connect to kernel driver
    let driver_mode = if DriverClient::is_available() {
        info!("Kernel driver detected - using Ring 0 mode");
        true
    } else {
        info!("No driver - using WinAPI mode");
        false
    };
    let _ = driver_mode; // Used in GameMemory

    // Initialize shared state
    let state = Arc::new(SharedState {
        esp_data: RwLock::new(EspData::default()),
        settings: RwLock::new(Settings::from(config)),
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
    info!("Client base: 0x{:X}", game.get_client_base());

    // Memory thread
    let state_clone = Arc::clone(&state);
    let game_clone = game.clone();
    let memory_thread = thread::spawn(move || {
        info!("Memory thread started (2ms tick)");
        let mut frame_count = 0u64;
        
        while RUNNING.load(Ordering::Relaxed) {
            if let Ok(data) = game_clone.read_entities() {
                let entity_count = data.entities.len();
                *state_clone.esp_data.write() = data;
                
                // Log every 500 frames (~1 sec)
                frame_count += 1;
                if frame_count % 500 == 0 {
                    info!("ESP: {} enemies visible", entity_count);
                }
            }
            thread::sleep(Duration::from_millis(2));
        }
        info!("Memory thread stopped");
    });

    // Create overlay
    info!("Creating overlay...");
    let mut overlay = match Overlay::create(Arc::clone(&state)) {
        Ok(o) => o,
        Err(e) => {
            error!("Failed to create overlay: {}", e);
            return Err(e);
        }
    };
    
    info!("Overlay ready!");
    println!();
    info!("Press INSERT to open menu");
    info!("Stream-Proof: ENABLED (hidden from recordings)");
    println!();
    
    // Main loop
    if let Err(e) = overlay.run() {
        error!("Overlay error: {}", e);
    }

    // Cleanup
    RUNNING.store(false, Ordering::Relaxed);
    let _ = memory_thread.join();
    
    // Save config on exit
    let settings = state.settings.read();
    let config = Config {
        esp_enabled: settings.esp_enabled,
        box_esp: settings.box_esp,
        health_bar: settings.health_bar,
        armor_bar: settings.armor_bar,
        stream_proof: true,
    };
    let _ = config.save();
    
    info!("Goodbye!");
    Ok(())
}

fn print_banner() {
    println!();
    println!("╔═══════════════════════════════════════════════════╗");
    println!("║                                                   ║");
    println!("║   ███████╗██╗  ██╗████████╗███████╗██████╗ ███╗   ██╗ █████╗  ║");
    println!("║   ██╔════╝╚██╗██╔╝╚══██╔══╝██╔════╝██╔══██╗████╗  ██║██╔══██╗ ║");
    println!("║   █████╗   ╚███╔╝    ██║   █████╗  ██████╔╝██╔██╗ ██║███████║ ║");
    println!("║   ██╔══╝   ██╔██╗    ██║   ██╔══╝  ██╔══██╗██║╚██╗██║██╔══██║ ║");
    println!("║   ███████╗██╔╝ ██╗   ██║   ███████╗██║  ██║██║ ╚████║██║  ██║ ║");
    println!("║   ╚══════╝╚═╝  ╚═╝   ╚═╝   ╚══════╝╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝  ╚═╝ ║");
    println!("║                                                   ║");
    println!("║              CS2 External ESP v1.0                ║");
    println!("║           Based on Valthrun Architecture          ║");
    println!("║                                                   ║");
    println!("╚═══════════════════════════════════════════════════╝");
    println!();
}
