//! ESP rendering module

use glam::Vec3;

/// View matrix (4x4)
pub type ViewMatrix = [[f32; 4]; 4];

/// Single entity data
#[derive(Clone, Default)]
pub struct EntityData {
    pub origin: Vec3,
    pub health: i32,
    pub armor: i32,
}

/// All ESP data for current frame
#[derive(Clone, Default)]
pub struct EspData {
    pub view_matrix: ViewMatrix,
    pub entities: Vec<EntityData>,
}

/// Screen position
#[derive(Clone, Copy, Default)]
pub struct ScreenPos {
    pub x: f32,
    pub y: f32,
}

/// World to Screen transformation
pub fn world_to_screen(
    pos: Vec3, 
    matrix: &ViewMatrix, 
    screen_width: f32, 
    screen_height: f32
) -> Option<ScreenPos> {
    let w = matrix[3][0] * pos.x + matrix[3][1] * pos.y + matrix[3][2] * pos.z + matrix[3][3];
    
    if w < 0.001 {
        return None;
    }
    
    let inv = 1.0 / w;
    
    let x = (screen_width / 2.0) * (1.0 + (
        matrix[0][0] * pos.x + matrix[0][1] * pos.y + matrix[0][2] * pos.z + matrix[0][3]
    ) * inv);
    
    let y = (screen_height / 2.0) * (1.0 - (
        matrix[1][0] * pos.x + matrix[1][1] * pos.y + matrix[1][2] * pos.z + matrix[1][3]
    ) * inv);
    
    Some(ScreenPos { x, y })
}

/// RGBA Color
#[derive(Clone, Copy)]
pub struct Color {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: u8,
}

impl Color {
    pub const fn new(r: u8, g: u8, b: u8, a: u8) -> Self {
        Self { r, g, b, a }
    }
    
    pub const RED: Color = Color::new(255, 0, 0, 255);
    pub const GREEN: Color = Color::new(0, 255, 0, 255);
    pub const BLUE: Color = Color::new(0, 100, 255, 255);
    pub const BLACK: Color = Color::new(0, 0, 0, 255);
    pub const WHITE: Color = Color::new(255, 255, 255, 255);
    pub const ORANGE: Color = Color::new(255, 165, 0, 255);
    pub const BACKGROUND: Color = Color::new(0, 0, 0, 128);
    
    /// Convert to ARGB u32 (for D3D)
    pub fn to_argb(&self) -> u32 {
        ((self.a as u32) << 24) | ((self.r as u32) << 16) | ((self.g as u32) << 8) | (self.b as u32)
    }
}

/// Draw command for renderer
#[derive(Clone)]
pub enum DrawCommand {
    Rect { x: f32, y: f32, w: f32, h: f32, color: Color, thickness: f32 },
    RectFilled { x: f32, y: f32, w: f32, h: f32, color: Color },
    Text { x: f32, y: f32, text: String, color: Color },
}

/// Generate draw commands for ESP
pub fn generate_esp_commands(
    data: &EspData,
    settings: &crate::Settings,
    screen_width: f32,
    screen_height: f32,
) -> Vec<DrawCommand> {
    let mut commands = Vec::new();
    
    if !settings.esp_enabled {
        return commands;
    }
    
    for entity in &data.entities {
        // Calculate screen positions
        let feet = match world_to_screen(entity.origin, &data.view_matrix, screen_width, screen_height) {
            Some(p) => p,
            None => continue,
        };
        
        let head_pos = entity.origin + Vec3::new(0.0, 0.0, 72.0);
        let head = match world_to_screen(head_pos, &data.view_matrix, screen_width, screen_height) {
            Some(p) => p,
            None => continue,
        };
        
        // Calculate box dimensions
        let box_height = feet.y - head.y;
        let box_width = box_height * 0.4;
        let box_x = head.x - box_width / 2.0;
        
        // Box ESP
        if settings.box_esp {
            // Black outline
            commands.push(DrawCommand::Rect {
                x: box_x - 1.0,
                y: head.y - 1.0,
                w: box_width + 2.0,
                h: box_height + 2.0,
                color: Color::BLACK,
                thickness: 3.0,
            });
            
            // Red box
            commands.push(DrawCommand::Rect {
                x: box_x,
                y: head.y,
                w: box_width,
                h: box_height,
                color: Color::RED,
                thickness: 1.5,
            });
        }
        
        // Health bar
        if settings.health_bar {
            let bar_x = box_x - 6.0;
            let health_ratio = entity.health as f32 / 100.0;
            let health_height = box_height * health_ratio;
            
            // Background
            commands.push(DrawCommand::RectFilled {
                x: bar_x,
                y: head.y,
                w: 4.0,
                h: box_height,
                color: Color::BACKGROUND,
            });
            
            // Health
            commands.push(DrawCommand::RectFilled {
                x: bar_x + 1.0,
                y: feet.y - health_height,
                w: 2.0,
                h: health_height,
                color: Color::GREEN,
            });
            
            // Outline
            commands.push(DrawCommand::Rect {
                x: bar_x,
                y: head.y,
                w: 4.0,
                h: box_height,
                color: Color::BLACK,
                thickness: 1.0,
            });
        }
        
        // Armor bar
        if settings.armor_bar && entity.armor > 0 {
            let bar_x = box_x + box_width + 2.0;
            let armor_ratio = entity.armor as f32 / 100.0;
            let armor_height = box_height * armor_ratio;
            
            // Background
            commands.push(DrawCommand::RectFilled {
                x: bar_x,
                y: head.y,
                w: 4.0,
                h: box_height,
                color: Color::BACKGROUND,
            });
            
            // Armor
            commands.push(DrawCommand::RectFilled {
                x: bar_x + 1.0,
                y: feet.y - armor_height,
                w: 2.0,
                h: armor_height,
                color: Color::BLUE,
            });
            
            // Outline
            commands.push(DrawCommand::Rect {
                x: bar_x,
                y: head.y,
                w: 4.0,
                h: box_height,
                color: Color::BLACK,
                thickness: 1.0,
            });
        }
    }
    
    commands
}

