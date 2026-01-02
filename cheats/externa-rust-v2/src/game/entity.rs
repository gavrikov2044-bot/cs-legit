use glam::Vec3;

#[derive(Clone, Copy, Debug)]
pub struct Entity {
    pub pawn: usize,
    pub controller: usize,
    pub pos: Vec3,
    pub health: i32,
    pub team: i32,
    pub bones: [Vec3; 30], // Store key bones
}

impl Entity {
    pub const HEAD_OFFSET: Vec3 = Vec3::new(0.0, 0.0, 72.0);
}

