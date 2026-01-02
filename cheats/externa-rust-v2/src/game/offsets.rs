// Dynamic offsets resolved by scanner
pub struct Offsets {
    pub dw_entity_list: usize,
    pub dw_local_player_controller: usize,
    pub dw_view_matrix: usize,
}

// Static offsets (netvars)
pub mod netvars {
    pub const M_I_HEALTH: usize = 0x344;
    pub const M_I_TEAM_NUM: usize = 0x3E3;
    pub const M_P_GAME_SCENE_NODE: usize = 0x328;
    pub const M_VEC_ABS_ORIGIN: usize = 0xD0;
    pub const M_H_PLAYER_PAWN: usize = 0x80C;
    pub const M_MODEL_STATE: usize = 0x190;
    pub const M_BONE_ARRAY: usize = 0x80;
}

