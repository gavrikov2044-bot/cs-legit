use glam::{Vec3, Vec2};

/// World to Screen transformation (Colin's working version)
/// Returns screen pixel coordinates
pub fn w2s(matrix: &[[f32; 4]; 4], pos: Vec3, screen_width: f32, screen_height: f32) -> Option<Vec2> {
    // Calculate clip coordinates (row-major matrix)
    let vx = pos.x * matrix[0][0] + pos.y * matrix[0][1] + pos.z * matrix[0][2] + matrix[0][3];
    let vy = pos.x * matrix[1][0] + pos.y * matrix[1][1] + pos.z * matrix[1][2] + matrix[1][3];
    let vw = pos.x * matrix[3][0] + pos.y * matrix[3][1] + pos.z * matrix[3][2] + matrix[3][3];
    
    // Behind camera check
    if vw < 0.1 {
        return None;
    }
    
    // Perspective divide to NDC (-1 to 1)
    let rw = 1.0 / vw;
    let ndx = vx * rw;
    let ndy = vy * rw;
    
    // NDC to screen pixels
    let screen_x = (ndx * 0.5 + 0.5) * screen_width;
    let screen_y = (1.0 - (ndy * 0.5 + 0.5)) * screen_height;

    Some(Vec2::new(screen_x, screen_y))
}

