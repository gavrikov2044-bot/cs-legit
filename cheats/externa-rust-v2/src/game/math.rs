use glam::{Vec3, Vec2};

/// World to Screen transformation (DragonBurn formula - proven stable)
/// Uses exact same math as DragonBurn/View.h for guaranteed accuracy
pub fn w2s(matrix: &[[f32; 4]; 4], pos: Vec3, screen_width: f32, screen_height: f32) -> Option<Vec2> {
    // Calculate W (depth) - same as DragonBurn View.h line 14
    let w = pos.x * matrix[3][0] + pos.y * matrix[3][1] + pos.z * matrix[3][2] + matrix[3][3];
    
    // Behind camera check (DragonBurn uses 0.01)
    if w < 0.01 {
        return None;
    }
    
    // Calculate X and Y clip coords
    let vx = pos.x * matrix[0][0] + pos.y * matrix[0][1] + pos.z * matrix[0][2] + matrix[0][3];
    let vy = pos.x * matrix[1][0] + pos.y * matrix[1][1] + pos.z * matrix[1][2] + matrix[1][3];
    
    // Screen center (SightX, SightY in DragonBurn)
    let sight_x = screen_width * 0.5;
    let sight_y = screen_height * 0.5;
    
    // DragonBurn formula exactly:
    // ToPos.x = SightX + (vx / View) * SightX
    // ToPos.y = SightY - (vy / View) * SightY
    let screen_x = sight_x + (vx / w) * sight_x;
    let screen_y = sight_y - (vy / w) * sight_y;

    Some(Vec2::new(screen_x, screen_y))
}

