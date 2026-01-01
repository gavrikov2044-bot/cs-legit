/*
 * Configuration System
 * Save/Load launcher settings
 */

#pragma once
#include <Windows.h>
#include <string>
#include <fstream>
#include <sstream>

namespace Config {
    
    // Config structure
    struct LauncherConfig {
        // General
        bool startWithWindows = false;
        bool minimizeToTray = true;
        bool autoUpdate = true;
        bool checkUpdatesOnStartup = true;
        
        // Injection
        int injectionMethod = 1; // 0=Manual, 1=Auto, 2=Manual Mapping
        float injectionDelay = 5.0f;
        
        // Display
        bool showFPS = false;
        bool showNotifications = true;
        int theme = 0; // 0=Dark Future, 1=Cyber, 2=Classic
        
        // Advanced
        bool enableWebSocket = true;
        bool verboseLogging = false;
        int statusRefreshInterval = 2; // seconds
        int updateCheckInterval = 30; // seconds
    };
    
    LauncherConfig g_config;
    
    // Get config file path
    std::string GetConfigPath() {
        char path[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path) == S_OK) {
            std::string appData(path);
            appData += "\\SingleProject";
            CreateDirectoryA(appData.c_str(), NULL);
            return appData + "\\config.ini";
        }
        return "config.ini";
    }
    
    // Parse INI line
    bool ParseLine(const std::string& line, std::string& key, std::string& value) {
        if (line.empty() || line[0] == ';' || line[0] == '#') return false;
        
        size_t pos = line.find('=');
        if (pos == std::string::npos) return false;
        
        key = line.substr(0, pos);
        value = line.substr(pos + 1);
        
        // Trim whitespace
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.pop_back();
        while (!key.empty() && (key[0] == ' ' || key[0] == '\t')) key.erase(0, 1);
        while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) value.erase(0, 1);
        
        return true;
    }
    
    // Convert string to bool
    bool ToBool(const std::string& str) {
        return str == "true" || str == "1" || str == "yes";
    }
    
    // Convert string to int
    int ToInt(const std::string& str) {
        try {
            return std::stoi(str);
        } catch (...) {
            return 0;
        }
    }
    
    // Convert string to float
    float ToFloat(const std::string& str) {
        try {
            return std::stof(str);
        } catch (...) {
            return 0.0f;
        }
    }
    
    // Load configuration from file
    bool Load() {
        std::string path = GetConfigPath();
        std::ifstream file(path);
        
        if (!file.is_open()) {
            // No config file, use defaults
            return false;
        }
        
        std::string line, key, value;
        while (std::getline(file, line)) {
            if (!ParseLine(line, key, value)) continue;
            
            // Parse config values
            if (key == "start_with_windows") {
                g_config.startWithWindows = ToBool(value);
            }
            else if (key == "minimize_to_tray") {
                g_config.minimizeToTray = ToBool(value);
            }
            else if (key == "auto_update") {
                g_config.autoUpdate = ToBool(value);
            }
            else if (key == "check_updates_on_startup") {
                g_config.checkUpdatesOnStartup = ToBool(value);
            }
            else if (key == "injection_method") {
                g_config.injectionMethod = ToInt(value);
            }
            else if (key == "injection_delay") {
                g_config.injectionDelay = ToFloat(value);
            }
            else if (key == "show_fps") {
                g_config.showFPS = ToBool(value);
            }
            else if (key == "show_notifications") {
                g_config.showNotifications = ToBool(value);
            }
            else if (key == "theme") {
                g_config.theme = ToInt(value);
            }
            else if (key == "enable_websocket") {
                g_config.enableWebSocket = ToBool(value);
            }
            else if (key == "verbose_logging") {
                g_config.verboseLogging = ToBool(value);
            }
            else if (key == "status_refresh_interval") {
                g_config.statusRefreshInterval = ToInt(value);
            }
            else if (key == "update_check_interval") {
                g_config.updateCheckInterval = ToInt(value);
            }
        }
        
        file.close();
        return true;
    }
    
    // Save configuration to file
    bool Save() {
        std::string path = GetConfigPath();
        std::ofstream file(path);
        
        if (!file.is_open()) {
            return false;
        }
        
        file << "; Launcher Configuration\n";
        file << "; Auto-generated - edit carefully\n\n";
        
        file << "[General]\n";
        file << "start_with_windows = " << (g_config.startWithWindows ? "true" : "false") << "\n";
        file << "minimize_to_tray = " << (g_config.minimizeToTray ? "true" : "false") << "\n";
        file << "auto_update = " << (g_config.autoUpdate ? "true" : "false") << "\n";
        file << "check_updates_on_startup = " << (g_config.checkUpdatesOnStartup ? "true" : "false") << "\n\n";
        
        file << "[Injection]\n";
        file << "injection_method = " << g_config.injectionMethod << " ; 0=Manual, 1=Auto, 2=Manual Mapping\n";
        file << "injection_delay = " << g_config.injectionDelay << " ; seconds\n\n";
        
        file << "[Display]\n";
        file << "show_fps = " << (g_config.showFPS ? "true" : "false") << "\n";
        file << "show_notifications = " << (g_config.showNotifications ? "true" : "false") << "\n";
        file << "theme = " << g_config.theme << " ; 0=Dark Future, 1=Cyber, 2=Classic\n\n";
        
        file << "[Advanced]\n";
        file << "enable_websocket = " << (g_config.enableWebSocket ? "true" : "false") << "\n";
        file << "verbose_logging = " << (g_config.verboseLogging ? "true" : "false") << "\n";
        file << "status_refresh_interval = " << g_config.statusRefreshInterval << " ; seconds\n";
        file << "update_check_interval = " << g_config.updateCheckInterval << " ; seconds\n";
        
        file.close();
        return true;
    }
    
    // Add to Windows startup
    bool AddToStartup() {
        HKEY hKey;
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        
        if (RegOpenKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            RegSetValueExA(hKey, "SingleProjectLauncher", 0, REG_SZ, (BYTE*)path, (DWORD)strlen(path) + 1);
            RegCloseKey(hKey);
            return true;
        }
        
        return false;
    }
    
    // Remove from Windows startup
    bool RemoveFromStartup() {
        HKEY hKey;
        
        if (RegOpenKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueA(hKey, "SingleProjectLauncher");
            RegCloseKey(hKey);
            return true;
        }
        
        return false;
    }
    
    // Apply config changes
    void Apply() {
        // Handle startup registry
        if (g_config.startWithWindows) {
            AddToStartup();
        } else {
            RemoveFromStartup();
        }
        
        // Save to file
        Save();
    }
}

