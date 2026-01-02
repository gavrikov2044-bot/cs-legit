use glam::{Vec3, Vec2};

pub fn w2s(matrix: &[[f32; 4]; 4], pos: Vec3, screen_width: f32, screen_height: f32) -> Option<Vec2> {
    let w = matrix[3][0] * pos.x + matrix[3][1] * pos.y + matrix[3][2] * pos.z + matrix[3][3];
    
    if w < 0.001 {
        return None;
    }

    let x = matrix[0][0] * pos.x + matrix[0][1] * pos.y + matrix[0][2] * pos.z + matrix[0][3];
    let y = matrix[1][0] * pos.x + matrix[1][1] * pos.y + matrix[1][2] * pos.z + matrix[1][3];

    let inv_w = 1.0 / w;
    let sx = (screen_width / 2.0) * (1.0 + x * inv_w);
    let sy = (screen_height / 2.0) * (1.0 - y * inv_w);

    Some(Vec2::new(sx, sy))
}

