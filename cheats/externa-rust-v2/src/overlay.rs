use std::sync::atomic::{AtomicBool, Ordering};
use windows::core::{w, Result};
use windows::Win32::Foundation::{HWND, LPARAM, LRESULT, WPARAM, RECT, COLORREF};
use windows::Win32::Graphics::Gdi::{
    GetDC, ReleaseDC, CreatePen, CreateSolidBrush, SelectObject, DeleteObject,
    Rectangle, FillRect, SetBkMode, TRANSPARENT, PS_SOLID, HGDIOBJ, HBRUSH,
    InvalidateRect, BeginPaint, EndPaint, PAINTSTRUCT, GetStockObject, NULL_BRUSH,
    UpdateWindow,
};
use windows::Win32::UI::WindowsAndMessaging::{
    CreateWindowExW, RegisterClassExW, DefWindowProcW, ShowWindow,
    WS_EX_TOPMOST, WS_EX_LAYERED, WS_EX_TRANSPARENT, WS_EX_NOACTIVATE,
    WS_POPUP, WS_VISIBLE, WNDCLASSEXW, CS_HREDRAW, CS_VREDRAW,
    SW_SHOWDEFAULT, SetLayeredWindowAttributes, LWA_COLORKEY,
    PeekMessageW, TranslateMessage, DispatchMessageW, PM_REMOVE, MSG,
};
use windows::Win32::UI::Input::KeyboardAndMouse::GetAsyncKeyState;
use windows::Win32::System::LibraryLoader::GetModuleHandleW;

pub static RUNNING: AtomicBool = AtomicBool::new(true);

// Transparent color key (will be invisible)
const TRANSPARENT_COLOR: u32 = 0x00010101; // RGB(1,1,1)

unsafe extern "system" fn wnd_proc(hwnd: HWND, msg: u32, wparam: WPARAM, lparam: LPARAM) -> LRESULT {
    match msg {
        0x0010 /* WM_CLOSE */ => {
            RUNNING.store(false, Ordering::Relaxed);
            LRESULT(0)
        }
        0x000F /* WM_PAINT */ => {
            let mut ps = PAINTSTRUCT::default();
            let hdc = BeginPaint(hwnd, &mut ps);
            
            // Fill with transparent color
            let brush = CreateSolidBrush(COLORREF(TRANSPARENT_COLOR));
            FillRect(hdc, &ps.rcPaint, brush);
            DeleteObject(HGDIOBJ(brush.0));
            
            EndPaint(hwnd, &ps);
            LRESULT(0)
        }
        _ => DefWindowProcW(hwnd, msg, wparam, lparam)
    }
}

pub struct Overlay {
    hwnd: HWND,
    width: i32,
    height: i32,
}

impl Overlay {
    pub fn new() -> Result<Self> {
        unsafe {
            let instance = GetModuleHandleW(None)?;
            let class_name = w!("ExternaOverlay");

            let wc = WNDCLASSEXW {
                cbSize: std::mem::size_of::<WNDCLASSEXW>() as u32,
                style: CS_HREDRAW | CS_VREDRAW,
                lpfnWndProc: Some(wnd_proc),
                hInstance: instance.into(),
                lpszClassName: class_name,
                hbrBackground: HBRUSH(GetStockObject(NULL_BRUSH).0),
                ..Default::default()
            };
            RegisterClassExW(&wc);

            let width = 1920;
            let height = 1080;

            let hwnd = CreateWindowExW(
                WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
                class_name,
                w!(""),
                WS_POPUP | WS_VISIBLE,
                0, 0, width, height,
                None, None, Some(instance.into()), None
            )?;

            // Color key transparency - RGB(1,1,1) will be invisible
            SetLayeredWindowAttributes(hwnd, COLORREF(TRANSPARENT_COLOR), 0, LWA_COLORKEY)?;
            ShowWindow(hwnd, SW_SHOWDEFAULT);
            let _ = UpdateWindow(hwnd);

            Ok(Self { hwnd, width, height })
        }
    }

    /// Process Windows messages - MUST be called every frame
    pub fn process_messages(&self) -> bool {
        unsafe {
            let mut msg = MSG::default();
            while PeekMessageW(&mut msg, None, 0, 0, PM_REMOVE).as_bool() {
                if msg.message == 0x0012 /* WM_QUIT */ {
                    RUNNING.store(false, Ordering::Relaxed);
                    return false;
                }
                let _ = TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }

            // END key to exit
            if GetAsyncKeyState(0x23) & 0x8000u16 as i16 != 0 {
                RUNNING.store(false, Ordering::Relaxed);
                return false;
            }

            true
        }
    }

    /// Clear the overlay (redraw with transparent color)
    pub fn clear(&self) {
        unsafe {
            let _ = InvalidateRect(Some(self.hwnd), None, true);
            let _ = UpdateWindow(self.hwnd);
        }
    }

    pub fn draw_box(&self, x: f32, y: f32, w: f32, h: f32, r: u8, g: u8, b: u8) {
        unsafe {
            let hdc = GetDC(Some(self.hwnd));
            SetBkMode(hdc, TRANSPARENT);

            let color = COLORREF(r as u32 | ((g as u32) << 8) | ((b as u32) << 16));
            let pen = CreatePen(PS_SOLID, 2, color);
            let null_brush = GetStockObject(NULL_BRUSH);
            
            let old_pen = SelectObject(hdc, HGDIOBJ(pen.0));
            let old_brush = SelectObject(hdc, null_brush);

            Rectangle(hdc, x as i32, y as i32, (x + w) as i32, (y + h) as i32);

            SelectObject(hdc, old_pen);
            SelectObject(hdc, old_brush);
            DeleteObject(HGDIOBJ(pen.0));
            ReleaseDC(Some(self.hwnd), hdc);
        }
    }

    pub fn draw_filled_rect(&self, x: f32, y: f32, w: f32, h: f32, r: u8, g: u8, b: u8) {
        unsafe {
            let hdc = GetDC(Some(self.hwnd));

            let color = COLORREF(r as u32 | ((g as u32) << 8) | ((b as u32) << 16));
            let brush = CreateSolidBrush(color);

            let rect = RECT {
                left: x as i32,
                top: y as i32,
                right: (x + w) as i32,
                bottom: (y + h) as i32,
            };

            FillRect(hdc, &rect, brush);
            DeleteObject(HGDIOBJ(brush.0));
            ReleaseDC(Some(self.hwnd), hdc);
        }
    }

    /// Draw outlined box (more visible)
    pub fn draw_outlined_box(&self, x: f32, y: f32, w: f32, h: f32, r: u8, g: u8, b: u8) {
        // Black outline
        self.draw_box(x - 1.0, y - 1.0, w + 2.0, h + 2.0, 0, 0, 0);
        self.draw_box(x + 1.0, y + 1.0, w - 2.0, h - 2.0, 0, 0, 0);
        // Colored box
        self.draw_box(x, y, w, h, r, g, b);
    }
}
