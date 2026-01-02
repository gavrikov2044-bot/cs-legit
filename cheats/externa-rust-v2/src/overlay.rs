use std::sync::atomic::{AtomicBool, Ordering};
use windows::core::{w, Result};
use windows::Win32::Foundation::{HWND, LPARAM, LRESULT, WPARAM, RECT};
use windows::Win32::Graphics::Gdi::{
    GetDC, ReleaseDC, CreatePen, CreateSolidBrush, SelectObject, DeleteObject,
    Rectangle, FillRect, SetBkMode, TRANSPARENT, PS_SOLID, HGDIOBJ,
};
use windows::Win32::UI::WindowsAndMessaging::{
    CreateWindowExW, RegisterClassExW, DefWindowProcW, ShowWindow,
    TranslateMessage, DispatchMessageW, PeekMessageW,
    WS_EX_TOPMOST, WS_EX_LAYERED, WS_EX_TRANSPARENT, WS_POPUP, WS_VISIBLE,
    PM_REMOVE, WM_QUIT, WNDCLASSEXW, CS_HREDRAW, CS_VREDRAW, MSG,
    SW_SHOWDEFAULT, SetLayeredWindowAttributes, LWA_ALPHA,
};
use windows::Win32::System::LibraryLoader::GetModuleHandleW;

pub static RUNNING: AtomicBool = AtomicBool::new(true);

unsafe extern "system" fn wnd_proc(hwnd: HWND, msg: u32, wparam: WPARAM, lparam: LPARAM) -> LRESULT {
    if msg == WM_QUIT {
        RUNNING.store(false, Ordering::Relaxed);
    }
    DefWindowProcW(hwnd, msg, wparam, lparam)
}

pub struct Overlay {
    hwnd: HWND,
}

impl Overlay {
    pub fn new() -> Result<Self> {
        unsafe {
            let instance = GetModuleHandleW(None)?;
            let class_name = w!("ExternaGDI");

            let wc = WNDCLASSEXW {
                cbSize: std::mem::size_of::<WNDCLASSEXW>() as u32,
                style: CS_HREDRAW | CS_VREDRAW,
                lpfnWndProc: Some(wnd_proc),
                hInstance: instance.into(),
                lpszClassName: class_name,
                ..Default::default()
            };
            RegisterClassExW(&wc);

            let hwnd = CreateWindowExW(
                WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT,
                class_name,
                w!(""),
                WS_POPUP | WS_VISIBLE,
                0, 0, 1920, 1080,
                None, None, Some(instance.into()), None
            )?;

            SetLayeredWindowAttributes(hwnd, windows::Win32::Foundation::COLORREF(0), 255, LWA_ALPHA)?;
            ShowWindow(hwnd, SW_SHOWDEFAULT);

            Ok(Self { hwnd })
        }
    }

    pub fn draw_box(&self, x: f32, y: f32, w: f32, h: f32, r: u8, g: u8, b: u8) {
        unsafe {
            let hdc = GetDC(Some(self.hwnd));
            SetBkMode(hdc, TRANSPARENT);

            let pen = CreatePen(PS_SOLID, 2, windows::Win32::Foundation::COLORREF(
                r as u32 | ((g as u32) << 8) | ((b as u32) << 16)
            ));
            let old_pen = SelectObject(hdc, HGDIOBJ(pen.0));

            Rectangle(hdc, x as i32, y as i32, (x + w) as i32, (y + h) as i32);

            SelectObject(hdc, old_pen);
            DeleteObject(HGDIOBJ(pen.0));
            ReleaseDC(Some(self.hwnd), hdc);
        }
    }

    pub fn draw_filled_rect(&self, x: f32, y: f32, w: f32, h: f32, r: u8, g: u8, b: u8) {
        unsafe {
            let hdc = GetDC(Some(self.hwnd));

            let brush = CreateSolidBrush(windows::Win32::Foundation::COLORREF(
                r as u32 | ((g as u32) << 8) | ((b as u32) << 16)
            ));

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
}
