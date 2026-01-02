use std::ptr;
use windows::core::{Interface, w, Result};
use windows::Win32::Foundation::{HWND, RECT, BOOL, LPARAM, WPARAM, LRESULT};
use windows::Win32::Graphics::Dxgi::{IDXGISwapChain, DXGI_SWAP_CHAIN_DESC, DXGI_SWAP_EFFECT_DISCARD, DXGI_USAGE_RENDER_TARGET_OUTPUT};
use windows::Win32::Graphics::Dxgi::Common::{DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_MODE_DESC, DXGI_RATIONAL};
use windows::Win32::Graphics::Direct3D11::{
    D3D11CreateDeviceAndSwapChain, ID3D11Device, ID3D11DeviceContext, ID3D11RenderTargetView, 
    D3D11_SDK_VERSION, D3D_DRIVER_TYPE_HARDWARE, D3D11_CREATE_DEVICE_FLAG, ID3D11Texture2D,
};
use windows::Win32::Graphics::Direct3D::{D3D_DRIVER_TYPE, D3D_FEATURE_LEVEL};
use windows::Win32::UI::WindowsAndMessaging::{
    CreateWindowExW, RegisterClassExW, DefWindowProcW, ShowWindow, SetLayeredWindowAttributes,
    TranslateMessage, DispatchMessageW, PeekMessageW, PostQuitMessage,
    WS_EX_TOPMOST, WS_EX_LAYERED, WS_EX_TRANSPARENT, WS_POPUP, WS_VISIBLE, 
    PM_REMOVE, WM_QUIT, WNDCLASSEXW, CS_HREDRAW, CS_VREDRAW, MSG,
    GWL_EXSTYLE, SetWindowLongPtrW, GetWindowLongPtrW,
};
use windows::Win32::System::LibraryLoader::GetModuleHandleW;

pub struct D3D11Overlay {
    hwnd: HWND,
    device: ID3D11Device,
    context: ID3D11DeviceContext,
    swap_chain: IDXGISwapChain,
    render_target: ID3D11RenderTargetView,
}

static mut RUNNING: bool = true;

unsafe extern "system" fn wnd_proc(hwnd: HWND, msg: u32, wparam: WPARAM, lparam: LPARAM) -> LRESULT {
    if msg == WM_QUIT {
        RUNNING = false;
    }
    DefWindowProcW(hwnd, msg, wparam, lparam)
}

impl D3D11Overlay {
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
                ..Default::default()
            };
            RegisterClassExW(&wc);

            // Create Transparent Window
            let hwnd = CreateWindowExW(
                WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT,
                class_name,
                w!("Externa Overlay"),
                WS_POPUP | WS_VISIBLE,
                0, 0, 1920, 1080, // Fullscreen
                None, None, instance, None
            );
            
            SetLayeredWindowAttributes(hwnd, 0, 0, windows::Win32::UI::WindowsAndMessaging::LWA_COLORKEY);

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
                D3D11_CREATE_DEVICE_FLAG(0),
                None,
                D3D11_SDK_VERSION,
                &sc_desc,
                &mut swap_chain,
                &mut device,
                None,
                &mut context
            )?;

            let device = device.unwrap();
            let context = context.unwrap();
            let swap_chain = swap_chain.unwrap();

            // Create Render Target
            let back_buffer: ID3D11Texture2D = swap_chain.GetBuffer(0)?;
            let render_target = device.CreateRenderTargetView(&back_buffer, None)?;

            Ok(Self { hwnd, device, context, swap_chain, render_target })
        }
    }

    pub fn render_loop<F>(&self, mut draw_fn: F) 
    where F: FnMut(&ID3D11DeviceContext, &ID3D11RenderTargetView) 
    {
        unsafe {
            let mut msg = MSG::default();
            while RUNNING {
                if PeekMessageW(&mut msg, None, 0, 0, PM_REMOVE).as_bool() {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }

                // Clear screen
                let color = [0.0, 0.0, 0.0, 0.0]; // Transparent
                self.context.ClearRenderTargetView(&self.render_target, &color);

                // Draw callback
                draw_fn(&self.context, &self.render_target);

                // Present
                self.swap_chain.Present(1, 0).unwrap();
            }
        }
    }
}

