//! Overlay window module
//! Based on Valthrun overlay architecture
//! Features: Stream-Proof, Transparent, Click-Through

use std::sync::Arc;
use std::mem;

use anyhow::Result;
use windows::core::w;
use windows::Win32::Foundation::{HWND, LPARAM, LRESULT, WPARAM, RECT, BOOL};
use windows::Win32::Graphics::Gdi::{
    CreateSolidBrush, CreatePen, SelectObject, DeleteObject,
    Rectangle, FillRect, SetBkMode, TRANSPARENT, SetTextColor,
    GetDC, ReleaseDC, InvalidateRect, CreateFontW, DrawTextW, UpdateWindow,
    PS_SOLID, GetStockObject, NULL_BRUSH, HRGN, HGDIOBJ, DT_CENTER, DT_VCENTER, DT_SINGLELINE,
    FW_BOLD, FONT_CHARSET, FONT_OUTPUT_PRECISION, FONT_CLIP_PRECISION, FONT_QUALITY,
};
use windows::Win32::Graphics::Dwm::{
    DwmExtendFrameIntoClientArea, DwmEnableBlurBehindWindow, DwmIsCompositionEnabled,
    DWM_BLURBEHIND, DWM_BB_ENABLE,
};
use windows::Win32::UI::Controls::MARGINS;
use windows::Win32::UI::WindowsAndMessaging::{
    CreateWindowExW, RegisterClassExW, DefWindowProcW, PostQuitMessage,
    ShowWindow, TranslateMessage, DispatchMessageW,
    GetSystemMetrics, GetWindowLongPtrW, SetWindowLongPtrW,
    SetForegroundWindow, PeekMessageW, SetWindowPos, SetWindowDisplayAffinity,
    WNDCLASSEXW, MSG, 
    WS_POPUP, WS_VISIBLE, WS_CLIPSIBLINGS,
    WS_EX_TRANSPARENT, WS_EX_TOOLWINDOW, WS_EX_NOACTIVATE, WS_EX_LAYERED,
    WM_DESTROY, WM_NCHITTEST, WM_ERASEBKGND,
    SW_SHOWNOACTIVATE, HTTRANSPARENT, SM_CXSCREEN, SM_CYSCREEN,
    GWL_EXSTYLE, PM_REMOVE, HWND_TOPMOST,
    SWP_NOMOVE, SWP_NOSIZE, SWP_NOACTIVATE,
    CS_HREDRAW, CS_VREDRAW,
    WDA_EXCLUDEFROMCAPTURE, WDA_NONE,
};
use windows::Win32::UI::Input::KeyboardAndMouse::GetAsyncKeyState;

use crate::{SharedState, RUNNING};
use crate::esp::{generate_esp_commands, DrawCommand, Color};

/// Overlay window
pub struct Overlay {
    hwnd: HWND,
    width: i32,
    height: i32,
    state: Arc<SharedState>,
    stream_proof_enabled: bool,
}

// Store state pointer for WndProc
static mut OVERLAY_STATE: Option<*const SharedState> = None;

impl Overlay {
    /// Create overlay window (Valthrun-style)
    pub fn create(state: Arc<SharedState>) -> Result<Self> {
        let width = unsafe { GetSystemMetrics(SM_CXSCREEN) };
        let height = unsafe { GetSystemMetrics(SM_CYSCREEN) };
        
        unsafe {
            OVERLAY_STATE = Some(Arc::as_ptr(&state));
        }
        
        let class_name = w!("ExternaOverlayRust");
        
        let wc = WNDCLASSEXW {
            cbSize: mem::size_of::<WNDCLASSEXW>() as u32,
            style: CS_HREDRAW | CS_VREDRAW,
            lpfnWndProc: Some(wnd_proc),
            hInstance: unsafe { windows::Win32::System::LibraryLoader::GetModuleHandleW(None)? }.into(),
            lpszClassName: class_name,
            ..Default::default()
        };
        
        unsafe { RegisterClassExW(&wc) };
        
        let hwnd = unsafe {
            CreateWindowExW(
                WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
                class_name,
                w!(""),
                WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS,
                0, 0, width, height,
                None,
                None,
                None,
                None,
            )?
        };
        
        unsafe {
            if !DwmIsCompositionEnabled()?.as_bool() {
                log::error!("DWM Composition is disabled!");
            }
            
            let mut bb = DWM_BLURBEHIND::default();
            bb.dwFlags = DWM_BB_ENABLE;
            bb.fEnable = BOOL::from(true);
            bb.hRgnBlur = HRGN::default();
            let _ = DwmEnableBlurBehindWindow(hwnd, &bb);
            
            let _ = DwmExtendFrameIntoClientArea(hwnd, &MARGINS {
                cxLeftWidth: -1,
                cxRightWidth: -1,
                cyTopHeight: -1,
                cyBottomHeight: -1,
            });
            
            let _ = SetWindowPos(hwnd, Some(HWND_TOPMOST), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        
        Ok(Self { 
            hwnd, 
            width, 
            height, 
            state,
            stream_proof_enabled: false,
        })
    }
    
    /// Toggle Stream-Proof mode (hide from screen capture)
    pub fn set_stream_proof(&mut self, enabled: bool) {
        unsafe {
            let affinity = if enabled { WDA_EXCLUDEFROMCAPTURE } else { WDA_NONE };
            if SetWindowDisplayAffinity(self.hwnd, affinity).is_ok() {
                self.stream_proof_enabled = enabled;
                log::info!("Stream-Proof: {}", if enabled { "ENABLED" } else { "DISABLED" });
            } else {
                log::warn!("Failed to set stream-proof mode");
            }
        }
    }
    
    /// Run main loop
    pub fn run(&mut self) -> Result<()> {
        self.set_stream_proof(true);
        
        unsafe { ShowWindow(self.hwnd, SW_SHOWNOACTIVATE) };
        
        let mut msg = MSG::default();
        
        loop {
            while unsafe { PeekMessageW(&mut msg, None, 0, 0, PM_REMOVE) }.as_bool() {
                if msg.message == windows::Win32::UI::WindowsAndMessaging::WM_QUIT {
                    RUNNING.store(false, std::sync::atomic::Ordering::Relaxed);
                    break;
                }
                unsafe {
                    let _ = TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
            }
            
            if !RUNNING.load(std::sync::atomic::Ordering::Relaxed) {
                break;
            }
            
            // INSERT key toggle
            if unsafe { GetAsyncKeyState(0x2D) } & 1 != 0 {  // VK_INSERT = 0x2D
                let mut settings = self.state.settings.write();
                settings.menu_open = !settings.menu_open;
                
                let ex_style = unsafe { GetWindowLongPtrW(self.hwnd, GWL_EXSTYLE) } as u32;
                
                if settings.menu_open {
                    unsafe {
                        SetWindowLongPtrW(
                            self.hwnd, 
                            GWL_EXSTYLE, 
                            (ex_style & !WS_EX_TRANSPARENT.0) as isize
                        );
                        let _ = SetForegroundWindow(self.hwnd);
                    }
                    log::info!("Menu opened");
                } else {
                    unsafe {
                        SetWindowLongPtrW(
                            self.hwnd, 
                            GWL_EXSTYLE, 
                            (ex_style | WS_EX_TRANSPARENT.0) as isize
                        );
                    }
                    log::info!("Menu closed");
                }
            }
            
            self.render();
            
            std::thread::sleep(std::time::Duration::from_millis(8));
        }
        
        Ok(())
    }
    
    /// Render frame
    fn render(&self) {
        unsafe { let _ = InvalidateRect(Some(self.hwnd), None, true); }
        
        let settings = self.state.settings.read().clone();
        let esp_data = self.state.esp_data.read().clone();
        
        let commands = generate_esp_commands(
            &esp_data, 
            &settings, 
            self.width as f32, 
            self.height as f32
        );
        
        let hdc = unsafe { GetDC(Some(self.hwnd)) };
        unsafe { SetBkMode(hdc, TRANSPARENT); }
        
        for cmd in commands {
            match cmd {
                DrawCommand::Rect { x, y, w, h, color, thickness } => {
                    self.draw_rect(hdc, x, y, w, h, color, thickness as i32);
                }
                DrawCommand::RectFilled { x, y, w, h, color } => {
                    self.draw_rect_filled(hdc, x, y, w, h, color);
                }
                DrawCommand::Text { .. } => {}
            }
        }
        
        if settings.menu_open {
            log::debug!("Drawing menu at ({}, {})", self.width / 2 - 200, self.height / 2 - 150);
            self.draw_menu(hdc);
        }
        
        unsafe { ReleaseDC(Some(self.hwnd), hdc); }
        
        // Force window update
        unsafe {
            let _ = windows::Win32::Graphics::Gdi::UpdateWindow(self.hwnd);
        }
    }
    
    fn draw_rect(&self, hdc: windows::Win32::Graphics::Gdi::HDC, x: f32, y: f32, w: f32, h: f32, color: Color, thickness: i32) {
        unsafe {
            let pen = CreatePen(PS_SOLID, thickness, windows::Win32::Foundation::COLORREF(
                (color.r as u32) | ((color.g as u32) << 8) | ((color.b as u32) << 16)
            ));
            let old_pen = SelectObject(hdc, HGDIOBJ(pen.0));
            let null_brush = GetStockObject(NULL_BRUSH);
            let old_brush = SelectObject(hdc, null_brush);
            
            Rectangle(hdc, x as i32, y as i32, (x + w) as i32, (y + h) as i32);
            
            SelectObject(hdc, old_pen);
            SelectObject(hdc, old_brush);
            DeleteObject(HGDIOBJ(pen.0));
        }
    }
    
    fn draw_rect_filled(&self, hdc: windows::Win32::Graphics::Gdi::HDC, x: f32, y: f32, w: f32, h: f32, color: Color) {
        unsafe {
            let brush = CreateSolidBrush(windows::Win32::Foundation::COLORREF(
                (color.r as u32) | ((color.g as u32) << 8) | ((color.b as u32) << 16)
            ));
            
            let rect = RECT {
                left: x as i32,
                top: y as i32,
                right: (x + w) as i32,
                bottom: (y + h) as i32,
            };
            
            FillRect(hdc, &rect, brush);
            DeleteObject(HGDIOBJ(brush.0));
        }
    }
    
    fn draw_menu(&self, hdc: windows::Win32::Graphics::Gdi::HDC) {
        let menu_x = (self.width / 2 - 200) as f32;
        let menu_y = (self.height / 2 - 150) as f32;
        let menu_w = 400.0;
        let menu_h = 300.0;
        
        // Background
        self.draw_rect_filled(hdc, menu_x, menu_y, menu_w, menu_h, Color::new(20, 20, 30, 240));
        self.draw_rect(hdc, menu_x, menu_y, menu_w, menu_h, Color::new(80, 80, 200, 255), 2);
        
        // Header
        self.draw_rect_filled(hdc, menu_x, menu_y, menu_w, 35.0, Color::new(40, 40, 60, 255));
        self.draw_rect_filled(hdc, menu_x, menu_y + 35.0, menu_w, 2.0, Color::new(100, 100, 255, 255));
        self.draw_text(hdc, "EXTERNA CS2", (menu_x + menu_w / 2.0) as i32, (menu_y + 10.0) as i32, Color::new(255, 255, 255, 255), 18);
        
        // ESP Settings box
        self.draw_rect_filled(hdc, menu_x + 20.0, menu_y + 50.0, 360.0, 130.0, Color::new(30, 30, 45, 255));
        self.draw_rect(hdc, menu_x + 20.0, menu_y + 50.0, 360.0, 130.0, Color::new(60, 60, 80, 255), 1);
        
        // Settings text
        self.draw_text_left(hdc, "ESP Settings", (menu_x + 30.0) as i32, (menu_y + 58.0) as i32, Color::new(100, 100, 255, 255), 14);
        self.draw_text_left(hdc, "[x] Box ESP", (menu_x + 40.0) as i32, (menu_y + 85.0) as i32, Color::new(200, 200, 200, 255), 13);
        self.draw_text_left(hdc, "[x] Health Bar", (menu_x + 40.0) as i32, (menu_y + 105.0) as i32, Color::new(200, 200, 200, 255), 13);
        self.draw_text_left(hdc, "[x] Armor Bar", (menu_x + 40.0) as i32, (menu_y + 125.0) as i32, Color::new(200, 200, 200, 255), 13);
        self.draw_text_left(hdc, "[x] Stream-Proof", (menu_x + 40.0) as i32, (menu_y + 145.0) as i32, Color::new(100, 255, 100, 255), 13);
        
        // Close button
        let btn_x = menu_x + 100.0;
        let btn_y = menu_y + menu_h - 55.0;
        let btn_w = 200.0;
        let btn_h = 40.0;
        
        self.draw_rect_filled(hdc, btn_x, btn_y, btn_w, btn_h, Color::new(180, 40, 40, 255));
        self.draw_rect(hdc, btn_x, btn_y, btn_w, btn_h, Color::new(220, 60, 60, 255), 1);
        self.draw_text(hdc, "CLOSE CHEAT", (btn_x + btn_w / 2.0) as i32, (btn_y + 10.0) as i32, Color::new(255, 255, 255, 255), 14);
        
        // Footer
        self.draw_text(hdc, "Press INSERT to close menu", (menu_x + menu_w / 2.0) as i32, (menu_y + menu_h - 20.0) as i32, Color::new(100, 100, 100, 255), 11);
    }
    
    fn draw_text(&self, hdc: windows::Win32::Graphics::Gdi::HDC, text: &str, x: i32, y: i32, color: Color, size: i32) {
        unsafe {
            let font = CreateFontW(
                size, 0, 0, 0, FW_BOLD.0 as i32, 0, 0, 0,
                FONT_CHARSET(1), FONT_OUTPUT_PRECISION(0), FONT_CLIP_PRECISION(0), FONT_QUALITY(5), 0,
                windows::core::w!("Segoe UI"),
            );
            
            let old_font = SelectObject(hdc, HGDIOBJ(font.0));
            SetTextColor(hdc, windows::Win32::Foundation::COLORREF(
                (color.r as u32) | ((color.g as u32) << 8) | ((color.b as u32) << 16)
            ));
            
            let mut wide: Vec<u16> = text.encode_utf16().chain(std::iter::once(0)).collect();
            let len = wide.len() - 1;
            let mut rect = RECT {
                left: x - 150,
                top: y,
                right: x + 150,
                bottom: y + size + 5,
            };
            
            DrawTextW(hdc, &mut wide[..len], &mut rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            SelectObject(hdc, old_font);
            DeleteObject(HGDIOBJ(font.0));
        }
    }
    
    fn draw_text_left(&self, hdc: windows::Win32::Graphics::Gdi::HDC, text: &str, x: i32, y: i32, color: Color, size: i32) {
        unsafe {
            let font = CreateFontW(
                size, 0, 0, 0, 400, 0, 0, 0,
                FONT_CHARSET(1), FONT_OUTPUT_PRECISION(0), FONT_CLIP_PRECISION(0), FONT_QUALITY(5), 0,
                windows::core::w!("Segoe UI"),
            );
            
            let old_font = SelectObject(hdc, HGDIOBJ(font.0));
            SetTextColor(hdc, windows::Win32::Foundation::COLORREF(
                (color.r as u32) | ((color.g as u32) << 8) | ((color.b as u32) << 16)
            ));
            
            let mut wide: Vec<u16> = text.encode_utf16().chain(std::iter::once(0)).collect();
            let len = wide.len() - 1;
            let mut rect = RECT {
                left: x,
                top: y,
                right: x + 300,
                bottom: y + size + 5,
            };
            
            DrawTextW(hdc, &mut wide[..len], &mut rect, DT_SINGLELINE);
            
            SelectObject(hdc, old_font);
            DeleteObject(HGDIOBJ(font.0));
        }
    }
}

/// Window procedure
unsafe extern "system" fn wnd_proc(hwnd: HWND, msg: u32, wparam: WPARAM, lparam: LPARAM) -> LRESULT {
    match msg {
        WM_DESTROY => {
            RUNNING.store(false, std::sync::atomic::Ordering::Relaxed);
            PostQuitMessage(0);
            LRESULT(0)
        }
        WM_ERASEBKGND => {
            LRESULT(1)
        }
        WM_NCHITTEST => {
            if let Some(state_ptr) = OVERLAY_STATE {
                let state = &*state_ptr;
                if !state.settings.read().menu_open {
                    return LRESULT(HTTRANSPARENT as isize);
                }
            }
            DefWindowProcW(hwnd, msg, wparam, lparam)
        }
        _ => DefWindowProcW(hwnd, msg, wparam, lparam),
    }
}
