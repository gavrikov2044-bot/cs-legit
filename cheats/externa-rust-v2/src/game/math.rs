use glam::{Vec3, Vec2};

/// World to Screen transformation - Row-Major (CS2/Source2 view matrix layout)
/// Matrix layout: matrix[row][col] where rows are X, Y, Z, W
pub fn w2s(matrix: &[[f32; 4]; 4], pos: Vec3, screen_width: f32, screen_height: f32) -> Option<Vec2> {
    // Row-Major: W (clip) comes from row 3
    let w = matrix[3][0] * pos.x + matrix[3][1] * pos.y + matrix[3][2] * pos.z + matrix[3][3];
    
    // Behind camera check
    if w < 0.001 {
        return None;
    }

    // X from row 0, Y from row 1
    let x = matrix[0][0] * pos.x + matrix[0][1] * pos.y + matrix[0][2] * pos.z + matrix[0][3];
    let y = matrix[1][0] * pos.x + matrix[1][1] * pos.y + matrix[1][2] * pos.z + matrix[1][3];

    // NDC to screen coordinates
    let screen_x = (screen_width * 0.5) * (1.0 + x / w);
    let screen_y = (screen_height * 0.5) * (1.0 - y / w);

    Some(Vec2::new(screen_x, screen_y))
}

