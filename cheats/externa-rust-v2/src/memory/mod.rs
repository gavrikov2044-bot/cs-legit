pub mod handle;
pub mod scanner;
pub mod syscall;

use std::sync::Arc;
use windows::Win32::Foundation::HANDLE;

#[derive(Clone)]
pub struct Memory {
    pub handle: Arc<handle::SendHandle>,
    pub pid: u32,
    pub client_base: usize,
}

