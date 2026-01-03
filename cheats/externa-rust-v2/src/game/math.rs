use glam::{Vec3, Vec2};

/// World to Screen transformation - DragonBurn implementation (row-major)
pub fn w2s(matrix: &[[f32; 4]; 4], pos: Vec3, screen_width: f32, screen_height: f32) -> Option<Vec2> {
    let sight_x = screen_width / 2.0;
    let sight_y = screen_height / 2.0;
    
    // clip_w (view depth)
    let view = matrix[3][0] * pos.x + matrix[3][1] * pos.y + matrix[3][2] * pos.z + matrix[3][3];
    
    if view <= 0.01 {
        return None;
    }

    // Screen coordinates (DragonBurn formula)
    let screen_x = sight_x + (matrix[0][0] * pos.x + matrix[0][1] * pos.y + matrix[0][2] * pos.z + matrix[0][3]) / view * sight_x;
    let screen_y = sight_y - (matrix[1][0] * pos.x + matrix[1][1] * pos.y + matrix[1][2] * pos.z + matrix[1][3]) / view * sight_y;

    Some(Vec2::new(screen_x, screen_y))
}

