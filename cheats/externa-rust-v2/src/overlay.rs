use std::ptr;
use std::sync::atomic::{AtomicBool, Ordering};
use windows::core::{Interface, w, Result, PCWSTR};
use windows::Win32::Foundation::{HWND, RECT, BOOL, LPARAM, WPARAM, LRESULT};
use windows::Win32::Graphics::Dxgi::{IDXGISwapChain, DXGI_SWAP_CHAIN_DESC, DXGI_SWAP_EFFECT_DISCARD, DXGI_USAGE_RENDER_TARGET_OUTPUT, IDXGISurface};
use windows::Win32::Graphics::Dxgi::Common::{DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_MODE_DESC, DXGI_RATIONAL};
use windows::Win32::Graphics::Direct3D11::{
    D3D11CreateDeviceAndSwapChain, ID3D11Device, ID3D11DeviceContext, ID3D11RenderTargetView, 
    D3D11_SDK_VERSION, D3D_DRIVER_TYPE_HARDWARE, D3D11_CREATE_DEVICE_FLAG, ID3D11Texture2D,
    D3D11_CREATE_DEVICE_BGRA_SUPPORT,
};
use windows::Win32::Graphics::Direct2D::{
    D2D1CreateFactory, ID2D1Factory, ID2D1RenderTarget, ID2D1SolidColorBrush,
    D2D1_FACTORY_TYPE_SINGLE_THREADED, D2D1_RENDER_TARGET_PROPERTIES, D2D1_PIXEL_FORMAT,
    D2D1_ALPHA_MODE_PREMULTIPLIED, D2D1_DEBUG_LEVEL_NONE, D2D1_RENDER_TARGET_TYPE_DEFAULT,
    D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT,
};
use windows::Win32::Graphics::Direct2D::Common::{D2D1_COLOR_F, D2D1_ALPHA_MODE_IGNORE};
use windows::Win32::UI::WindowsAndMessaging::{
    CreateWindowExW, RegisterClassExW, DefWindowProcW, ShowWindow, SetLayeredWindowAttributes,
    TranslateMessage, DispatchMessageW, PeekMessageW, PostQuitMessage,
    WS_EX_TOPMOST, WS_EX_LAYERED, WS_EX_TRANSPARENT, WS_POPUP, WS_VISIBLE, 
    PM_REMOVE, WM_QUIT, WNDCLASSEXW, CS_HREDRAW, CS_VREDRAW, MSG,
    GWL_EXSTYLE, SetWindowLongPtrW, GetWindowLongPtrW, LWA_COLORKEY,
    SetWindowDisplayAffinity, WDA_EXCLUDEFROMCAPTURE,
};
use windows::Win32::System::LibraryLoader::GetModuleHandleW;
use windows::Win32::Graphics::Dwm::{DwmExtendFrameIntoClientArea, DWM_BLURBEHIND, DWM_BB_ENABLE, DwmEnableBlurBehindWindow};
use windows::Win32::UI::Controls::MARGINS;

pub struct D3D11Overlay {
    hwnd: HWND,
    swap_chain: IDXGISwapChain,
    d2d_target: ID2D1RenderTarget,
    brush: ID2D1SolidColorBrush,
    factory: ID2D1Factory,
}

static RUNNING: AtomicBool = AtomicBool::new(true);

unsafe extern "system" fn wnd_proc(hwnd: HWND, msg: u32, wparam: WPARAM, lparam: LPARAM) -> LRESULT {
    if msg == WM_QUIT {
        RUNNING.store(false, Ordering::Relaxed);
    }
    DefWindowProcW(hwnd, msg, wparam, lparam)
}

impl D3D11Overlay {
    pub fn new() -> Result<Self> {
        unsafe {
            let instance = GetModuleHandleW(None)?;
            let class_name = w!("ExternaOverlayV2");
            
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
                w!("Externa Overlay"),
                WS_POPUP | WS_VISIBLE,
                0, 0, 1920, 1080,
                None, None, instance, None
            )?;
            
            // DWM Transparency
            let margins = MARGINS { cxLeftWidth: -1, cxRightWidth: -1, cyTopHeight: -1, cyBottomHeight: -1 };
            DwmExtendFrameIntoClientArea(hwnd, &margins)?;
            
            // Stream Proof
            let _ = SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);

            // Init D3D11
            let sc_desc = DXGI_SWAP_CHAIN_DESC {
                BufferDesc: DXGI_MODE_DESC {
                    Width: 1920,
                    Height: 1080,
                    RefreshRate: DXGI_RATIONAL { Numerator: 60, Denominator: 1 },
                    Format: DXGI_FORMAT_R8G8B8A8_UNORM,
                    ..Default::default()
                },
                SampleDesc: windows::Win32::Graphics::Dxgi::Common::DXGI_SAMPLE_DESC { Count: 1, Quality: 0 },
                BufferUsage: DXGI_USAGE_RENDER_TARGET_OUTPUT,
                BufferCount: 1,
                OutputWindow: hwnd,
                Windowed: BOOL(1),
                SwapEffect: DXGI_SWAP_EFFECT_DISCARD,
                Flags: 0,
            };

            let mut device: Option<ID3D11Device> = None;
            let mut context: Option<ID3D11DeviceContext> = None;
            let mut swap_chain: Option<IDXGISwapChain> = None;
            
            D3D11CreateDeviceAndSwapChain(
                None,
                D3D_DRIVER_TYPE_HARDWARE,
                None,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                None,
                D3D11_SDK_VERSION,
                Some(&sc_desc),
                Some(&mut swap_chain),
                Some(&mut device),
                None,
                Some(&mut context)
            )?;

            let swap_chain = swap_chain.unwrap();

            // Init D2D1
            let factory: ID2D1Factory = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, None)?;
            
            let surface: IDXGISurface = swap_chain.GetBuffer(0)?;
            
            let props = D2D1_RENDER_TARGET_PROPERTIES {
                r#type: D2D1_RENDER_TARGET_TYPE_DEFAULT,
                pixelFormat: D2D1_PIXEL_FORMAT {
                    format: DXGI_FORMAT_R8G8B8A8_UNORM,
                    alphaMode: D2D1_ALPHA_MODE_PREMULTIPLIED,
                },
                dpiX: 0.0,
                dpiY: 0.0,
                usage: D2D1_RENDER_TARGET_USAGE_NONE,
                minLevel: D2D1_FEATURE_LEVEL_DEFAULT,
            };

            let d2d_target = factory.CreateDxgiSurfaceRenderTarget(&surface, &props)?;
            let brush = d2d_target.CreateSolidColorBrush(&D2D1_COLOR_F { r: 1.0, g: 0.0, b: 0.0, a: 1.0 }, None)?;

            Ok(Self { hwnd, swap_chain, d2d_target, brush, factory })
        }
    }

    pub fn render_loop<F>(&self, mut draw_fn: F) 
    where F: FnMut(&ID2D1RenderTarget, &ID2D1SolidColorBrush) 
    {
        unsafe {
            let mut msg = MSG::default();
            while RUNNING.load(Ordering::Relaxed) {
                if PeekMessageW(&mut msg, None, 0, 0, PM_REMOVE).as_bool() {
                    if msg.message == WM_QUIT { break; }
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }

                self.d2d_target.BeginDraw();
                self.d2d_target.Clear(Some(&D2D1_COLOR_F { r: 0.0, g: 0.0, b: 0.0, a: 0.0 }));

                draw_fn(&self.d2d_target, &self.brush);

                let _ = self.d2d_target.EndDraw(None, None);
                let _ = self.swap_chain.Present(1, DXGI_PRESENT(0));
            }
        }
    }
}
