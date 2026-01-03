use glam::Vec3;

/// Key bone indices for skeleton ESP
#[derive(Clone, Copy, Debug, Default)]
pub struct Bones {
    pub head: Vec3,
    pub neck: Vec3,
    pub spine_1: Vec3,
    pub spine_2: Vec3,
    pub pelvis: Vec3,
    pub left_shoulder: Vec3,
    pub left_elbow: Vec3,
    pub left_hand: Vec3,
    pub right_shoulder: Vec3,
    pub right_elbow: Vec3,
    pub right_hand: Vec3,
    pub left_hip: Vec3,
    pub left_knee: Vec3,
    pub left_foot: Vec3,
    pub right_hip: Vec3,
    pub right_knee: Vec3,
    pub right_foot: Vec3,
}

impl Bones {
    /// Check if bones are valid (at least head is non-zero)
    pub fn is_valid(&self) -> bool {
        self.head != Vec3::ZERO
    }
    
    /// Get skeleton lines for drawing
    /// Returns pairs of (from, to) bone positions
    pub fn get_skeleton_lines(&self) -> Vec<(Vec3, Vec3)> {
        vec![
            // Spine
            (self.head, self.neck),
            (self.neck, self.spine_1),
            (self.spine_1, self.spine_2),
            (self.spine_2, self.pelvis),
            
            // Left arm
            (self.neck, self.left_shoulder),
            (self.left_shoulder, self.left_elbow),
            (self.left_elbow, self.left_hand),
            
            // Right arm
            (self.neck, self.right_shoulder),
            (self.right_shoulder, self.right_elbow),
            (self.right_elbow, self.right_hand),
            
            // Left leg
            (self.pelvis, self.left_hip),
            (self.left_hip, self.left_knee),
            (self.left_knee, self.left_foot),
            
            // Right leg
            (self.pelvis, self.right_hip),
            (self.right_hip, self.right_knee),
            (self.right_knee, self.right_foot),
        ]
    }
}

#[derive(Clone, Copy, Debug)]
pub struct Entity {
    pub pawn: usize,
    pub controller: usize,
    pub pos: Vec3,
    pub health: i32,
    pub team: i32,
    pub bones: Bones,
}
