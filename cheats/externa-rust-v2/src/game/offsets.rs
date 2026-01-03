// Static offsets from dump (2026-01-02 18:37 UTC)
pub const DW_ENTITY_LIST: usize = 0x1D13CE8;
pub const DW_LOCAL_PLAYER_CONTROLLER: usize = 0x1E1DC18;
pub const DW_VIEW_MATRIX: usize = 0x1E323D0;

// Dynamic offsets struct (kept for compatibility)
pub struct Offsets {
    pub dw_entity_list: usize,
    pub dw_local_player_controller: usize,
    pub dw_view_matrix: usize,
}

pub mod netvars {
    pub const M_I_HEALTH: usize = 0x34C;
    pub const M_I_TEAM_NUM: usize = 0x3EB;
    pub const M_P_GAME_SCENE_NODE: usize = 0x330;
    pub const M_VEC_ABS_ORIGIN: usize = 0xD0;
    pub const M_H_PLAYER_PAWN: usize = 0x8FC;
    // pub const M_MODEL_STATE: usize = 0x190; // For bones (not used yet)
    // pub const M_BONE_ARRAY: usize = 0x80;
}
