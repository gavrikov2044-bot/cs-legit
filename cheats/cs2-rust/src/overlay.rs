//! D3D11 Overlay
//! Based on C++ version architecture

use std::sync::Arc;
use std::mem;
use std::ptr;

use anyhow::{Result, anyhow};
use windows::core::{w, Interface};
use windows::Win32::Foundation::{HWND, LPARAM, LRESULT, WPARAM, RECT, BOOL};
use windows::Win32::Graphics::Gdi::UpdateWindow;
use windows::Win32::Graphics::Dwm::{
    DwmExtendFrameIntoClientArea, DwmEnableBlurBehindWindow,
    DWM_BLURBEHIND, DWM_BB_ENABLE, HRGN,
};
use windows::Win32::Graphics::Direct3D::D3D_DRIVER_TYPE_HARDWARE;
use windows::Win32::Graphics::Direct3D11::{
    D3D11CreateDeviceAndSwapChain, ID3D11Device, ID3D11DeviceContext,
    ID3D11RenderTargetView, ID3D11Texture2D,
    D3D11_CREATE_DEVICE_BGRA_SUPPORT, D3D11_SDK_VERSION,
};
use windows::Win32::Graphics::Dxgi::{
    IDXGISwapChain, DXGI_SWAP_CHAIN_DESC, DXGI_MODE_DESC,
    DXGI_SAMPLE_DESC, DXGI_USAGE_RENDER_TARGET_OUTPUT,
    DXGI_SWAP_EFFECT_DISCARD,
};
use windows::Win32::Graphics::Dxgi::Common::{
    DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
    DXGI_MODE_SCALING_UNSPECIFIED,
};
use windows::Win32::UI::Controls::MARGINS;
use windows::Win32::UI::WindowsAndMessaging::{
    CreateWindowExW, RegisterClassExW, DefWindowProcW, PostQuitMessage,
    ShowWindow, TranslateMessage, DispatchMessageW,
    GetSystemMetrics, GetWindowLongPtrW, SetWindowLongPtrW,
    SetForegroundWindow, PeekMessageW, SetWindowPos, SetWindowDisplayAffinity,
    WNDCLASSEXW, MSG,
    WS_POPUP, WS_VISIBLE,
    WS_EX_TOPMOST, WS_EX_TRANSPARENT, WS_EX_TOOLWINDOW, WS_EX_NOACTIVATE, WS_EX_LAYERED,
    WM_DESTROY, WM_NCHITTEST,
    SW_SHOWNOACTIVATE, HTTRANSPARENT, SM_CXSCREEN, SM_CYSCREEN,
    GWL_EXSTYLE, PM_REMOVE, HWND_TOPMOST,
    SWP_NOMOVE, SWP_NOSIZE, SWP_NOACTIVATE,
    CS_HREDRAW, CS_VREDRAW,
    WDA_EXCLUDEFROMCAPTURE, WDA_NONE,
    SetLayeredWindowAttributes, LWA_ALPHA,
};
use windows::Win32::UI::Input::KeyboardAndMouse::GetAsyncKeyState;

use crate::{SharedState, RUNNING, Settings};
use crate::esp::EspData;

/// D3D11 Overlay
pub struct Overlay {
    hwnd: HWND,
    width: i32,
    height: i32,
    state: Arc<SharedState>,
    
    // D3D11
    device: ID3D11Device,
    context: ID3D11DeviceContext,
    swapchain: IDXGISwapChain,
    render_target: ID3D11RenderTargetView,
}

static mut OVERLAY_STATE: Option<*const SharedState> = None;

impl Overlay {
    pub fn create(state: Arc<SharedState>) -> Result<Self> {
        let width = unsafe { GetSystemMetrics(SM_CXSCREEN) };
        let height = unsafe { GetSystemMetrics(SM_CYSCREEN) };
        
        unsafe { OVERLAY_STATE = Some(Arc::as_ptr(&state)); }
        
        // Create window
        let class_name = w!("ExternaD3D11");
        
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
                WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
                class_name,
                w!(""),
                WS_POPUP | WS_VISIBLE,
                0, 0, width, height,
                None, None, None, None,
            )?
        };
        
        // Make window transparent
        unsafe {
            SetLayeredWindowAttributes(hwnd, windows::Win32::Foundation::COLORREF(0), 255, LWA_ALPHA)?;
            
            // DWM transparency
            let margins = MARGINS {
                cxLeftWidth: -1,
                cxRightWidth: -1,
                cyTopHeight: -1,
                cyBottomHeight: -1,
            };
            let _ = DwmExtendFrameIntoClientArea(hwnd, &margins);
            
            let mut bb = DWM_BLURBEHIND::default();
            bb.dwFlags = DWM_BB_ENABLE;
            bb.fEnable = BOOL::from(true);
            bb.hRgnBlur = HRGN::default();
            let _ = DwmEnableBlurBehindWindow(hwnd, &bb);
            
            let _ = SetWindowPos(hwnd, Some(HWND_TOPMOST), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        
        // Create D3D11
        let swap_desc = DXGI_SWAP_CHAIN_DESC {
            BufferDesc: DXGI_MODE_DESC {
                Width: width as u32,
                Height: height as u32,
                RefreshRate: windows::Win32::Graphics::Dxgi::Common::DXGI_RATIONAL { Numerator: 60, Denominator: 1 },
                Format: DXGI_FORMAT_B8G8R8A8_UNORM,
                ScanlineOrdering: DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
                Scaling: DXGI_MODE_SCALING_UNSPECIFIED,
            },
            SampleDesc: DXGI_SAMPLE_DESC { Count: 1, Quality: 0 },
            BufferUsage: DXGI_USAGE_RENDER_TARGET_OUTPUT,
            BufferCount: 1,
            OutputWindow: hwnd,
            Windowed: BOOL::from(true),
            SwapEffect: DXGI_SWAP_EFFECT_DISCARD,
            Flags: 0,
        };
        
        let mut device: Option<ID3D11Device> = None;
        let mut context: Option<ID3D11DeviceContext> = None;
        let mut swapchain: Option<IDXGISwapChain> = None;
        
        unsafe {
            D3D11CreateDeviceAndSwapChain(
                None,
                D3D_DRIVER_TYPE_HARDWARE,
                None,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                None,
                D3D11_SDK_VERSION,
                Some(&swap_desc),
                Some(&mut swapchain),
                Some(&mut device),
                None,
                Some(&mut context),
            )?;
        }
        
        let device = device.ok_or_else(|| anyhow!("Failed to create D3D11 device"))?;
        let context = context.ok_or_else(|| anyhow!("Failed to create D3D11 context"))?;
        let swapchain = swapchain.ok_or_else(|| anyhow!("Failed to create swapchain"))?;
        
        // Create render target
        let back_buffer: ID3D11Texture2D = unsafe { swapchain.GetBuffer(0)? };
        let render_target = unsafe { device.CreateRenderTargetView(&back_buffer, None)? };
        
        log::info!("D3D11 Overlay created successfully!");
        
        Ok(Self {
            hwnd,
            width,
            height,
            state,
            device,
            context,
            swapchain,
            render_target,
        })
    }
    
    pub fn set_stream_proof(&mut self, enabled: bool) {
        unsafe {
            let affinity = if enabled { WDA_EXCLUDEFROMCAPTURE } else { WDA_NONE };
            if SetWindowDisplayAffinity(self.hwnd, affinity).is_ok() {
                log::info!("Stream-Proof: {}", if enabled { "ENABLED" } else { "DISABLED" });
            }
        }
    }
    
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
            
            // INSERT key
            if unsafe { GetAsyncKeyState(0x2D) } & 1 != 0 {
                let mut settings = self.state.settings.write();
                settings.menu_open = !settings.menu_open;
                
                let ex_style = unsafe { GetWindowLongPtrW(self.hwnd, GWL_EXSTYLE) } as u32;
                
                if settings.menu_open {
                    unsafe {
                        SetWindowLongPtrW(self.hwnd, GWL_EXSTYLE, (ex_style & !WS_EX_TRANSPARENT.0) as isize);
                        let _ = SetForegroundWindow(self.hwnd);
                    }
                    log::info!("Menu opened");
                } else {
                    unsafe {
                        SetWindowLongPtrW(self.hwnd, GWL_EXSTYLE, (ex_style | WS_EX_TRANSPARENT.0) as isize);
                    }
                    log::info!("Menu closed");
                }
            }
            
            self.render();
            
            std::thread::sleep(std::time::Duration::from_millis(8));
        }
        
        Ok(())
    }
    
    fn render(&self) {
        let settings = self.state.settings.read().clone();
        let esp_data = self.state.esp_data.read().clone();
        
        // Clear with transparent
        let clear_color = [0.0f32, 0.0, 0.0, 0.0];
        unsafe {
            self.context.OMSetRenderTargets(Some(&[Some(self.render_target.clone())]), None);
            self.context.ClearRenderTargetView(&self.render_target, &clear_color);
        }
        
        // Draw ESP
        self.draw_esp(&esp_data, &settings);
        
        // Draw menu
        if settings.menu_open {
            self.draw_menu();
        }
        
        // Present
        unsafe {
            let _ = self.swapchain.Present(1, 0);
        }
    }
    
    fn draw_esp(&self, data: &EspData, settings: &Settings) {
        if !settings.esp_enabled { return; }
        
        for entity in &data.entities {
            // World to screen
            let screen_pos = self.world_to_screen(&data.view_matrix, entity.origin);
            let head_pos = self.world_to_screen(&data.view_matrix, 
                glam::Vec3::new(entity.origin.x, entity.origin.y, entity.origin.z + 72.0));
            
            if let (Some(feet), Some(head)) = (screen_pos, head_pos) {
                let box_height = feet.y - head.y;
                let box_width = box_height / 2.0;
                let x = head.x - box_width / 2.0;
                let y = head.y;
                
                // Box ESP - render directly via D3D11 would need more setup
                // For now just log that we found entities
                log::debug!("Entity at screen ({}, {}), health: {}", x, y, entity.health);
            }
        }
    }
    
    fn draw_menu(&self) {
        // Menu would be rendered via D3D11
        // This requires vertex buffers, shaders etc.
        // For full implementation, we'd need ImGui or custom 2D renderer
        log::debug!("Drawing menu");
    }
    
    fn world_to_screen(&self, matrix: &[[f32; 4]; 4], pos: glam::Vec3) -> Option<glam::Vec2> {
        let w = matrix[3][0] * pos.x + matrix[3][1] * pos.y + matrix[3][2] * pos.z + matrix[3][3];
        
        if w < 0.001 {
            return None;
        }
        
        let x = matrix[0][0] * pos.x + matrix[0][1] * pos.y + matrix[0][2] * pos.z + matrix[0][3];
        let y = matrix[1][0] * pos.x + matrix[1][1] * pos.y + matrix[1][2] * pos.z + matrix[1][3];
        
        let screen_x = (self.width as f32 / 2.0) * (1.0 + x / w);
        let screen_y = (self.height as f32 / 2.0) * (1.0 - y / w);
        
        Some(glam::Vec2::new(screen_x, screen_y))
    }
}

unsafe extern "system" fn wnd_proc(hwnd: HWND, msg: u32, wparam: WPARAM, lparam: LPARAM) -> LRESULT {
    match msg {
        WM_DESTROY => {
            RUNNING.store(false, std::sync::atomic::Ordering::Relaxed);
            PostQuitMessage(0);
            LRESULT(0)
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
