use glam::{Vec3, Vec2};

/// World to Screen transformation
/// CS2 ViewMatrix is stored in a specific format - need to access by [col][row]
pub fn w2s(matrix: &[[f32; 4]; 4], pos: Vec3, screen_width: f32, screen_height: f32) -> Option<Vec2> {
    // CS2 ViewMatrix: access as matrix[col][row] for correct projection
    // W = last row of projection
    let w = matrix[0][3] * pos.x + matrix[1][3] * pos.y + matrix[2][3] * pos.z + matrix[3][3];
    
    if w < 0.001 {
        return None;
    }

    // X and Y from first two rows
    let x = matrix[0][0] * pos.x + matrix[1][0] * pos.y + matrix[2][0] * pos.z + matrix[3][0];
    let y = matrix[0][1] * pos.x + matrix[1][1] * pos.y + matrix[2][1] * pos.z + matrix[3][1];

    let inv_w = 1.0 / w;
    let sx = (screen_width / 2.0) * (1.0 + x * inv_w);
    let sy = (screen_height / 2.0) * (1.0 - y * inv_w);

    Some(Vec2::new(sx, sy))
}

