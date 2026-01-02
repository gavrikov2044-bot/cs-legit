use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Instant;
use windows::core::{w, Result};
use windows::Win32::Foundation::{HWND, LPARAM, LRESULT, WPARAM};
use windows::Win32::Graphics::Direct3D::D3D_DRIVER_TYPE_HARDWARE;
use windows::Win32::Graphics::Direct3D11::{
    D3D11CreateDeviceAndSwapChain, ID3D11Device, ID3D11DeviceContext,
    ID3D11RenderTargetView, D3D11_SDK_VERSION, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
};
use windows::Win32::Graphics::Dxgi::{IDXGISwapChain, DXGI_SWAP_CHAIN_DESC, DXGI_SWAP_EFFECT_DISCARD, DXGI_USAGE_RENDER_TARGET_OUTPUT};
use windows::Win32::Graphics::Dxgi::Common::{DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_MODE_DESC, DXGI_RATIONAL};
use windows::Win32::UI::WindowsAndMessaging::{
    CreateWindowExW, RegisterClassExW, DefWindowProcW, ShowWindow,
    TranslateMessage, DispatchMessageW, PeekMessageW, SetLayeredWindowAttributes,
    WS_EX_TOPMOST, WS_EX_LAYERED, WS_EX_TRANSPARENT, WS_POPUP, WS_VISIBLE,
    PM_REMOVE, WM_QUIT, WNDCLASSEXW, CS_HREDRAW, CS_VREDRAW, MSG,
    LWA_ALPHA, SW_SHOWDEFAULT,
};
use windows::Win32::System::LibraryLoader::GetModuleHandleW;
use windows::Win32::Graphics::Dwm::DwmExtendFrameIntoClientArea;
use windows::Win32::UI::Controls::MARGINS;

use imgui::*;
use imgui_dx11_renderer::Renderer;

pub static RUNNING: AtomicBool = AtomicBool::new(true);

pub struct ImGuiOverlay {
    hwnd: HWND,
    device: ID3D11Device,
    context: ID3D11DeviceContext,
    swap_chain: IDXGISwapChain,
    render_target: ID3D11RenderTargetView,
    imgui: Context,
    renderer: Renderer,
    last_frame: Instant,
}

unsafe extern "system" fn wnd_proc(hwnd: HWND, msg: u32, wparam: WPARAM, lparam: LPARAM) -> LRESULT {
    if msg == WM_QUIT {
        RUNNING.store(false, Ordering::Relaxed);
    }
    DefWindowProcW(hwnd, msg, wparam, lparam)
}

impl ImGuiOverlay {
    pub fn new() -> Result<Self> {
        unsafe {
            let instance = GetModuleHandleW(None)?.into();
            let class_name = w!("ExternaImGui");

            let wc = WNDCLASSEXW {
                cbSize: std::mem::size_of::<WNDCLASSEXW>() as u32,
                style: CS_HREDRAW | CS_VREDRAW,
                lpfnWndProc: Some(wnd_proc),
                hInstance: instance,
                lpszClassName: class_name,
                ..Default::default()
            };
            RegisterClassExW(&wc);

            let hwnd = CreateWindowExW(
                WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT,
                class_name,
                w!("Externa"),
                WS_POPUP | WS_VISIBLE,
                0, 0, 1920, 1080,
                None, None, instance, None
            )?;

            SetLayeredWindowAttributes(hwnd, windows::Win32::Foundation::COLORREF(0), 255, LWA_ALPHA)?;

            let margins = MARGINS { cxLeftWidth: -1, cxRightWidth: -1, cyTopHeight: -1, cyBottomHeight: -1 };
            let _ = DwmExtendFrameIntoClientArea(hwnd, &margins);

            // D3D11
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
                Windowed: true.into(),
                SwapEffect: DXGI_SWAP_EFFECT_DISCARD,
                Flags: 0,
            };

            let mut device = None;
            let mut context = None;
            let mut swap_chain = None;

            D3D11CreateDeviceAndSwapChain(
                None,
                D3D_DRIVER_TYPE_HARDWARE,
                windows::Win32::Foundation::HMODULE(0),
                D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                None,
                D3D11_SDK_VERSION,
                Some(&sc_desc),
                Some(&mut swap_chain),
                Some(&mut device),
                None,
                Some(&mut context),
            )?;

            let device = device.unwrap();
            let context = context.unwrap();
            let swap_chain = swap_chain.unwrap();

            let back_buffer: windows::Win32::Graphics::Direct3D11::ID3D11Texture2D = swap_chain.GetBuffer(0)?;
            let render_target = device.CreateRenderTargetView(&back_buffer, None)?;

            // ImGui
            let mut imgui = Context::create();
            imgui.set_ini_filename(None);

            let renderer = Renderer::new(&mut imgui, &device)?;

            ShowWindow(hwnd, SW_SHOWDEFAULT);

            Ok(Self {
                hwnd,
                device,
                context,
                swap_chain,
                render_target,
                imgui,
                renderer,
                last_frame: Instant::now(),
            })
        }
    }

    pub fn render_frame<F>(&mut self, mut draw_fn: F)
    where F: FnMut(&Ui)
    {
        let now = Instant::now();
        let delta = now - self.last_frame;
        self.last_frame = now;

        let ui = self.imgui.frame();
        draw_fn(&ui);

        unsafe {
            self.context.OMSetRenderTargets(Some(&[Some(self.render_target.clone())]), None);
            let clear = [0.0f32, 0.0, 0.0, 0.0];
            self.context.ClearRenderTargetView(&self.render_target, &clear);

            self.renderer.render(ui.render()).ok();
            let _ = self.swap_chain.Present(1, 0);
        }
    }

    pub fn run<F>(&mut self, mut update_fn: F)
    where F: FnMut(&Ui)
    {
        let mut msg = MSG::default();

        while RUNNING.load(Ordering::Relaxed) {
            unsafe {
                while PeekMessageW(&mut msg, None, 0, 0, PM_REMOVE).as_bool() {
                    if msg.message == WM_QUIT {
                        RUNNING.store(false, Ordering::Relaxed);
                        return;
                    }
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
            }

            self.render_frame(&mut update_fn);
            std::thread::sleep(std::time::Duration::from_millis(8));
        }
    }
}
