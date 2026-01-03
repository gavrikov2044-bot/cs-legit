use glam::Vec3;

#[derive(Clone, Copy, Debug)]
pub struct Entity {
    pub _pawn: usize,
    pub _controller: usize,
    pub pos: Vec3,
    pub _health: i32,
    pub team: i32,
    pub _bones: [Vec3; 30], // Store key bones
}

impl Entity {
    // pub const HEAD_OFFSET: Vec3 = Vec3::new(0.0, 0.0, 72.0); // Unused for now
}

