//! Configuration module
//! Saves and loads settings from JSON file

use std::fs;
use std::path::PathBuf;

use anyhow::Result;

/// Config file path
fn config_path() -> PathBuf {
    let mut path = std::env::current_exe().unwrap_or_default();
    path.set_file_name("externa_config.json");
    path
}

/// Serializable settings
#[derive(Clone, Debug)]
pub struct Config {
    pub esp_enabled: bool,
    pub box_esp: bool,
    pub health_bar: bool,
    pub armor_bar: bool,
    pub stream_proof: bool,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            esp_enabled: true,
            box_esp: true,
            health_bar: true,
            armor_bar: true,
            stream_proof: true,
        }
    }
}

impl Config {
    /// Load from file or create default
    pub fn load() -> Self {
        match fs::read_to_string(config_path()) {
            Ok(content) => Self::parse(&content).unwrap_or_default(),
            Err(_) => {
                let config = Self::default();
                let _ = config.save();
                config
            }
        }
    }
    
    /// Save to file
    pub fn save(&self) -> Result<()> {
        let content = format!(
            r#"{{
  "esp_enabled": {},
  "box_esp": {},
  "health_bar": {},
  "armor_bar": {},
  "stream_proof": {}
}}"#,
            self.esp_enabled,
            self.box_esp,
            self.health_bar,
            self.armor_bar,
            self.stream_proof
        );
        
        fs::write(config_path(), content)?;
        log::info!("Config saved to {:?}", config_path());
        Ok(())
    }
    
    /// Parse JSON manually (no serde dependency)
    fn parse(content: &str) -> Option<Self> {
        let mut config = Self::default();
        
        for line in content.lines() {
            let line = line.trim();
            if line.contains("esp_enabled") {
                config.esp_enabled = line.contains("true");
            } else if line.contains("box_esp") {
                config.box_esp = line.contains("true");
            } else if line.contains("health_bar") {
                config.health_bar = line.contains("true");
            } else if line.contains("armor_bar") {
                config.armor_bar = line.contains("true");
            } else if line.contains("stream_proof") {
                config.stream_proof = line.contains("true");
            }
        }
        
        Some(config)
    }
}

