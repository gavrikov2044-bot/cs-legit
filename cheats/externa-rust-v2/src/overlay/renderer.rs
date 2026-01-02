use anyhow::Result;
use windows::core::HSTRING;
use windows::Win32::Foundation::{HWND, RECT};
use windows::Win32::Graphics::Direct2D::ID2D1RenderTarget;
use windows::Win32::Graphics::Direct2D::{ID2D1Factory, ID2D1HwndRenderTarget, ID2D1SolidColorBrush,
    D2D1CreateFactory, D2D1_FACTORY_TYPE_SINGLE_THREADED,
    D2D1_RENDER_TARGET_PROPERTIES, D2D1_HWND_RENDER_TARGET_PROPERTIES,
    D2D1_PRESENT_OPTIONS_NONE, D2D1_DEBUG_LEVEL_NONE,
    D2D1_FACTORY_OPTIONS,
};
use windows::Win32::Graphics::Dxgi::Common::DXGI_FORMAT_B8G8R8A8_UNORM;
use windows::Win32::Graphics::Direct2D::Common::{D2D1_ALPHA_MODE_PREMULTIPLIED, D2D1_PIXEL_FORMAT, D2D1_COLOR_F, D2D_SIZE_U}; // Added D2D_SIZE_U
use windows::Win32::UI::WindowsAndMessaging::{
    CreateWindowExW, RegisterClassExW, DefWindowProcW, ShowWindow,
    WS_EX_TOPMOST, WS_EX_LAYERED, WS_EX_TRANSPARENT, WS_EX_NOACTIVATE, WS_POPUP, WS_VISIBLE,
    WNDCLASSEXW, CS_HREDRAW, CS_VREDRAW, SW_SHOWDEFAULT, SetLayeredWindowAttributes, LWA_ALPHA,
    PeekMessageW, TranslateMessage, DispatchMessageW, PM_REMOVE, MSG,
};
use windows::Win32::System::LibraryLoader::GetModuleHandleW;

pub struct Direct2DOverlay {
    pub hwnd: HWND,
    factory: ID2D1Factory,
    target: ID2D1HwndRenderTarget,
    brush_enemy: ID2D1SolidColorBrush,
    brush_team: ID2D1SolidColorBrush,
    pub width: u32,
    pub height: u32,
}

unsafe extern "system" fn wnd_proc(hwnd: HWND, msg: u32, wparam: windows::Win32::Foundation::WPARAM, lparam: windows::Win32::Foundation::LPARAM) -> windows::Win32::Foundation::LRESULT {
    if msg == 0x0010 { // WM_CLOSE
        std::process::exit(0);
    }
    DefWindowProcW(hwnd, msg, wparam, lparam)
}

impl Direct2DOverlay {
    pub fn new() -> Result<Self> {
        unsafe {
            let instance = GetModuleHandleW(None)?;
            let class_name = HSTRING::from("ExternaOverlayD2D");

            let wc = WNDCLASSEXW {
                cbSize: std::mem::size_of::<WNDCLASSEXW>() as u32,
                style: CS_HREDRAW | CS_VREDRAW,
                lpfnWndProc: Some(wnd_proc),
                hInstance: instance.into(),
                lpszClassName: windows::core::PCWSTR::from_raw(class_name.as_ptr()),
                ..Default::default()
            };
            RegisterClassExW(&wc);

            let width = 1920; // Auto-detect later
            let height = 1080;

            let hwnd = CreateWindowExW(
                WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
                &class_name,
                &HSTRING::from(""),
                WS_POPUP | WS_VISIBLE,
                0, 0, width as i32, height as i32,
                None, None, Some(instance.into()), None
            )?;

            SetLayeredWindowAttributes(hwnd, windows::Win32::Foundation::COLORREF(0), 255, LWA_ALPHA)?;
            ShowWindow(hwnd, SW_SHOWDEFAULT);

            // Init D2D
            let factory_options = D2D1_FACTORY_OPTIONS { debugLevel: D2D1_DEBUG_LEVEL_NONE };
            let factory: ID2D1Factory = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, Some(&factory_options))?;
            
            let rc = RECT { left: 0, top: 0, right: width as i32, bottom: height as i32 };
            let render_props = D2D1_RENDER_TARGET_PROPERTIES {
                pixelFormat: D2D1_PIXEL_FORMAT {
                    format: DXGI_FORMAT_B8G8R8A8_UNORM,
                    alphaMode: D2D1_ALPHA_MODE_PREMULTIPLIED,
                },
                ..Default::default()
            };
            let hwnd_props = D2D1_HWND_RENDER_TARGET_PROPERTIES {
                hwnd,
                pixelSize: D2D_SIZE_U { width, height }, // Use D2D_SIZE_U
                presentOptions: D2D1_PRESENT_OPTIONS_NONE,
            };

            let target = factory.CreateHwndRenderTarget(&render_props, &hwnd_props)?;
            
            // Create brushes directly on the HWND render target
            // Cast to ID2D1RenderTarget interface to access CreateSolidColorBrush
            let render_target: ID2D1RenderTarget = target.cast()?;
            let brush_enemy = render_target.CreateSolidColorBrush(&D2D1_COLOR_F { r: 1.0, g: 0.0, b: 0.0, a: 1.0 }, None)?;
            let brush_team = render_target.CreateSolidColorBrush(&D2D1_COLOR_F { r: 0.0, g: 1.0, b: 0.0, a: 1.0 }, None)?;

            Ok(Self { hwnd, factory, target, brush_enemy, brush_team, width, height })
        }
    }

    pub fn begin_scene(&self) {
        unsafe {
            self.target.BeginDraw();
            self.target.Clear(Some(&windows::Win32::Graphics::Direct2D::Common::D2D1_COLOR_F { r: 0.0, g: 0.0, b: 0.0, a: 0.0 }));
        }
    }

    pub fn end_scene(&self) {
        unsafe {
            let _ = self.target.EndDraw(None, None);
        }
    }

    pub fn draw_line(&self, x1: f32, y1: f32, x2: f32, y2: f32, enemy: bool) {
        unsafe {
            let brush = if enemy { &self.brush_enemy } else { &self.brush_team };
            let p1 = windows::Win32::Graphics::Direct2D::Common::D2D_POINT_2F { x: x1, y: y1 };
            let p2 = windows::Win32::Graphics::Direct2D::Common::D2D_POINT_2F { x: x2, y: y2 };
            self.target.DrawLine(p1, p2, brush, 1.5, None);
        }
    }

    pub fn draw_box(&self, x: f32, y: f32, w: f32, h: f32, enemy: bool) {
        unsafe {
            let brush = if enemy { &self.brush_enemy } else { &self.brush_team };
            let rect = windows::Win32::Graphics::Direct2D::Common::D2D_RECT_F {
                left: x, top: y, right: x + w, bottom: y + h
            };
            self.target.DrawRectangle(&rect, brush, 1.5, None);
        }
    }

    pub fn handle_message(&self) -> bool {
        unsafe {
            let mut msg = MSG::default();
            if PeekMessageW(&mut msg, None, 0, 0, PM_REMOVE).as_bool() {
                if msg.message == 0x0012 { return false; }
                let _ = TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            true
        }
    }
}

