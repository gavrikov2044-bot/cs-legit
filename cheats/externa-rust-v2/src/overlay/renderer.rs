use anyhow::Result;
use rand::Rng;
use windows::core::HSTRING;
use windows::Win32::Foundation::{HWND, RECT};
use windows::Win32::Graphics::Direct2D::{
    ID2D1Factory, ID2D1HwndRenderTarget, ID2D1SolidColorBrush,
    D2D1CreateFactory, D2D1_FACTORY_TYPE_SINGLE_THREADED,
    D2D1_RENDER_TARGET_PROPERTIES, D2D1_HWND_RENDER_TARGET_PROPERTIES,
    D2D1_PRESENT_OPTIONS_NONE, D2D1_DEBUG_LEVEL_NONE,
    D2D1_FACTORY_OPTIONS,
};
use windows::Win32::Graphics::Dxgi::Common::DXGI_FORMAT_B8G8R8A8_UNORM;
use windows::Win32::Graphics::Direct2D::Common::{
    D2D1_ALPHA_MODE_PREMULTIPLIED, D2D1_PIXEL_FORMAT, D2D1_COLOR_F, 
    D2D_SIZE_U, D2D_POINT_2F, D2D_RECT_F,
};
use windows::Win32::UI::WindowsAndMessaging::{
    CreateWindowExW, RegisterClassExW, DefWindowProcW, ShowWindow,
    WS_EX_TOPMOST, WS_EX_LAYERED, WS_EX_TRANSPARENT, WS_EX_NOACTIVATE, WS_POPUP, WS_VISIBLE,
    WNDCLASSEXW, CS_HREDRAW, CS_VREDRAW, SW_SHOWDEFAULT, SetLayeredWindowAttributes, LWA_COLORKEY,
    PeekMessageW, TranslateMessage, DispatchMessageW, PM_REMOVE, MSG,
    GetSystemMetrics, SM_CXSCREEN, SM_CYSCREEN,
};
use windows::Win32::System::LibraryLoader::GetModuleHandleW;
// DwmFlush disabled - was causing crashes
// use windows::Win32::Graphics::Dwm::DwmFlush;

pub struct Direct2DOverlay {
    #[allow(dead_code)]
    hwnd: HWND,
    _factory: ID2D1Factory,
    target: ID2D1HwndRenderTarget,
    brush_enemy: ID2D1SolidColorBrush,
    brush_team: ID2D1SolidColorBrush,
    brush_health: ID2D1SolidColorBrush,
    brush_health_bg: ID2D1SolidColorBrush,
    brush_skeleton: ID2D1SolidColorBrush,
    brush_snapline: ID2D1SolidColorBrush,
    pub width: u32,
    pub height: u32,
    class_name: String,
}

/// Generate random class name to avoid detection signatures
fn generate_random_class_name() -> String {
    let mut rng = rand::thread_rng();
    let prefixes = ["Microsoft", "Windows", "System", "Desktop", "Shell", "Explorer", "Task"];
    let suffixes = ["Helper", "Service", "Manager", "Worker", "Host", "Provider", "Handler"];
    
    format!(
        "{}{}{}{}",
        prefixes[rng.gen_range(0..prefixes.len())],
        suffixes[rng.gen_range(0..suffixes.len())],
        rng.gen_range(1000..9999),
        rng.gen_range(0..99)
    )
}

unsafe extern "system" fn wnd_proc(
    hwnd: HWND, 
    msg: u32, 
    wparam: windows::Win32::Foundation::WPARAM, 
    lparam: windows::Win32::Foundation::LPARAM
) -> windows::Win32::Foundation::LRESULT {
    use windows::Win32::UI::WindowsAndMessaging::PostQuitMessage;
    if msg == 0x0010 { // WM_CLOSE
        PostQuitMessage(0);
        return windows::Win32::Foundation::LRESULT(0);
    }
    DefWindowProcW(hwnd, msg, wparam, lparam)
}

impl Direct2DOverlay {
    pub fn new() -> Result<Self> {
        unsafe {
            let instance = GetModuleHandleW(None)?;
            
            // Randomized class name for anti-detection
            let class_name_str = generate_random_class_name();
            let class_name = HSTRING::from(&class_name_str);
            log::info!("[Overlay] Window class: {}", class_name_str);

            let wc = WNDCLASSEXW {
                cbSize: std::mem::size_of::<WNDCLASSEXW>() as u32,
                style: CS_HREDRAW | CS_VREDRAW,
                lpfnWndProc: Some(wnd_proc),
                hInstance: instance.into(),
                lpszClassName: windows::core::PCWSTR::from_raw(class_name.as_ptr()),
                ..Default::default()
            };
            RegisterClassExW(&wc);

            // Auto-detect screen resolution
            let width = GetSystemMetrics(SM_CXSCREEN) as u32;
            let height = GetSystemMetrics(SM_CYSCREEN) as u32;
            log::info!("[Overlay] Screen resolution: {}x{}", width, height);

            let hwnd = CreateWindowExW(
                WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
                &class_name,
                &HSTRING::from(""),
                WS_POPUP | WS_VISIBLE,
                0, 0, width as i32, height as i32,
                None, None, Some(instance.into()), None
            )?;

            // Black pixels = transparent
            SetLayeredWindowAttributes(hwnd, windows::Win32::Foundation::COLORREF(0), 0, LWA_COLORKEY)?;
            let _ = ShowWindow(hwnd, SW_SHOWDEFAULT);

            // Init D2D
            let factory_options = D2D1_FACTORY_OPTIONS { debugLevel: D2D1_DEBUG_LEVEL_NONE };
            let factory: ID2D1Factory = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, Some(&factory_options))?;
            
            let _rc = RECT { left: 0, top: 0, right: width as i32, bottom: height as i32 };
            let render_props = D2D1_RENDER_TARGET_PROPERTIES {
                pixelFormat: D2D1_PIXEL_FORMAT {
                    format: DXGI_FORMAT_B8G8R8A8_UNORM,
                    alphaMode: D2D1_ALPHA_MODE_PREMULTIPLIED,
                },
                ..Default::default()
            };
            let hwnd_props = D2D1_HWND_RENDER_TARGET_PROPERTIES {
                hwnd,
                pixelSize: D2D_SIZE_U { width, height },
                presentOptions: D2D1_PRESENT_OPTIONS_NONE,
            };

            let target = factory.CreateHwndRenderTarget(&render_props, &hwnd_props)?;
            
            // Create brushes
            let brush_enemy = target.CreateSolidColorBrush(&D2D1_COLOR_F { r: 1.0, g: 0.2, b: 0.2, a: 1.0 }, None)?;
            let brush_team = target.CreateSolidColorBrush(&D2D1_COLOR_F { r: 0.2, g: 1.0, b: 0.2, a: 1.0 }, None)?;
            let brush_health = target.CreateSolidColorBrush(&D2D1_COLOR_F { r: 0.0, g: 1.0, b: 0.0, a: 1.0 }, None)?;
            let brush_health_bg = target.CreateSolidColorBrush(&D2D1_COLOR_F { r: 0.15, g: 0.15, b: 0.15, a: 1.0 }, None)?;
            let brush_skeleton = target.CreateSolidColorBrush(&D2D1_COLOR_F { r: 1.0, g: 1.0, b: 1.0, a: 1.0 }, None)?;
            let brush_snapline = target.CreateSolidColorBrush(&D2D1_COLOR_F { r: 1.0, g: 0.8, b: 0.0, a: 0.7 }, None)?;

            Ok(Self { 
                hwnd, 
                _factory: factory, 
                target, 
                brush_enemy, 
                brush_team,
                brush_health,
                brush_health_bg,
                brush_skeleton,
                brush_snapline,
                width, 
                height,
                class_name: class_name_str,
            })
        }
    }

    pub fn begin_scene(&self) -> bool {
        unsafe {
            self.target.BeginDraw();
            self.target.Clear(Some(&D2D1_COLOR_F { r: 0.0, g: 0.0, b: 0.0, a: 0.0 }));
            true
        }
    }

    pub fn end_scene(&self) -> bool {
        unsafe {
            // EndDraw returns error if device lost
            match self.target.EndDraw(None, None) {
                Ok(_) => {
                    // DwmFlush disabled - was causing crashes on some systems
                    // let _ = DwmFlush();
                    true
                }
                Err(e) => {
                    log::error!("[Overlay] EndDraw failed: {:?}", e);
                    false
                }
            }
        }
    }

    /// Draw ESP box around player
    pub fn draw_box(&self, x: f32, y: f32, w: f32, h: f32, enemy: bool) {
        unsafe {
            let brush = if enemy { &self.brush_enemy } else { &self.brush_team };
            let rect = D2D_RECT_F {
                left: x, top: y, right: x + w, bottom: y + h
            };
            self.target.DrawRectangle(&rect, brush, 1.5, None);
        }
    }

    /// Draw line (for skeleton ESP)
    pub fn draw_line(&self, x1: f32, y1: f32, x2: f32, y2: f32, is_skeleton: bool) {
        unsafe {
            let brush = if is_skeleton { &self.brush_skeleton } else { &self.brush_enemy };
            let p1 = D2D_POINT_2F { x: x1, y: y1 };
            let p2 = D2D_POINT_2F { x: x2, y: y2 };
            self.target.DrawLine(p1, p2, brush, if is_skeleton { 1.0 } else { 1.5 }, None);
        }
    }

    /// Draw snapline from bottom center of screen to player
    pub fn draw_snapline(&self, target_x: f32, target_y: f32) {
        unsafe {
            let p1 = D2D_POINT_2F { x: self.width as f32 / 2.0, y: self.height as f32 };
            let p2 = D2D_POINT_2F { x: target_x, y: target_y };
            self.target.DrawLine(p1, p2, &self.brush_snapline, 1.0, None);
        }
    }

    /// Draw health bar next to ESP box
    pub fn draw_health_bar(&self, x: f32, y: f32, h: f32, health: i32) {
        let bar_width = 4.0;
        let bar_x = x - bar_width - 2.0;
        let health_ratio = (health as f32 / 100.0).clamp(0.0, 1.0);
        let health_height = h * health_ratio;
        
        unsafe {
            // Background
            let bg_rect = D2D_RECT_F {
                left: bar_x, top: y, right: bar_x + bar_width, bottom: y + h
            };
            self.target.FillRectangle(&bg_rect, &self.brush_health_bg);
            
            // Health fill (from bottom)
            let health_rect = D2D_RECT_F {
                left: bar_x, 
                top: y + h - health_height, 
                right: bar_x + bar_width, 
                bottom: y + h
            };
            self.target.FillRectangle(&health_rect, &self.brush_health);
        }
    }

    pub fn handle_message(&self) -> bool {
        unsafe {
            let mut msg = MSG::default();
            while PeekMessageW(&mut msg, None, 0, 0, PM_REMOVE).as_bool() {
                if msg.message == 0x0012 { // WM_QUIT
                    return false; 
                }
                let _ = TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            true
        }
    }
    
    #[allow(dead_code)]
    pub fn hwnd(&self) -> HWND {
        self.hwnd
    }
    
    pub fn class_name(&self) -> &str {
        &self.class_name
    }
}

impl Drop for Direct2DOverlay {
    fn drop(&mut self) {
        log::info!("[Overlay] Shutting down...");
    }
}
