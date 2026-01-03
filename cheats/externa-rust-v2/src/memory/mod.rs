pub mod handle;
pub mod scanner;
pub mod syscall;

use std::sync::Arc;

#[derive(Clone)]
pub struct Memory {
    pub handle: Arc<handle::SendHandle>,
    pub _pid: u32,
    pub client_base: usize,
}

