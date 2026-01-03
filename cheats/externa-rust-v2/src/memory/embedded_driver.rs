//! Embedded Driver - Single EXE Solution
//! 
//! Драйвер встроен прямо в .exe как байты
//! При запуске: извлекается → загружается → удаляется
//! Если не сработает → автоматически syscall fallback
//!
//! Схема:
//! ┌─────────────────────────────────────────────┐
//! │  Externa-CS2.exe (содержит драйверы)        │
//! │  ┌─────────────────────────────────────┐    │
//! │  │ laithdriver.sys (встроен)           │    │
//! │  │ iqvw64e.sys (встроен)               │    │
//! │  └─────────────────────────────────────┘    │
//! └─────────────────────────────────────────────┘
//!              ↓ При запуске
//! ┌─────────────────────────────────────────────┐
//! │  1. Извлечь в %TEMP%                        │
//! │  2. Загрузить через kdmapper                │
//! │  3. Удалить файлы                           │
//! │  4. Работать через драйвер                  │
//! │     ИЛИ fallback на syscall                 │
//! └─────────────────────────────────────────────┘

use std::fs;
use std::io::Write;
use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, Ordering};

// ============================================================================
// Embedded Driver Bytes
// ============================================================================

/// Флаг: встроены ли драйверы при компиляции
/// Если assets/laithdriver.sys существует при сборке - включаются
#[cfg(feature = "embed_drivers")]
const LAITH_DRIVER: &[u8] = include_bytes!("../../assets/laithdriver.sys");

#[cfg(feature = "embed_drivers")]
const INTEL_DRIVER: &[u8] = include_bytes!("../../assets/iqvw64e.sys");

/// Placeholder если драйверы не встроены
#[cfg(not(feature = "embed_drivers"))]
const LAITH_DRIVER: &[u8] = &[];

#[cfg(not(feature = "embed_drivers"))]
const INTEL_DRIVER: &[u8] = &[];

/// Глобальный статус: драйвер загружен?
static DRIVER_LOADED: AtomicBool = AtomicBool::new(false);

// ============================================================================
// Embedded Driver Manager
// ============================================================================

pub struct EmbeddedDriver {
    temp_dir: PathBuf,
    laith_path: PathBuf,
    intel_path: PathBuf,
    cleanup_on_drop: bool,
}

impl EmbeddedDriver {
    /// Проверить есть ли встроенные драйверы
    pub fn has_embedded_drivers() -> bool {
        !LAITH_DRIVER.is_empty() && !INTEL_DRIVER.is_empty()
    }
    
    /// Создать менеджер драйверов
    pub fn new() -> Self {
        let temp_dir = std::env::temp_dir().join("externa_drv");
        let laith_path = temp_dir.join("laithdriver.sys");
        let intel_path = temp_dir.join("iqvw64e.sys");
        
        Self {
            temp_dir,
            laith_path,
            intel_path,
            cleanup_on_drop: true,
        }
    }
    
    /// Главная функция: загрузить драйвер и вернуть результат
    /// 
    /// Возвращает:
    /// - Ok(true) = драйвер загружен, используем kernel mode
    /// - Ok(false) = драйвер не загружен, используем syscall
    /// - Err = критическая ошибка
    pub fn load(&mut self) -> Result<bool, String> {
        log::info!("[Embedded] ══════════════════════════════════════");
        log::info!("[Embedded] Single-EXE Driver Loader");
        log::info!("[Embedded] ══════════════════════════════════════");
        
        // Проверяем: драйвер уже загружен?
        if super::driver::DriverReader::connect().is_some() {
            log::info!("[Embedded] Driver already loaded - using kernel mode!");
            DRIVER_LOADED.store(true, Ordering::SeqCst);
            return Ok(true);
        }
        
        // Проверяем: есть встроенные драйверы?
        if !Self::has_embedded_drivers() {
            log::info!("[Embedded] No embedded drivers - trying external files...");
            return self.try_external_drivers();
        }
        
        log::info!("[Embedded] Extracting embedded drivers...");
        log::info!("[Embedded]   laithdriver.sys: {} bytes", LAITH_DRIVER.len());
        log::info!("[Embedded]   iqvw64e.sys: {} bytes", INTEL_DRIVER.len());
        
        // Создаём временную папку
        if let Err(e) = fs::create_dir_all(&self.temp_dir) {
            log::warn!("[Embedded] Failed to create temp dir: {}", e);
            return Ok(false); // Fallback to syscall
        }
        
        // Извлекаем драйверы
        if let Err(e) = self.extract_drivers() {
            log::warn!("[Embedded] Failed to extract drivers: {}", e);
            return Ok(false);
        }
        
        // Загружаем через kdmapper
        log::info!("[Embedded] Loading driver via kdmapper...");
        match super::kdmapper::load_driver(&self.laith_path.to_string_lossy()) {
            Ok(_) => {
                log::info!("[Embedded] ✓ Driver loaded successfully!");
                DRIVER_LOADED.store(true, Ordering::SeqCst);
                
                // Удаляем файлы сразу после загрузки (безопасность)
                self.cleanup_files();
                
                Ok(true)
            }
            Err(e) => {
                log::warn!("[Embedded] KDMapper failed: {}", e);
                log::info!("[Embedded] Falling back to syscall mode");
                
                self.cleanup_files();
                Ok(false)
            }
        }
    }
    
    /// Извлечь драйверы в temp
    fn extract_drivers(&self) -> Result<(), String> {
        // laithdriver.sys
        let mut file = fs::File::create(&self.laith_path)
            .map_err(|e| format!("Create laithdriver.sys: {}", e))?;
        file.write_all(LAITH_DRIVER)
            .map_err(|e| format!("Write laithdriver.sys: {}", e))?;
        
        // iqvw64e.sys
        let mut file = fs::File::create(&self.intel_path)
            .map_err(|e| format!("Create iqvw64e.sys: {}", e))?;
        file.write_all(INTEL_DRIVER)
            .map_err(|e| format!("Write iqvw64e.sys: {}", e))?;
        
        log::info!("[Embedded] Drivers extracted to: {:?}", self.temp_dir);
        Ok(())
    }
    
    /// Попробовать загрузить внешние файлы драйверов
    fn try_external_drivers(&self) -> Result<bool, String> {
        let exe_dir = std::env::current_exe()
            .ok()
            .and_then(|p| p.parent().map(|p| p.to_path_buf()))
            .unwrap_or_default();
        
        // Ищем laithdriver.sys
        let driver_paths = [
            exe_dir.join("laithdriver.sys"),
            exe_dir.join("driver/laithdriver.sys"),
            std::env::current_dir().unwrap_or_default().join("laithdriver.sys"),
        ];
        
        let driver_path = driver_paths.iter().find(|p| p.exists());
        
        if let Some(driver_path) = driver_path {
            log::info!("[Embedded] Found external driver: {:?}", driver_path);
            
            // Ищем iqvw64e.sys
            let intel_paths = [
                exe_dir.join("iqvw64e.sys"),
                exe_dir.join("driver/iqvw64e.sys"),
                std::env::current_dir().unwrap_or_default().join("iqvw64e.sys"),
            ];
            
            if intel_paths.iter().any(|p| p.exists()) {
                log::info!("[Embedded] Intel driver found, using kdmapper...");
                match super::kdmapper::load_driver(&driver_path.to_string_lossy()) {
                    Ok(_) => {
                        log::info!("[Embedded] ✓ External driver loaded!");
                        DRIVER_LOADED.store(true, Ordering::SeqCst);
                        return Ok(true);
                    }
                    Err(e) => {
                        log::warn!("[Embedded] KDMapper failed: {}", e);
                    }
                }
            }
        }
        
        log::info!("[Embedded] No drivers available - using syscall mode");
        Ok(false)
    }
    
    /// Удалить временные файлы
    fn cleanup_files(&self) {
        let _ = fs::remove_file(&self.laith_path);
        let _ = fs::remove_file(&self.intel_path);
        let _ = fs::remove_dir(&self.temp_dir);
        log::debug!("[Embedded] Temp files cleaned up");
    }
    
    /// Проверить загружен ли драйвер
    pub fn is_loaded() -> bool {
        DRIVER_LOADED.load(Ordering::SeqCst)
    }
}

impl Drop for EmbeddedDriver {
    fn drop(&mut self) {
        if self.cleanup_on_drop {
            self.cleanup_files();
        }
    }
}

// ============================================================================
// Simple API
// ============================================================================

/// Простая функция для main.rs
/// Возвращает true если драйвер загружен
pub fn auto_load_driver() -> bool {
    let mut loader = EmbeddedDriver::new();
    match loader.load() {
        Ok(loaded) => {
            if loaded {
                log::info!("[Driver] ✓ Kernel driver active - ULTRA-FAST mode!");
            } else {
                log::info!("[Driver] Using syscall mode (still fast!)");
            }
            loaded
        }
        Err(e) => {
            log::error!("[Driver] Error: {}", e);
            false
        }
    }
}

/// Проверить режим работы
pub fn get_mode() -> &'static str {
    if DRIVER_LOADED.load(Ordering::SeqCst) {
        "Kernel Driver (ultra-fast)"
    } else if super::syscall::is_active() {
        "Direct Syscall (fast)"
    } else {
        "WinAPI (fallback)"
    }
}

