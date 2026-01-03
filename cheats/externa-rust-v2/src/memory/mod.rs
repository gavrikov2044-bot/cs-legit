pub mod driver;
pub mod driver_loader;
pub mod embedded_driver;
pub mod handle;
pub mod hyperv;
pub mod kdmapper;
pub mod scanner;
pub mod syscall;

use std::sync::Arc;
use parking_lot::Mutex;

/// Memory reader with multi-level support
/// Priority: 1) hyper-reV  2) Kernel Driver  3) Syscall  4) ReadProcessMemory
#[derive(Clone)]
pub struct Memory {
    pub handle: Arc<handle::SendHandle>,
    pub _pid: u32,
    pub client_base: usize,
    /// Optional kernel driver for ultra-fast reads
    pub driver: Option<Arc<Mutex<driver::DriverReader>>>,
    /// Optional hyper-reV for hypervisor-level reads (most undetectable)
    pub hyperv: Option<Arc<Mutex<hyperv::HyperVReader>>>,
}

impl Memory {
    /// Read memory using best available method
    /// Priority: hyper-reV > Driver > Syscall > WinAPI
    pub fn read_raw(&self, address: usize, buffer: &mut [u8]) -> bool {
        // Try hyper-reV first (most undetectable)
        if let Some(ref hv) = self.hyperv {
            let hv_guard = hv.lock();
            if hv_guard.is_available() {
                if hv_guard.read_virtual(address as u64, buffer) {
                    return true;
                }
            }
        }
        
        // Try driver (fastest kernel-level)
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

