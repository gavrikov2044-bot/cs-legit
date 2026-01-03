//! CS2 Offsets - Multiple fallback methods
//! Priority: 1) Pattern Scanner  2) cs2-dumper API  3) Hardcoded fallback

use serde::Deserialize;

// ============================================================================
// Hardcoded Fallback Offsets (cs2-dumper 2026-01-03)
// ============================================================================

pub const DW_ENTITY_LIST: usize = 0x1A146C8;              // cs2-dumper
pub const DW_LOCAL_PLAYER_CONTROLLER: usize = 0x1A6ED90;  // cs2-dumper
pub const DW_VIEW_MATRIX: usize = 0x1A84490;              // cs2-dumper

// ============================================================================
// Offsets Struct
// ============================================================================

#[derive(Clone, Debug)]
pub struct Offsets {
    pub dw_entity_list: usize,
    pub dw_local_player_controller: usize,
    pub dw_view_matrix: usize,
}

impl Default for Offsets {
    fn default() -> Self {
        Self {
            dw_entity_list: DW_ENTITY_LIST,
            dw_local_player_controller: DW_LOCAL_PLAYER_CONTROLLER,
            dw_view_matrix: DW_VIEW_MATRIX,
        }
    }
}

impl Offsets {
    /// Get offsets using multiple methods with fallbacks
    /// Priority: Pattern Scanner -> cs2-dumper API -> Hardcoded
    #[allow(dead_code)]
    pub fn fetch() -> Self {
        log::info!("[Offsets] Fetching offsets...");
        
        // Method 1: cs2-dumper API (fast, reliable when available)
        match fetch_from_api() {
            Ok(offsets) => {
                log::info!("[Offsets] Loaded from cs2-dumper API");
                return offsets;
            }
            Err(e) => {
                log::warn!("[Offsets] API fetch failed: {}", e);
            }
        }
        
        // Method 2: Hardcoded fallback
        log::warn!("[Offsets] Using HARDCODED fallback offsets!");
        log::warn!("[Offsets] These may be outdated - consider updating manually");
        Self::default()
    }
    
    /// Get offsets - prioritize API, fallback to hardcoded
    /// Scanner disabled - gives wrong results
    pub fn fetch_with_scan(_pid: u32) -> Self {
        log::info!("[Offsets] Fetching offsets...");
        
        // Try cs2-dumper API first
        match fetch_from_api() {
            Ok(offsets) => {
                log::info!("[Offsets] Loaded from cs2-dumper API:");
                log::info!("[Offsets]   dwEntityList: 0x{:X}", offsets.dw_entity_list);
                log::info!("[Offsets]   dwLocalPlayerController: 0x{:X}", offsets.dw_local_player_controller);
                log::info!("[Offsets]   dwViewMatrix: 0x{:X}", offsets.dw_view_matrix);
                return offsets;
            }
            Err(e) => {
                log::warn!("[Offsets] API failed: {}", e);
            }
        }
        
        // Fallback to hardcoded
        let result = Self::default();
        log::warn!("[Offsets] Using HARDCODED fallback:");
        log::info!("[Offsets]   dwEntityList: 0x{:X}", result.dw_entity_list);
        log::info!("[Offsets]   dwLocalPlayerController: 0x{:X}", result.dw_local_player_controller);
        log::info!("[Offsets]   dwViewMatrix: 0x{:X}", result.dw_view_matrix);
        
        result
    }
}

// ============================================================================
// cs2-dumper API Fetch
// ============================================================================

#[allow(dead_code)]
#[derive(Deserialize, Debug)]
struct DumperClientDll {
    #[serde(rename = "dwEntityList")]
    dw_entity_list: Option<usize>,
    #[serde(rename = "dwLocalPlayerController")]
    dw_local_player_controller: Option<usize>,
    #[serde(rename = "dwViewMatrix")]
    dw_view_matrix: Option<usize>,
}

fn fetch_from_api() -> anyhow::Result<Offsets> {
    const URL: &str = "https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/offsets.json";
    
    log::info!("[Offsets] Fetching from cs2-dumper...");
    
    let response = ureq::get(URL)
        .timeout(std::time::Duration::from_secs(10))
        .call()?;
    
    let body = response.into_string()?;
    let json: serde_json::Value = serde_json::from_str(&body)?;
    
    let client = json.get("client.dll")
        .ok_or_else(|| anyhow::anyhow!("client.dll not found"))?;
    
    let entity_list = client.get("dwEntityList")
        .and_then(|v: &serde_json::Value| v.as_u64())
        .ok_or_else(|| anyhow::anyhow!("dwEntityList not found"))? as usize;
    
    let local_player = client.get("dwLocalPlayerController")
        .and_then(|v: &serde_json::Value| v.as_u64())
        .ok_or_else(|| anyhow::anyhow!("dwLocalPlayerController not found"))? as usize;
    
    let view_matrix = client.get("dwViewMatrix")
        .and_then(|v: &serde_json::Value| v.as_u64())
        .ok_or_else(|| anyhow::anyhow!("dwViewMatrix not found"))? as usize;
    
    log::info!("[Offsets] dwEntityList: 0x{:X}", entity_list);
    log::info!("[Offsets] dwLocalPlayerController: 0x{:X}", local_player);
    log::info!("[Offsets] dwViewMatrix: 0x{:X}", view_matrix);
    
    Ok(Offsets {
        dw_entity_list: entity_list,
        dw_local_player_controller: local_player,
        dw_view_matrix: view_matrix,
    })
}

// ============================================================================
// Netvars (change less frequently)
// ============================================================================

pub mod netvars {
    // C_BaseEntity - ORIGINAL WORKING VALUES
    pub const M_I_HEALTH: usize = 0x34C;           // m_iHealth
    pub const M_I_TEAM_NUM: usize = 0x3EB;         // m_iTeamNum
    pub const M_P_GAME_SCENE_NODE: usize = 0x330;  // m_pGameSceneNode
    
    // C_BasePlayerPawn  
    #[allow(dead_code)]
    pub const M_V_OLD_ORIGIN: usize = 0x15A0;      // m_vOldOrigin (legacy, use bones instead)
    
    // CCSPlayerController
    pub const M_H_PLAYER_PAWN: usize = 0x8FC;      // m_hPlayerPawn
    
    // CGameSceneNode
    #[allow(dead_code)]
    pub const M_VEC_ABS_ORIGIN: usize = 0xD0;      // m_vecAbsOrigin
    
    // CSkeletonInstance / Bones
    pub const M_MODEL_STATE: usize = 0x190;        // m_modelState (original)
    pub const M_BONE_ARRAY: usize = 0x80;          // Bone array offset
    
    // Bone indices (CS2)
    pub const BONE_HEAD: usize = 6;
    pub const BONE_NECK: usize = 5;
    pub const BONE_SPINE_1: usize = 4;
    pub const BONE_SPINE_2: usize = 2;
    pub const BONE_PELVIS: usize = 0;
    pub const BONE_LEFT_SHOULDER: usize = 8;
    pub const BONE_LEFT_ELBOW: usize = 9;
    pub const BONE_LEFT_HAND: usize = 10;
    pub const BONE_RIGHT_SHOULDER: usize = 13;
    pub const BONE_RIGHT_ELBOW: usize = 14;
    pub const BONE_RIGHT_HAND: usize = 15;
    pub const BONE_LEFT_HIP: usize = 22;
    pub const BONE_LEFT_KNEE: usize = 23;
    pub const BONE_LEFT_FOOT: usize = 24;
    pub const BONE_RIGHT_HIP: usize = 25;
    pub const BONE_RIGHT_KNEE: usize = 26;
    pub const BONE_RIGHT_FOOT: usize = 27;
}
