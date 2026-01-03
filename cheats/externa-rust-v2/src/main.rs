//! Externa Rust V2 - External ESP for CS2
//! Features: Box ESP, Skeleton ESP, Snaplines, Health bars, Hotkey toggle

mod game;
mod memory;
mod overlay;

use std::fs::File;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::thread;
use std::time::Duration;
use anyhow::Result;
use glam::Vec3;
use log::{info, warn, error, LevelFilter};
use env_logger::Builder;
use parking_lot::Mutex;
use windows::Win32::UI::Input::KeyboardAndMouse::GetAsyncKeyState;

use crate::game::entity::{Entity, Bones};
use crate::game::offsets::{Offsets, netvars};

// ============================================================================
// Logging Setup
// ============================================================================

/// Initialize logging to both console and file (externa.log)
fn init_logging() {
    // Create log file
    let log_file = File::create("externa.log").ok();
    let log_file = Arc::new(Mutex::new(log_file));
    let log_file_clone = log_file.clone();
    
    Builder::new()
        .filter_level(LevelFilter::Info)
        .format(move |buf, record| {
            use std::io::Write;
            let timestamp = chrono::Local::now().format("%Y-%m-%d %H:%M:%S");
            let line = format!("[{}] [{}] {}\n", timestamp, record.level(), record.args());
            
            // Write to file
            if let Some(ref mut file) = *log_file_clone.lock() {
                let _ = file.write_all(line.as_bytes());
                let _ = file.flush();
            }
            
            // Write to console
            write!(buf, "{}", line)
        })
        .init();
}

// ============================================================================
// Configuration
// ============================================================================

/// ESP Settings
struct EspConfig {
    enabled: AtomicBool,
    show_box: AtomicBool,
    show_skeleton: AtomicBool,
    show_snaplines: AtomicBool,
    show_health: AtomicBool,
}

impl Default for EspConfig {
    fn default() -> Self {
        Self {
            enabled: AtomicBool::new(true),
            show_box: AtomicBool::new(true),
            show_skeleton: AtomicBool::new(true),
            show_snaplines: AtomicBool::new(false),
            show_health: AtomicBool::new(true),
        }
    }
}

// ============================================================================
// Game State
// ============================================================================

struct GameState {
    entities: Vec<Entity>,
    local_team: i32,
}

// No smoothing - direct W2S coordinates for perfect lock
// ViewMatrix is read in render loop so no jitter

// ============================================================================
// Main
// ============================================================================

fn main() -> Result<()> {
    // Set up panic handler to log crashes
    std::panic::set_hook(Box::new(|panic_info| {
        let msg = if let Some(s) = panic_info.payload().downcast_ref::<&str>() {
            s.to_string()
        } else if let Some(s) = panic_info.payload().downcast_ref::<String>() {
            s.clone()
        } else {
            "Unknown panic".to_string()
        };
        
        let location = panic_info.location()
            .map(|l| format!("{}:{}", l.file(), l.line()))
            .unwrap_or_else(|| "unknown".to_string());
        
        eprintln!("\n╔════════════════════════════════════════════╗");
        eprintln!("║              CRASH DETECTED!               ║");
        eprintln!("╠════════════════════════════════════════════╣");
        eprintln!("║ Location: {}", location);
        eprintln!("║ Error: {}", msg);
        eprintln!("╚════════════════════════════════════════════╝");
        
        // Also write to log file
        if let Ok(mut file) = std::fs::OpenOptions::new()
            .create(true)
            .append(true)
            .open("externa.log") 
        {
            use std::io::Write;
            let _ = writeln!(file, "\n[CRASH] {} at {}", msg, location);
        }
        
        // Wait for user to see the message
        eprintln!("\nPress Enter to exit...");
        let _ = std::io::stdin().read_line(&mut String::new());
    }));
    
    // Initialize logging (console + externa.log file)
    init_logging();
    
    info!("╔════════════════════════════════════════════╗");
    info!("║      Externa Rust V2.4.1 - CS2 ESP         ║");
    info!("║  [INSERT] Toggle ESP | [END] Exit          ║");
    info!("║  Log file: externa.log                     ║");
    info!("╚════════════════════════════════════════════╝");

    // Initialize syscalls
    memory::syscall::init();
    
    // Auto-load driver (embedded or external)
    // Falls back to syscall if driver loading fails
    let _driver_loaded = memory::embedded_driver::auto_load_driver();
    info!("[Mode] {}", memory::embedded_driver::get_mode());
    
    // Wait for CS2 and attach first (needed for pattern scanner)
    let mem = loop {
        match attach_to_cs2() {
            Ok(mem) => break Arc::new(mem),
            Err(e) => {
                info!("Waiting for CS2... ({})", e);
                thread::sleep(Duration::from_secs(2));
            }
        }
    };
    
    info!("Attached to CS2! PID: {}, Client: 0x{:X}", mem._pid, mem.client_base);
    
    // Fetch offsets with pattern scanner (priority) or API fallback
    let offsets = Arc::new(Offsets::fetch_with_scan(mem._pid));
    
    // Verify offsets
    verify_offsets(&mem, &offsets);
    
    // Shared state
    let state = Arc::new(Mutex::new(GameState {
        entities: Vec::with_capacity(64),
        local_team: 0,
    }));
    
    // ESP Config
    let config = Arc::new(EspConfig::default());

    // Spawn memory reading thread
    spawn_memory_thread(mem.clone(), state.clone(), offsets.clone());
    
    // Spawn input handling thread
    spawn_input_thread(config.clone());

    // Main overlay loop (pass mem and offsets for direct view matrix reading)
    match run_overlay_loop(state, config, mem, offsets) {
        Ok(_) => info!("Overlay loop finished normally"),
        Err(e) => error!("Overlay loop error: {:?}", e),
    }

    info!("Exiting... Press Enter to close.");
    let _ = std::io::stdin().read_line(&mut String::new());
    Ok(())
}

// ============================================================================
// CS2 Attachment
// ============================================================================

fn attach_to_cs2() -> Result<memory::Memory> {
    // Try kernel driver first (much faster!)
    if let Some(mut driver) = memory::driver::DriverReader::connect() {
        info!("[Driver] Kernel driver detected - using ultra-fast mode!");
        
        // Get PID and module base via driver
        let pid = get_pid("cs2.exe")?;
        
        if driver.attach(pid) {
            if let Some(client_base) = driver.get_module_base(pid, "client.dll") {
                info!("[Driver] client.dll base: 0x{:X}", client_base);
                
                // Still need a process handle for fallback
                let handle = open_process(pid)?;
                
                return Ok(memory::Memory {
                    handle: Arc::new(memory::handle::SendHandle(handle)),
                    _pid: pid,
                    client_base,
                    driver: Some(Arc::new(parking_lot::Mutex::new(driver))),
                });
            }
        }
        warn!("[Driver] Failed to attach via driver, falling back to usermode");
    }
    
    // Fallback to standard usermode approach
    info!("[Memory] Using usermode memory reading (syscall + ReadProcessMemory)");
    let pid = get_pid("cs2.exe")?;
    let handle = open_process(pid)?;
    let client_base = get_module_base(pid, "client.dll")?;
    
    Ok(memory::Memory {
        handle: Arc::new(memory::handle::SendHandle(handle)),
        _pid: pid,
        client_base,
        driver: None,
    })
}

fn verify_offsets(mem: &memory::Memory, offsets: &Offsets) {
    let test_ent_list: usize = mem.read(mem.client_base + offsets.dw_entity_list).unwrap_or(0);
    let test_local: usize = mem.read(mem.client_base + offsets.dw_local_player_controller).unwrap_or(0);
    
    info!("Offset verification:");
    info!("  EntityList ptr: 0x{:X} (valid: {})", test_ent_list, test_ent_list > 0x10000000000);
    info!("  LocalCtrl ptr: 0x{:X} (valid: {})", test_local, test_local > 0x10000000000);
    
    if test_ent_list < 0x10000000000 {
        error!("EntityList pointer invalid! Offsets may be outdated.");
        error!("Try restarting to fetch fresh offsets from cs2-dumper.");
    }
}

// ============================================================================
// Memory Reading Thread
// ============================================================================

fn spawn_memory_thread(
    mem: Arc<memory::Memory>,
    state: Arc<Mutex<GameState>>,
    offsets: Arc<Offsets>,
) {
    thread::spawn(move || {
        info!("Memory thread started");
        let mut last_log = std::time::Instant::now();
        
        loop {
            {
                let mut st = state.lock();
                
                // NOTE: View matrix is now read in render loop for better sync!
                
                // Read local player team
                st.local_team = read_local_team(&mem, &offsets);
                
                // Read all entities
                st.entities.clear();
                read_entities(&mem, &offsets, &mut st.entities);
                
                // Periodic logging
                if last_log.elapsed().as_secs() >= 5 {
                    last_log = std::time::Instant::now();
                    info!("Entities: {} | LocalTeam: {}", st.entities.len(), st.local_team);
                }
            }
            
            // Slightly faster update for smoother entity tracking
            thread::sleep(Duration::from_millis(2));
        }
    });
}

fn read_local_team(mem: &memory::Memory, offsets: &Offsets) -> i32 {
    const STRIDE: usize = 112; // 0x70 - original working value
    
    let local_ctrl: usize = mem.read(mem.client_base + offsets.dw_local_player_controller).unwrap_or(0);
    if local_ctrl == 0 || local_ctrl > 0x7FF000000000 {
        return 0;
    }
    
    let pawn_h: u32 = mem.read(local_ctrl + netvars::M_H_PLAYER_PAWN).unwrap_or(0);
    if pawn_h == 0 || pawn_h == 0xFFFFFFFF {
        return 0;
    }
    
    let ent_list: usize = mem.read(mem.client_base + offsets.dw_entity_list).unwrap_or(0);
    if ent_list == 0 {
        return 0;
    }
    
    let chunk_idx = (pawn_h as usize & 0x7FFF) >> 9;
    let entry_idx = pawn_h as usize & 0x1FF;
    let entry: usize = mem.read(ent_list + 8 * chunk_idx + 0x10).unwrap_or(0);
    
    if entry == 0 {
        return 0;
    }
    
    let pawn: usize = mem.read(entry + entry_idx * STRIDE).unwrap_or(0);
    if pawn == 0 || pawn > 0x7FF000000000 {
        return 0;
    }
    
    mem.read(pawn + netvars::M_I_TEAM_NUM).unwrap_or(0)
}

// Debug counter for first few iterations
static DEBUG_COUNTER: std::sync::atomic::AtomicU32 = std::sync::atomic::AtomicU32::new(0);

fn read_entities(mem: &memory::Memory, offsets: &Offsets, entities: &mut Vec<Entity>) {
    const STRIDE: usize = 112; // 0x70 - original working value
    
    let ent_list: usize = mem.read(mem.client_base + offsets.dw_entity_list).unwrap_or(0);
    if ent_list == 0 {
        return;
    }
    
    // Debug logging (first 3 iterations only)
    let debug_count = DEBUG_COUNTER.fetch_add(1, Ordering::Relaxed);
    let should_debug = debug_count < 3;
    
    let mut stats = (0u32, 0u32, 0u32, 0u32, 0u32, 0u32); // list_entry, controller, pawn_h, pawn, health, added
    
    // Iterate player slots (1-64)
    for i in 1..=64usize {
        let chunk_idx = (i & 0x7FFF) >> 9;
        let list_entry: usize = mem.read(ent_list + 8 * chunk_idx + 0x10).unwrap_or(0);
        if list_entry == 0 { continue; }
        stats.0 += 1;
        
        let entry_idx = i & 0x1FF;
        let controller: usize = mem.read(list_entry + entry_idx * STRIDE).unwrap_or(0);
        if controller == 0 || controller > 0x7FF000000000 { continue; }
        stats.1 += 1;
        
        let pawn_h: u32 = mem.read(controller + netvars::M_H_PLAYER_PAWN).unwrap_or(0);
        if pawn_h == 0 || pawn_h == 0xFFFFFFFF { continue; }
        stats.2 += 1;
        
        // Get pawn from entity list
        let pawn_chunk_idx = (pawn_h as usize & 0x7FFF) >> 9;
        let pawn_entry_idx = pawn_h as usize & 0x1FF;
        let list_entry2: usize = mem.read(ent_list + 8 * pawn_chunk_idx + 0x10).unwrap_or(0);
        if list_entry2 == 0 { continue; }
        
        let pawn: usize = mem.read(list_entry2 + pawn_entry_idx * STRIDE).unwrap_or(0);
        if pawn == 0 || pawn > 0x7FF000000000 { continue; }
        stats.3 += 1;
        
        // Health check
        let health: i32 = mem.read(pawn + netvars::M_I_HEALTH).unwrap_or(0);
        if health <= 0 || health > 100 { continue; }
        stats.4 += 1;
        
        let team: i32 = mem.read(pawn + netvars::M_I_TEAM_NUM).unwrap_or(0);
        
        // Get GameSceneNode for accurate position
        let game_scene_node: usize = mem.read(pawn + netvars::M_P_GAME_SCENE_NODE).unwrap_or(0);
        if game_scene_node == 0 || game_scene_node > 0x7FF000000000 { continue; }
        
        // Use m_vecAbsOrigin from GameSceneNode (actual render position)
        let pos: Vec3 = mem.read(game_scene_node + 0xD0).unwrap_or(Vec3::ZERO); // m_vecAbsOrigin
        if pos == Vec3::ZERO { continue; }
        stats.5 += 1;
        
        // Cache bone_array pointer (used for fast render-time reads)
        let model_state = game_scene_node + netvars::M_MODEL_STATE;
        let bone_array: usize = mem.read(model_state + netvars::M_BONE_ARRAY).unwrap_or(0);
        
        // Read bones (pass game_scene_node to avoid double-read)
        let bones = read_bones_from_node(mem, game_scene_node);
        
        entities.push(Entity {
            pawn,
            controller,
            pos,
            health,
            team,
            bones,
            bone_array,
        });
    }
    
    // Debug logging
    if should_debug {
        info!("[DEBUG] Entity scan: list_entry={} ctrl={} pawn_h={} pawn={} health_ok={} pos_ok={}",
              stats.0, stats.1, stats.2, stats.3, stats.4, stats.5);
    }
}

/// Read bones from GameSceneNode (avoids double-reading the node)
fn read_bones_from_node(mem: &memory::Memory, game_scene_node: usize) -> Bones {
    let mut bones = Bones::default();
    
    // Get bone array pointer (modelState + boneArray offset)
    let model_state = game_scene_node + netvars::M_MODEL_STATE;
    let bone_array: usize = mem.read(model_state + netvars::M_BONE_ARRAY).unwrap_or(0);
    
    if bone_array == 0 || bone_array > 0x7FF000000000 {
        return bones;
    }
    
    // Read bone positions (each bone is a 4x4 matrix, we only need xyz position)
    // Bone matrix is 48 bytes (12 floats), position is at offset 0, 16, 32 (x, y, z at the 4th element of each row)
    // Actually in CS2, bones are stored as vec3 + padding = 16 bytes per bone
    const BONE_SIZE: usize = 32; // sizeof(BoneData) in CS2
    
    let read_bone = |index: usize| -> Vec3 {
        let offset = bone_array + index * BONE_SIZE;
        mem.read::<Vec3>(offset).unwrap_or(Vec3::ZERO)
    };
    
    bones.head = read_bone(netvars::BONE_HEAD);
    bones.neck = read_bone(netvars::BONE_NECK);
    bones.spine_1 = read_bone(netvars::BONE_SPINE_1);
    bones.spine_2 = read_bone(netvars::BONE_SPINE_2);
    bones.pelvis = read_bone(netvars::BONE_PELVIS);
    bones.left_shoulder = read_bone(netvars::BONE_LEFT_SHOULDER);
    bones.left_elbow = read_bone(netvars::BONE_LEFT_ELBOW);
    bones.left_hand = read_bone(netvars::BONE_LEFT_HAND);
    bones.right_shoulder = read_bone(netvars::BONE_RIGHT_SHOULDER);
    bones.right_elbow = read_bone(netvars::BONE_RIGHT_ELBOW);
    bones.right_hand = read_bone(netvars::BONE_RIGHT_HAND);
    bones.left_hip = read_bone(netvars::BONE_LEFT_HIP);
    bones.left_knee = read_bone(netvars::BONE_LEFT_KNEE);
    bones.left_foot = read_bone(netvars::BONE_LEFT_FOOT);
    bones.right_hip = read_bone(netvars::BONE_RIGHT_HIP);
    bones.right_knee = read_bone(netvars::BONE_RIGHT_KNEE);
    bones.right_foot = read_bone(netvars::BONE_RIGHT_FOOT);
    
    bones
}

// ============================================================================
// Input Handling Thread
// ============================================================================

fn spawn_input_thread(config: Arc<EspConfig>) {
    thread::spawn(move || {
        info!("Input thread started");
        let mut insert_was_pressed = false;
        let mut end_was_pressed = false;
        
        loop {
            unsafe {
                // INSERT - Toggle ESP
                let insert_pressed = GetAsyncKeyState(0x2D) & 0x8000u16 as i16 != 0;
                if insert_pressed && !insert_was_pressed {
                    let new_state = !config.enabled.load(Ordering::Relaxed);
                    config.enabled.store(new_state, Ordering::Relaxed);
                    info!("ESP: {}", if new_state { "ON" } else { "OFF" });
                }
                insert_was_pressed = insert_pressed;
                
                // END - Exit
                let end_pressed = GetAsyncKeyState(0x23) & 0x8000u16 as i16 != 0;
                if end_pressed && !end_was_pressed {
                    info!("Exit hotkey pressed");
                    std::process::exit(0);
                }
                end_was_pressed = end_pressed;
            }
            
            thread::sleep(Duration::from_millis(50));
        }
    });
}

// ============================================================================
// Overlay Loop
// ============================================================================

fn run_overlay_loop(
    state: Arc<Mutex<GameState>>,
    config: Arc<EspConfig>,
    mem: Arc<memory::Memory>,
    offsets: Arc<Offsets>,
) -> Result<()> {
    let overlay = overlay::renderer::Direct2DOverlay::new()?;
    info!("Overlay initialized: {}x{} | Class: {}", 
          overlay.width, overlay.height, overlay.class_name());
    
    let mut frame_count = 0u64;
    let mut last_fps_time = std::time::Instant::now();
    
    info!("[Render] Starting render loop...");
    
    // No artificial delay - render as fast as possible!
    // D2D Present will sync with display naturally
    
    loop {
        let _frame_start = std::time::Instant::now();
        
        // Log first few frames for debugging
        if frame_count < 3 {
            info!("[Render] Frame {} starting", frame_count);
        }
        
        // Handle Windows messages
        if !overlay.handle_message() {
            info!("[Render] WM_QUIT received, exiting loop");
            break;
        }
        
        // Begin drawing
        if !overlay.begin_scene() {
            warn!("Failed to begin scene");
            continue;
        }
        
        if frame_count < 3 {
            info!("[Render] Frame {} - scene started", frame_count);
        }
        
        // Only draw if ESP enabled
        if config.enabled.load(Ordering::Relaxed) {
            // 1. Read ViewMatrix FIRST (like DragonBurn UpdateGameState)
            let view_matrix: [[f32; 4]; 4] = mem.read(mem.client_base + offsets.dw_view_matrix)
                .unwrap_or([[0.0; 4]; 4]);
            
            // 2. Get cached data from memory thread (team, health, bone_array rarely change)
            let (local_team, entity_pawns): (i32, Vec<(usize, i32, i32, usize)>) = {
                let st = state.lock();
                let pawns: Vec<(usize, i32, i32, usize)> = st.entities.iter()
                    .filter(|e| e.health > 0)
                    .map(|e| (e.pawn, e.team, e.health, e.bone_array))
                    .collect();
                (st.local_team, pawns)
            };
            
            // 3. For each entity, read ALL bones fresh, then draw IMMEDIATELY
            // This matches DragonBurn's pattern: read -> W2S -> draw in tight sequence
            for (_pawn, team, cached_health, cached_bone_array) in entity_pawns {
                // Skip teammates
                if local_team != 0 && team == local_team {
                    continue;
                }
                
                if cached_bone_array == 0 {
                    continue;
                }
                
                // Read ALL bones in ONE batch (DragonBurn reads 30 bones × 32 bytes)
                // We read only the 17 bones we need
                let bones = read_bones_fresh(&mem, cached_bone_array);
                
                if !bones.is_valid() {
                    continue;
                }
                
                // Draw immediately with fresh data
                draw_entity_complete(
                    &overlay, 
                    &bones, 
                    cached_health, 
                    &view_matrix, 
                    &config
                );
            }
        }
        
        // End drawing
        if !overlay.end_scene() {
            error!("D2D device lost, attempting recovery...");
        }
        
        if frame_count < 3 {
            info!("[Render] Frame {} completed", frame_count);
        }
        
        // FPS counter
        frame_count += 1;
        if last_fps_time.elapsed().as_secs() >= 10 {
            let fps = frame_count as f64 / last_fps_time.elapsed().as_secs_f64();
            info!("Overlay FPS: {:.0}", fps);
            frame_count = 0;
            last_fps_time = std::time::Instant::now();
        }
        
        // Minimal sleep to prevent 100% CPU usage
        // D2D Present naturally syncs with compositor
        thread::sleep(Duration::from_micros(100));
    }
    
    Ok(())
}

/// Read ALL bones in ONE memory read (DragonBurn optimization)
/// Reads 30 bones × 32 bytes = 960 bytes in single ReadProcessMemory call
fn read_bones_fresh(mem: &memory::Memory, bone_array: usize) -> Bones {
    const BONE_SIZE: usize = 32;
    const NUM_BONES: usize = 30;  // CS2 has 30 bones
    const TOTAL_SIZE: usize = NUM_BONES * BONE_SIZE;  // 960 bytes
    
    // Read ALL bone data in ONE call (massive speedup!)
    let mut buffer = [0u8; TOTAL_SIZE];
    if !mem.read_raw(bone_array, &mut buffer) {
        return Bones::default();
    }
    
    // Extract Vec3 from buffer at given bone index
    let extract_bone = |index: usize| -> Vec3 {
        let offset = index * BONE_SIZE;
        if offset + 12 > TOTAL_SIZE {
            return Vec3::ZERO;
        }
        // Vec3 is first 12 bytes of each 32-byte bone struct
        let x = f32::from_le_bytes([buffer[offset], buffer[offset+1], buffer[offset+2], buffer[offset+3]]);
        let y = f32::from_le_bytes([buffer[offset+4], buffer[offset+5], buffer[offset+6], buffer[offset+7]]);
        let z = f32::from_le_bytes([buffer[offset+8], buffer[offset+9], buffer[offset+10], buffer[offset+11]]);
        Vec3::new(x, y, z)
    };
    
    Bones {
        head: extract_bone(netvars::BONE_HEAD),
        neck: extract_bone(netvars::BONE_NECK),
        spine_1: extract_bone(netvars::BONE_SPINE_1),
        spine_2: extract_bone(netvars::BONE_SPINE_2),
        pelvis: extract_bone(netvars::BONE_PELVIS),
        left_shoulder: extract_bone(netvars::BONE_LEFT_SHOULDER),
        left_elbow: extract_bone(netvars::BONE_LEFT_ELBOW),
        left_hand: extract_bone(netvars::BONE_LEFT_HAND),
        right_shoulder: extract_bone(netvars::BONE_RIGHT_SHOULDER),
        right_elbow: extract_bone(netvars::BONE_RIGHT_ELBOW),
        right_hand: extract_bone(netvars::BONE_RIGHT_HAND),
        left_hip: extract_bone(netvars::BONE_LEFT_HIP),
        left_knee: extract_bone(netvars::BONE_LEFT_KNEE),
        left_foot: extract_bone(netvars::BONE_LEFT_FOOT),
        right_hip: extract_bone(netvars::BONE_RIGHT_HIP),
        right_knee: extract_bone(netvars::BONE_RIGHT_KNEE),
        right_foot: extract_bone(netvars::BONE_RIGHT_FOOT),
    }
}

/// Draw entity with complete ESP (box, skeleton, health, snaplines)
/// Uses fresh bone data read at render time for perfect sync
fn draw_entity_complete(
    overlay: &overlay::renderer::Direct2DOverlay,
    bones: &Bones,
    health: i32,
    matrix: &[[f32; 4]; 4],
    config: &EspConfig,
) {
    let screen_w = overlay.width as f32;
    let screen_h = overlay.height as f32;
    
    // Use left_foot for feet position (from bone array - same source as head)
    let feet_pos = bones.left_foot;
    let head_pos = bones.head;
    
    // W2S transform
    let (s_feet, s_head) = match (
        game::math::w2s(matrix, feet_pos, screen_w, screen_h),
        game::math::w2s(matrix, head_pos, screen_w, screen_h)
    ) {
        (Some(f), Some(h)) => (f, h),
        _ => return,
    };
    
    // Box calculation (DragonBurn style)
    let box_h = s_feet.y - s_head.y;
    let box_w = box_h * 0.4;
    let box_x = s_head.x - box_w / 2.0;
    let box_y = s_head.y;
    
    // Sanity check
    if box_h < 5.0 || box_h > 800.0 {
        return;
    }
    
    // Draw skeleton FIRST (behind box)
    if config.show_skeleton.load(Ordering::Relaxed) {
        for (from, to) in bones.get_skeleton_lines() {
            if let (Some(s1), Some(s2)) = (
                game::math::w2s(matrix, from, screen_w, screen_h),
                game::math::w2s(matrix, to, screen_w, screen_h)
            ) {
                overlay.draw_line(s1.x, s1.y, s2.x, s2.y, true);
            }
        }
    }
    
    // Draw box ESP
    if config.show_box.load(Ordering::Relaxed) {
        overlay.draw_box(box_x, box_y, box_w, box_h, true);
    }
    
    // Draw health bar
    if config.show_health.load(Ordering::Relaxed) {
        overlay.draw_health_bar(box_x, box_y, box_h, health);
    }
    
    // Draw snaplines
    if config.show_snaplines.load(Ordering::Relaxed) {
        overlay.draw_snapline(s_feet.x, s_feet.y);
    }
}

/// Draw entity ESP (box, skeleton, health, snaplines) - uses cached Entity
#[allow(dead_code)]
fn draw_entity(
    overlay: &overlay::renderer::Direct2DOverlay,
    ent: &Entity,
    matrix: &[[f32; 4]; 4],
    config: &EspConfig,
) {
    let screen_w = overlay.width as f32;
    let screen_h = overlay.height as f32;
    
    // Calculate box from feet/head positions
    let feet_pos = ent.pos;
    let head_pos = if ent.bones.is_valid() {
        ent.bones.head
    } else {
        Vec3::new(ent.pos.x, ent.pos.y, ent.pos.z + 72.0)
    };
    
    let (s_feet, s_head) = match (
        game::math::w2s(matrix, feet_pos, screen_w, screen_h),
        game::math::w2s(matrix, head_pos, screen_w, screen_h)
    ) {
        (Some(f), Some(h)) => (f, h),
        _ => return,
    };
    
    let box_h = s_feet.y - s_head.y;
    let box_w = box_h * 0.4;
    let box_x = s_head.x - box_w / 2.0;
    let box_y = s_head.y;
    
    // Sanity check
    if box_h < 5.0 || box_h > 800.0 {
        return;
    }
    
    // Draw box ESP
    if config.show_box.load(Ordering::Relaxed) {
        overlay.draw_box(box_x, box_y, box_w, box_h, true);
    }
    
    // Draw health bar
    if config.show_health.load(Ordering::Relaxed) {
        overlay.draw_health_bar(box_x, box_y, box_h, ent.health);
    }
    
    // Draw snaplines
    if config.show_snaplines.load(Ordering::Relaxed) {
        overlay.draw_snapline(s_feet.x, s_feet.y);
    }
    
    // Draw skeleton
    if config.show_skeleton.load(Ordering::Relaxed) && ent.bones.is_valid() {
        for (from, to) in ent.bones.get_skeleton_lines() {
            if let (Some(s1), Some(s2)) = (
                game::math::w2s(matrix, from, screen_w, screen_h),
                game::math::w2s(matrix, to, screen_w, screen_h)
            ) {
                overlay.draw_line(s1.x, s1.y, s2.x, s2.y, true);
            }
        }
    }
}

// ============================================================================
// Windows Helpers
// ============================================================================

fn get_pid(name: &str) -> Result<u32> {
    use windows::Win32::System::Diagnostics::ToolHelp::*;
    unsafe {
        let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)?;
        let mut entry = PROCESSENTRY32W { 
            dwSize: std::mem::size_of::<PROCESSENTRY32W>() as u32, 
            ..Default::default() 
        };
        
        if Process32FirstW(snapshot, &mut entry).is_ok() {
            loop {
                let proc_name = String::from_utf16_lossy(
                    &entry.szExeFile[..entry.szExeFile.iter().position(|&c| c == 0).unwrap_or(0)]
                );
                if proc_name.eq_ignore_ascii_case(name) {
                    let _ = windows::Win32::Foundation::CloseHandle(snapshot);
                    return Ok(entry.th32ProcessID);
                }
                if Process32NextW(snapshot, &mut entry).is_err() { break; }
            }
        }
        let _ = windows::Win32::Foundation::CloseHandle(snapshot);
        Err(anyhow::anyhow!("Process '{}' not found", name))
    }
}

fn open_process(pid: u32) -> Result<windows::Win32::Foundation::HANDLE> {
    use windows::Win32::System::Threading::*;
    unsafe { 
        Ok(OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, false, pid)?) 
    }
}

fn get_module_base(pid: u32, name: &str) -> Result<usize> {
    use windows::Win32::System::Diagnostics::ToolHelp::*;
    unsafe {
        let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)?;
        let mut entry = MODULEENTRY32W { 
            dwSize: std::mem::size_of::<MODULEENTRY32W>() as u32, 
            ..Default::default() 
        };
        
        if Module32FirstW(snapshot, &mut entry).is_ok() {
            loop {
                let mod_name = String::from_utf16_lossy(
                    &entry.szModule[..entry.szModule.iter().position(|&c| c == 0).unwrap_or(0)]
                );
                if mod_name.eq_ignore_ascii_case(name) {
                    let _ = windows::Win32::Foundation::CloseHandle(snapshot);
                    return Ok(entry.modBaseAddr as usize);
                }
                if Module32NextW(snapshot, &mut entry).is_err() { break; }
            }
        }
        let _ = windows::Win32::Foundation::CloseHandle(snapshot);
        Err(anyhow::anyhow!("Module '{}' not found", name))
    }
}
