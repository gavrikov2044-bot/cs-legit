//! CS2 Memory Offsets
//! Update these when game updates

/// client.dll offsets
pub mod client {
    pub const DW_ENTITY_LIST: usize = 0x1A6A8D0;
    pub const DW_LOCAL_PLAYER_CONTROLLER: usize = 0x1A4F178;
    pub const DW_VIEW_MATRIX: usize = 0x1A8F690;
}

/// C_BaseEntity
pub mod entity {
    pub const M_I_HEALTH: usize = 0x344;
    pub const M_I_TEAM_NUM: usize = 0x3E3;
    pub const M_P_GAME_SCENE_NODE: usize = 0x328;
    pub const M_VEC_ABS_ORIGIN: usize = 0xD0;
}

/// CCSPlayerController
pub mod controller {
    pub const M_H_PLAYER_PAWN: usize = 0x80C;
}

/// C_CSPlayerPawn
pub mod pawn {
    pub const M_ARMOR_VALUE: usize = 0x354;
}

