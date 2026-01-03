pub mod driver;
pub mod handle;
pub mod scanner;
pub mod syscall;

use std::sync::Arc;
use parking_lot::Mutex;

/// Memory reader with driver support
/// Priority: 1) Kernel Driver  2) Syscall  3) ReadProcessMemory
#[derive(Clone)]
pub struct Memory {
    pub handle: Arc<handle::SendHandle>,
    pub _pid: u32,
    pub client_base: usize,
    /// Optional kernel driver for ultra-fast reads
    pub driver: Option<Arc<Mutex<driver::DriverReader>>>,
}

impl Memory {
    /// Read memory using best available method
    pub fn read_raw(&self, address: usize, buffer: &mut [u8]) -> bool {
        // Try driver first (fastest)
        if let Some(ref drv) = self.driver {
            if drv.lock().read_raw(address, buffer) {
                return true;
            }
        }
        
        // Fallback to syscall/ReadProcessMemory
        <Self as handle::ProcessReader>::read_raw(self, address, buffer)
    }
    
    /// Generic read using best method
    pub fn read<T: Copy>(&self, address: usize) -> Option<T> {
        let mut buffer = std::mem::MaybeUninit::<T>::zeroed();
        unsafe {
            if self.read_raw(address, std::slice::from_raw_parts_mut(
                buffer.as_mut_ptr() as *mut u8, 
                std::mem::size_of::<T>()
            )) {
                Some(buffer.assume_init())
            } else {
                None
            }
        }
    }
}

