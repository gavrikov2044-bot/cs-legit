/*
 * Single-Project Launcher v1.0.0
 * Premium Gaming Software
 */

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <TlHelp32.h>
#include <shellapi.h>
#include <ShlObj.h>
#include <WinInet.h>
#include <winhttp.h>
#include <string>
#include <filesystem>
#include <random>
#include <chrono>
#include <thread>
#include <cmath>
#include <fstream>
#include <atomic>
#include <vector>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "winhttp.lib")

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "protection.hpp"
#include "modern_theme.hpp"
#include "new_ui.hpp"

namespace fs = std::filesystem;

// ============================================
// Protection flags
// ============================================
bool g_protectionPassed = false;
bool g_bypassVM = false; // Set to true for development

// ============================================
// Configuration
// ============================================
#define PROJECT_NAME "Single-Project"

// LAUNCHER_VERSION is passed from CMake during CI build
// If not defined (local build), use fallback
#ifndef LAUNCHER_VERSION
    #define LAUNCHER_VERSION "2.0.50-local"
#endif

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 700
// Rebuild trigger v2

// Use direct IP for fast connection (Host header added for nginx)
#define SERVER_HOST "138.124.0.8"
#define SERVER_HOST_HEADER "single-project.duckdns.org"
#define SERVER_PORT 80

// ============================================
// Game Data
// ============================================
struct GameInfo {
    std::string id;
    std::string name;
    std::string icon;
    std::string processName;
    std::string description;
    bool hasLicense;
    int daysRemaining;
    std::string version;
    bool available;
};

std::vector<GameInfo> g_games = {
    {"cs2", "Counter-Strike 2", "CS", "cs2.exe", "ESP (Box, Health, Name, Distance)", false, 0, "1.0.0", true},
    {"dayz", "DayZ", "DZ", "DayZ_x64.exe", "In development", false, 0, "---", false},
    {"rust", "Rust", "Rust", "RustClient.exe", "In development", false, 0, "---", false},
    {"apex", "Apex Legends", "AP", "r5apex.exe", "In development", false, 0, "---", false}
};

int g_selectedGame = 0;

// ============================================
// App State
// ============================================
enum class Screen { Splash, Login, Register, Main };

Screen g_currentScreen = Screen::Splash;
char g_username[64] = "";
char g_password[64] = "";
char g_licenseKey[128] = "";
char g_activateKey[128] = "";
bool g_rememberLogin = true;
std::string g_errorMsg = "";
std::string g_successMsg = "";
std::string g_statusMsg = "";
std::string g_token = "";

std::atomic<bool> g_isLoading{false};
std::atomic<float> g_downloadProgress{0.0f};
std::atomic<bool> g_isDownloading{false};
std::atomic<bool> g_cheatRunning{false};

// Game status from server
std::string g_gameStatus = "operational";
std::string g_gameStatusMsg = "Working";
std::atomic<bool> g_statusRefreshing{false};
std::chrono::steady_clock::time_point g_lastRefresh;

float g_animTimer = 0.0f;
float g_splashTimer = 0.0f;
float g_fadeAlpha = 0.0f;

std::string g_displayName = "";
std::string g_hwid = "";

// ============================================
// Toast Notification System
// ============================================
struct Toast {
    std::string message;
    ImVec4 color;
    float lifetime;
    float alpha;
    bool isError;
};

std::vector<Toast> g_toasts;

void ShowToast(const std::string& message, bool isError = false) {
    Toast toast;
    toast.message = message;
    toast.color = isError ? ImVec4(0.95f, 0.25f, 0.30f, 1.0f) : ImVec4(0.15f, 0.85f, 0.45f, 1.0f);
    toast.lifetime = 3.0f;
    toast.alpha = 1.0f;
    toast.isError = isError;
    g_toasts.push_back(toast);
}

void UpdateToasts(float dt) {
    for (auto it = g_toasts.begin(); it != g_toasts.end();) {
        it->lifetime -= dt;
        if (it->lifetime < 0.5f) {
            it->alpha = it->lifetime / 0.5f;
        }
        if (it->lifetime <= 0) {
            it = g_toasts.erase(it);
        } else {
            ++it;
        }
    }
}

void RenderToasts() {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImVec2 ws = ImGui::GetIO().DisplaySize;
    
    float y = ws.y - 80.0f;
    for (auto& toast : g_toasts) {
        std::string text = toast.message;
        ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
        float width = textSize.x + 40.0f;
        float height = 50.0f;
        float x = ws.x - width - 20.0f;
        
        ImU32 bgCol = ImGui::ColorConvertFloat4ToU32(ImVec4(0.08f, 0.08f, 0.12f, 0.95f * toast.alpha));
        ImU32 borderCol = ImGui::ColorConvertFloat4ToU32(ImVec4(toast.color.x, toast.color.y, toast.color.z, toast.alpha));
        ImU32 textCol = ImGui::ColorConvertFloat4ToU32(ImVec4(0.95f, 0.95f, 0.98f, toast.alpha));
        
        // Shadow
        dl->AddRectFilled(ImVec2(x + 2, y + 2), ImVec2(x + width + 2, y + height + 2), IM_COL32(0, 0, 0, (int)(60 * toast.alpha)), 8.0f);
        
        // Background
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + height), bgCol, 8.0f);
        
        // Left accent bar
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + 4, y + height), borderCol, 8.0f, ImDrawFlags_RoundCornersLeft);
        
        // Border
        dl->AddRect(ImVec2(x, y), ImVec2(x + width, y + height), borderCol, 8.0f, 0, 1.5f);
        
        // Icon
        const char* icon = toast.isError ? "X" : "✓";
        dl->AddText(ImVec2(x + 15, y + 15), borderCol, icon);
        
        // Text
        dl->AddText(ImVec2(x + 35, y + 15), textCol, text.c_str());
        
        y -= height + 10.0f;
    }
}

// ============================================
// Theme (now in modern_theme.hpp)
// ============================================
// Old theme removed - using modern_theme.hpp

// ============================================
// DirectX
// ============================================
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
HWND g_hwnd = nullptr;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ============================================
// Utilities
// ============================================
std::string GetAppDataPath() {
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
        std::string appPath = std::string(path) + "\\SingleProject";
        CreateDirectoryA(appPath.c_str(), NULL);
        return appPath;
    }
    return ".";
}

std::string GetHWID() {
    char volumeName[MAX_PATH], fsName[MAX_PATH];
    DWORD serialNumber, maxLen, flags;
    if (GetVolumeInformationA("C:\\", volumeName, MAX_PATH, &serialNumber, &maxLen, &flags, fsName, MAX_PATH)) {
        char hwid[32];
        sprintf_s(hwid, "%08X", serialNumber);
        return std::string(hwid);
    }
    return "UNKNOWN";
}

void SaveSession(const std::string& token, const std::string& username) {
    std::ofstream file(GetAppDataPath() + "\\session.dat");
    if (file.is_open()) {
        file << token << "\n" << username;
        file.close();
    }
}

void SaveCredentials() {
    if (g_rememberLogin && strlen(g_username) > 0) {
        std::ofstream file(GetAppDataPath() + "\\credentials.dat");
        if (file.is_open()) {
            file << g_username << "\n" << g_password;
            file.close();
        }
    }
}

void LoadCredentials() {
    std::ifstream file(GetAppDataPath() + "\\credentials.dat");
    if (file.is_open()) {
        std::string user, pass;
        std::getline(file, user);
        std::getline(file, pass);
        file.close();
        if (!user.empty()) {
            strncpy_s(g_username, user.c_str(), sizeof(g_username) - 1);
            strncpy_s(g_password, pass.c_str(), sizeof(g_password) - 1);
        }
    }
}

bool LoadSession() {
    std::ifstream file(GetAppDataPath() + "\\session.dat");
    if (file.is_open()) {
        std::getline(file, g_token);
        std::getline(file, g_displayName);
        file.close();
        return !g_token.empty();
    }
    return false;
}

void ClearSession() {
    g_token = "";
    g_displayName = "";
    for (auto& g : g_games) {
        g.hasLicense = false;
        g.daysRemaining = 0;
    }
    DeleteFileA((GetAppDataPath() + "\\session.dat").c_str());
}

// ============================================
// HTTP
// ============================================
std::string HttpRequest(const std::string& method, const std::string& path, const std::string& data = "") {
    HINTERNET hInternet = InternetOpenA("SingleProject/2.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        DWORD error = GetLastError();
        g_statusMsg = "Network error: " + std::to_string(error);
        return "";
    }
    
    // Set aggressive timeouts (3 seconds connect, 5 seconds receive)
    DWORD connectTimeout = 3000;
    DWORD receiveTimeout = 5000;
    DWORD sendTimeout = 3000;
    InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &connectTimeout, sizeof(connectTimeout));
    InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &receiveTimeout, sizeof(receiveTimeout));
    InternetSetOptionA(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &sendTimeout, sizeof(sendTimeout));
    
    HINTERNET hConnect = InternetConnectA(hInternet, SERVER_HOST, SERVER_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        DWORD error = GetLastError();
        g_statusMsg = "Connection failed: " + std::to_string(error);
        InternetCloseHandle(hInternet);
        return "";
    }
    
    HINTERNET hRequest = HttpOpenRequestA(hConnect, method.c_str(), path.c_str(), NULL, NULL, NULL, 0, 0);
    if (!hRequest) {
        DWORD error = GetLastError();
        g_statusMsg = "Request failed: " + std::to_string(error);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }
    
    std::string headers = "Content-Type: application/json\r\n";
    headers += "Host: " SERVER_HOST_HEADER "\r\n"; // Required for nginx virtual host
    if (!g_token.empty()) headers += "Authorization: Bearer " + g_token + "\r\n";
    
    BOOL result = data.empty() 
        ? HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.length(), NULL, 0)
        : HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.length(), (LPVOID)data.c_str(), (DWORD)data.length());
    
    if (!result) {
        DWORD error = GetLastError();
        g_statusMsg = "Send failed: " + std::to_string(error);
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }
    
    // Check HTTP status code
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeSize, NULL);
    
    std::string response;
    char buffer[4096];
    DWORD bytesRead;
    while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        response += buffer;
    }
    
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    
    // Handle HTTP errors
    if (statusCode >= 400) {
        if (statusCode == 401) {
            g_statusMsg = "Authentication failed";
        } else if (statusCode == 403) {
            g_statusMsg = "Access denied";
        } else if (statusCode == 404) {
            g_statusMsg = "Endpoint not found";
        } else if (statusCode == 429) {
            g_statusMsg = "Too many requests, wait a moment";
        } else if (statusCode >= 500) {
            g_statusMsg = "Server error: " + std::to_string(statusCode);
        } else {
            g_statusMsg = "HTTP error: " + std::to_string(statusCode);
        }
    }
    
    return response;
}

std::string ExtractJson(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '"')) pos++;
    size_t end = pos;
    bool inStr = json[pos - 1] == '"';
    if (inStr) end = json.find('"', pos);
    else while (end < json.length() && json[end] != ',' && json[end] != '}') end++;
    return json.substr(pos, end - pos);
}

int ExtractInt(const std::string& json, const std::string& key) {
    std::string val = ExtractJson(json, key);
    try { return std::stoi(val); } catch (...) { return 0; }
}

// ============================================
// Auto-Update
// ============================================
std::atomic<bool> g_updateAvailable{false};
std::atomic<bool> g_updateDownloading{false};
std::atomic<float> g_updateProgress{0.0f};
std::string g_newVersion = "";

// Compare versions (returns: -1 if a < b, 0 if a == b, 1 if a > b)
int CompareVersions(const std::string& a, const std::string& b) {
    int aMajor = 0, aMinor = 0, aPatch = 0;
    int bMajor = 0, bMinor = 0, bPatch = 0;
    sscanf(a.c_str(), "%d.%d.%d", &aMajor, &aMinor, &aPatch);
    sscanf(b.c_str(), "%d.%d.%d", &bMajor, &bMinor, &bPatch);
    
    if (aMajor != bMajor) return aMajor < bMajor ? -1 : 1;
    if (aMinor != bMinor) return aMinor < bMinor ? -1 : 1;
    if (aPatch != bPatch) return aPatch < bPatch ? -1 : 1;
    return 0;
}

void CheckForUpdates() {
    std::thread([]() {
        try {
            // Read launcher version from server via /api/games/status (games.launcher.version)
            std::string response = HttpRequest("GET", "/api/games/status");
            if (response.empty()) return;
            
            // Skip update check for local development builds
            if (std::string(LAUNCHER_VERSION).find("local") != std::string::npos) return;
            
            // Find launcher block and version field
            std::string serverVersion;
            size_t launcherPos = response.find("\"launcher\":");
            if (launcherPos != std::string::npos) {
                size_t verPos = response.find("\"version\":\"", launcherPos);
                if (verPos != std::string::npos) {
                    verPos += 11;
                    size_t endPos = response.find("\"", verPos);
                    if (endPos != std::string::npos) {
                        serverVersion = response.substr(verPos, endPos - verPos);
                    }
                }
            }
            if (serverVersion.empty()) return;
            
            // Compare with current version
            if (CompareVersions(LAUNCHER_VERSION, serverVersion) < 0) {
                g_newVersion = serverVersion;
                g_updateAvailable = true;
            }
        } catch (...) {}
    }).detach();
}

bool DownloadUpdate() {
    g_updateDownloading = true;
    g_updateProgress = 0.0f;
    
    HINTERNET hInternet = InternetOpenA("SingleProject", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) { g_updateDownloading = false; return false; }
    
    HINTERNET hConnect = InternetConnectA(hInternet, SERVER_HOST, SERVER_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) { InternetCloseHandle(hInternet); g_updateDownloading = false; return false; }
    
    // Download launcher from public endpoint (server decrypts .enc)
    HINTERNET hRequest = HttpOpenRequestA(hConnect, "GET", "/api/download/launcher", NULL, NULL, NULL, INTERNET_FLAG_RELOAD, 0);
    if (!hRequest) { InternetCloseHandle(hConnect); InternetCloseHandle(hInternet); g_updateDownloading = false; return false; }
    
    if (!HttpSendRequestA(hRequest, NULL, 0, NULL, 0)) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        g_updateDownloading = false;
        return false;
    }
    
    // Get file size
    DWORD contentLength = 0;
    DWORD bufferSize = sizeof(contentLength);
    HttpQueryInfoA(hRequest, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &contentLength, &bufferSize, NULL);
    
    // Download to temp file
    std::string tempPath = GetAppDataPath() + "\\launcher_new.exe";
    std::ofstream outFile(tempPath, std::ios::binary);
    if (!outFile.is_open()) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        g_updateDownloading = false;
        return false;
    }
    
    char buffer[8192];
    DWORD bytesRead;
    DWORD totalRead = 0;
    
    while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        outFile.write(buffer, bytesRead);
        totalRead += bytesRead;
        if (contentLength > 0) {
            g_updateProgress = (float)totalRead / (float)contentLength;
        }
    }
    
    outFile.close();
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    
    g_updateDownloading = false;
    
    // Verify download
    if (totalRead < 1024) {
        DeleteFileA(tempPath.c_str());
        return false;
    }
    
    return true;
}

void ApplyUpdate() {
    std::string currentExe, tempExe, updaterBat;
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    currentExe = exePath;
    tempExe = GetAppDataPath() + "\\launcher_new.exe";
    updaterBat = GetAppDataPath() + "\\update.bat";
    
    // Create updater script
    std::ofstream bat(updaterBat);
    if (bat.is_open()) {
        bat << "@echo off\n";
        bat << "timeout /t 2 /nobreak >nul\n";
        bat << "copy /y \"" << tempExe << "\" \"" << currentExe << "\"\n";
        bat << "del \"" << tempExe << "\"\n";
        bat << "start \"\" \"" << currentExe << "\"\n";
        bat << "del \"%~f0\"\n";
        bat.close();
        
        // Run updater and exit (give bat time to start)
        ShellExecuteA(NULL, "open", updaterBat.c_str(), NULL, NULL, SW_HIDE);
        Sleep(500); // Wait for bat to start
        ExitProcess(0);
    }
}

// ============================================
// API Functions
// ============================================
void FetchUserLicenses() {
    std::string response = HttpRequest("GET", "/api/auth/me");
    if (response.empty()) return;
    
    for (auto& game : g_games) {
        game.hasLicense = false;
        game.daysRemaining = 0;
    }
    
    if (response.find("\"cs2\"") != std::string::npos || response.find("\"game_id\":\"cs2\"") != std::string::npos) {
        g_games[0].hasLicense = true;
        g_games[0].daysRemaining = ExtractInt(response, "days_remaining");
    }
}

// Fetch game status from server - non-blocking, always runs
void FetchGameStatus() {
    // Skip only if already refreshing for more than 5 seconds
    static auto lastRequestTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    
    if (g_statusRefreshing) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastRequestTime).count();
        if (elapsed < 5) return; // Still waiting for previous request
        // Timeout - force reset
        g_statusRefreshing = false;
    }
    
    g_statusRefreshing = true;
    lastRequestTime = now;
    
    std::thread([]() {
        try {
            std::string response = HttpRequest("GET", "/api/games/status");
            if (!response.empty()) {
                // Parse status for CS2 (primary game)
                size_t cs2Pos = response.find("\"cs2\":");
                if (cs2Pos != std::string::npos) {
                    size_t statusPos = response.find("\"status\":\"", cs2Pos);
                    if (statusPos != std::string::npos) {
                        statusPos += 10;
                        size_t endPos = response.find("\"", statusPos);
                        if (endPos != std::string::npos) {
                            g_gameStatus = response.substr(statusPos, endPos - statusPos);
                        }
                    }
                    
                    size_t msgPos = response.find("\"message\":\"", cs2Pos);
                    if (msgPos != std::string::npos) {
                        msgPos += 11;
                        size_t endPos = response.find("\"", msgPos);
                        if (endPos != std::string::npos) {
                            g_gameStatusMsg = response.substr(msgPos, endPos - msgPos);
                        }
                    }
                    
                    // Parse version
                    size_t verPos = response.find("\"version\":\"", cs2Pos);
                    if (verPos != std::string::npos) {
                        verPos += 11;
                        size_t endPos = response.find("\"", verPos);
                        if (endPos != std::string::npos) {
                            g_games[0].version = response.substr(verPos, endPos - verPos);
                        }
                    }
                }
                
                g_lastRefresh = std::chrono::steady_clock::now();
            }
        } catch (...) {}
        
        g_statusRefreshing = false;
    }).detach();
}

// Refresh all data (license, status)
void RefreshData() {
    if (g_token.empty()) return;
    
    g_isLoading = true;
    
    std::thread([&]() {
        try {
            // Fetch license info
            std::string response = HttpRequest("GET", "/api/auth/me");
            
            // Update game licenses from response
            if (!response.empty()) {
                for (auto& game : g_games) {
                    size_t pos = response.find("\"game_id\":\"" + game.id + "\"");
                    if (pos != std::string::npos) {
                        size_t daysPos = response.find("\"days_remaining\":", pos);
                        if (daysPos != std::string::npos) {
                            daysPos += 17;
                            int days = std::atoi(response.c_str() + daysPos);
                            game.hasLicense = true;
                            game.daysRemaining = days;
                        }
                    } else {
                        game.hasLicense = false;
                        game.daysRemaining = 0;
                    }
                }
            }
            
            // Fetch game status
            FetchGameStatus();
            
        } catch (...) {}
        
        g_isLoading = false;
        g_successMsg = "Data refreshed!";
    }).detach();
}

void DoLogin() {
    if (strlen(g_username) == 0 || strlen(g_password) == 0) {
        g_errorMsg = "Enter username and password";
        return;
    }
    
    // Validate input length
    if (strlen(g_username) < 3) {
        g_errorMsg = "Username too short (min 3 chars)";
        return;
    }
    
    if (strlen(g_password) < 4) {
        g_errorMsg = "Password too short (min 4 chars)";
        return;
    }
    
    g_isLoading = true;
    g_errorMsg = "";
    g_statusMsg = "Connecting...";
    
    std::thread([]() {
        try {
            g_hwid = GetHWID();
            std::string data = "{\"username\":\"" + std::string(g_username) + 
                              "\",\"password\":\"" + std::string(g_password) + 
                              "\",\"hwid\":\"" + g_hwid + "\"}";
            
            std::string response = HttpRequest("POST", "/api/auth/login", data);
            
            if (response.empty()) {
                g_errorMsg = !g_statusMsg.empty() ? g_statusMsg : "Connection failed - check internet";
                g_isLoading = false;
                g_statusMsg = "";
                return;
            }
            
            if (response.find("\"token\"") != std::string::npos) {
                g_token = ExtractJson(response, "token");
                g_displayName = g_username;
                SaveSession(g_token, g_displayName);
                SaveCredentials();
                FetchUserLicenses();
                FetchGameStatus();
                g_currentScreen = Screen::Main;
                g_fadeAlpha = 0.0f;
                ShowToast("Welcome, " + g_displayName + "!", false);
            } else {
                std::string error = ExtractJson(response, "error");
                if (error.empty()) {
                    error = !g_statusMsg.empty() ? g_statusMsg : "Login failed";
                }
                ShowToast(error, true);
            }
            
            g_isLoading = false;
            g_statusMsg = "";
        } catch (...) {
            g_errorMsg = "Unexpected error occurred";
            g_isLoading = false;
            g_statusMsg = "";
        }
    }).detach();
}

void DoRegister() {
    if (strlen(g_username) == 0 || strlen(g_password) == 0 || strlen(g_licenseKey) == 0) {
        g_errorMsg = "Fill all fields";
        return;
    }
    
    // Validate input
    if (strlen(g_username) < 3 || strlen(g_username) > 32) {
        g_errorMsg = "Username: 3-32 characters";
        return;
    }
    
    if (strlen(g_password) < 6 || strlen(g_password) > 64) {
        g_errorMsg = "Password: 6-64 characters";
        return;
    }
    
    if (strlen(g_licenseKey) < 10) {
        g_errorMsg = "Invalid license key format";
        return;
    }
    
    g_isLoading = true;
    g_errorMsg = "";
    g_statusMsg = "Creating account...";
    
    std::thread([]() {
        try {
            std::string data = "{\"username\":\"" + std::string(g_username) + 
                              "\",\"password\":\"" + std::string(g_password) + 
                              "\",\"license_key\":\"" + std::string(g_licenseKey) + "\"}";
            
            std::string response = HttpRequest("POST", "/api/auth/register", data);
            
            if (response.empty()) {
                g_errorMsg = !g_statusMsg.empty() ? g_statusMsg : "Connection failed - check internet";
                g_isLoading = false;
                g_statusMsg = "";
                return;
            }
            
            if (response.find("\"success\"") != std::string::npos) {
                g_successMsg = "Account created! Login now.";
                g_currentScreen = Screen::Login;
                g_fadeAlpha = 0.0f;
                memset(g_licenseKey, 0, sizeof(g_licenseKey));
            } else {
                g_errorMsg = ExtractJson(response, "error");
                if (g_errorMsg.empty()) {
                    g_errorMsg = !g_statusMsg.empty() ? g_statusMsg : "Registration failed";
                }
            }
            
            g_isLoading = false;
            g_statusMsg = "";
        } catch (...) {
            g_errorMsg = "Unexpected error occurred";
            g_isLoading = false;
            g_statusMsg = "";
        }
    }).detach();
}

void ActivateLicense(const std::string& gameId) {
    if (strlen(g_activateKey) == 0) {
        g_errorMsg = "Enter license key";
        return;
    }
    
    if (strlen(g_activateKey) < 10) {
        g_errorMsg = "Invalid license key format";
        return;
    }
    
    g_isLoading = true;
    g_errorMsg = "";
    g_statusMsg = "Activating license...";
    
    std::thread([gameId]() {
        try {
            std::string data = "{\"license_key\":\"" + std::string(g_activateKey) + "\"}";
            std::string response = HttpRequest("POST", "/api/auth/activate", data);
            
            if (response.empty()) {
                g_errorMsg = !g_statusMsg.empty() ? g_statusMsg : "Connection failed";
                g_isLoading = false;
                g_statusMsg = "";
                return;
            }
            
            if (response.find("\"success\"") != std::string::npos) {
                g_successMsg = "License activated!";
                memset(g_activateKey, 0, sizeof(g_activateKey));
                FetchUserLicenses();
            } else {
                g_errorMsg = ExtractJson(response, "error");
                if (g_errorMsg.empty()) {
                    g_errorMsg = !g_statusMsg.empty() ? g_statusMsg : "Activation failed";
                }
            }
            
            g_isLoading = false;
            g_statusMsg = "";
        } catch (...) {
            g_errorMsg = "Unexpected error occurred";
            g_isLoading = false;
            g_statusMsg = "";
        }
    }).detach();
}

bool DownloadFile(const std::string& url, const std::string& savePath) {
    HINTERNET hInternet = InternetOpenA("SingleProject", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return false;
    
    HINTERNET hConnect = InternetConnectA(hInternet, SERVER_HOST, SERVER_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) { InternetCloseHandle(hInternet); return false; }
    
    HINTERNET hRequest = HttpOpenRequestA(hConnect, "GET", url.c_str(), NULL, NULL, NULL, 0, 0);
    if (!hRequest) { InternetCloseHandle(hConnect); InternetCloseHandle(hInternet); return false; }
    
    std::string headers = "Authorization: Bearer " + g_token + "\r\n";
    if (!HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.length(), NULL, 0)) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return false;
    }
    
    DWORD contentLength = 0;
    DWORD bufLen = sizeof(contentLength);
    HttpQueryInfoA(hRequest, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &contentLength, &bufLen, NULL);
    
    std::ofstream file(savePath, std::ios::binary);
    if (!file.is_open()) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return false;
    }
    
    char buffer[8192];
    DWORD bytesRead, totalRead = 0;
    while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        file.write(buffer, bytesRead);
        totalRead += bytesRead;
        if (contentLength > 0) g_downloadProgress = (float)totalRead / contentLength;
    }
    
    file.close();
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return totalRead > 0;
}

// Check game status from server
std::string GetGameStatus(const std::string& gameId) {
    HINTERNET hSession = WinHttpOpen(L"SingleProject/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return "error";
    
    std::wstring wHost(SERVER_HOST, SERVER_HOST + strlen(SERVER_HOST));
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), SERVER_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return "error"; }
    
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/api/games/status", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return "error"; }
    
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return "error";
    }
    
    std::string response;
    DWORD bytesRead;
    char buffer[4096];
    while (WinHttpReadData(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        response += buffer;
    }
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    // Parse status for game
    std::string searchKey = "\"" + gameId + "\":{";
    size_t pos = response.find(searchKey);
    if (pos != std::string::npos) {
        size_t statusPos = response.find("\"status\":\"", pos);
        if (statusPos != std::string::npos) {
            statusPos += 10;
            size_t endPos = response.find("\"", statusPos);
            if (endPos != std::string::npos) {
                return response.substr(statusPos, endPos - statusPos);
            }
        }
    }
    
    return "operational";
}

void LaunchGame(int gameIndex) {
    GameInfo& game = g_games[gameIndex];
    
    // Check game status
    if (g_gameStatus != "operational") {
        if (g_gameStatus == "updating") {
            g_errorMsg = "Cheat is updating. Please wait.";
        } else if (g_gameStatus == "offline") {
            g_errorMsg = "Cheat is currently offline.";
        } else {
            g_errorMsg = "Cheat unavailable: " + g_gameStatusMsg;
        }
        return;
    }
    
    if (!game.hasLicense) {
        g_errorMsg = "No license for " + game.name;
        return;
    }
    
    if (!game.available) {
        g_errorMsg = game.name + " coming soon!";
        return;
    }
    
    // Check game status before launching
    g_statusMsg = "Checking status...";
    std::string status = GetGameStatus(game.id);
    
    if (status == "updating") {
        g_errorMsg = "Cheat is being updated. Please wait...";
        g_statusMsg = "";
        return;
    }
    
    if (status == "maintenance" || status == "offline") {
        g_errorMsg = "Cheat is currently offline for maintenance";
        g_statusMsg = "";
        return;
    }
    
    if (status == "error") {
        g_errorMsg = "Cannot check status. Try again later.";
        g_statusMsg = "";
        return;
    }
    
    g_isDownloading = true;
    g_downloadProgress = 0.0f;
    g_statusMsg = "Downloading...";
    g_errorMsg = "";
    
    std::thread([gameIndex, &game]() {
        std::string savePath = GetAppDataPath() + "\\" + game.id + "_external.exe";
        std::string url = "/api/download/" + game.id + "/external";
        
        if (DownloadFile(url, savePath)) {
            g_statusMsg = "Checking game...";
            
            bool gameRunning = false;
            HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (hSnap != INVALID_HANDLE_VALUE) {
                PROCESSENTRY32 pe = { sizeof(pe) };
                if (Process32First(hSnap, &pe)) {
                    do {
                        if (_stricmp(pe.szExeFile, game.processName.c_str()) == 0) {
                            gameRunning = true;
                            break;
                        }
                    } while (Process32Next(hSnap, &pe));
                }
                CloseHandle(hSnap);
            }
            
            if (!gameRunning) {
                g_errorMsg = "Start " + game.name + " first!";
                g_isDownloading = false;
                return;
            }
            
            g_statusMsg = "Launching...";
            Sleep(500);
            
            STARTUPINFOA si = { sizeof(si) };
            PROCESS_INFORMATION pi;
            if (CreateProcessA(savePath.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                g_cheatRunning = true;
                g_successMsg = "Running!";
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            } else {
                g_errorMsg = "Launch failed";
            }
        } else {
            g_errorMsg = "Download failed";
        }
        
        g_isDownloading = false;
        g_statusMsg = "";
    }).detach();
}

void DoLogout() {
    ClearSession();
    g_currentScreen = Screen::Login;
    g_fadeAlpha = 0.0f;
    g_successMsg = "";
    g_errorMsg = "";
    g_cheatRunning = false;
}

// ============================================
// UI Components
// ============================================

// Modern Progress Bar with gradient
void DrawProgressBar(ImDrawList* dl, ImVec2 pos, ImVec2 size, float progress, const char* label = nullptr) {
    progress = std::clamp(progress, 0.0f, 1.0f);
    
    // Background
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(20, 20, 28, 255), 4.0f);
    
    // Progress fill with gradient
    if (progress > 0.01f) {
        float fillWidth = size.x * progress;
        dl->AddRectFilledMultiColor(
            pos,
            ImVec2(pos.x + fillWidth, pos.y + size.y),
            IM_COL32(140, 90, 245, 255),
            IM_COL32(80, 150, 255, 255),
            IM_COL32(80, 150, 255, 255),
            IM_COL32(140, 90, 245, 255)
        );
    }
    
    // Border
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(60, 60, 80, 180), 4.0f, 0, 1.5f);
    
    // Label
    if (label) {
        ImVec2 textSize = ImGui::CalcTextSize(label);
        ImVec2 textPos(pos.x + (size.x - textSize.x) * 0.5f, pos.y + (size.y - textSize.y) * 0.5f);
        dl->AddText(textPos, IM_COL32(255, 255, 255, 255), label);
    }
}

// Animated Loading Spinner
void DrawSpinner(ImDrawList* dl, ImVec2 center, float radius, ImU32 color, float time) {
    const int num_segments = 30;
    float angle_offset = time * 5.0f;
    
    for (int i = 0; i < num_segments; ++i) {
        float angle = (float)i / num_segments * 2.0f * 3.14159f + angle_offset;
        float alpha = (float)i / num_segments;
        ImU32 col = IM_COL32(
            (color >> IM_COL32_R_SHIFT) & 0xFF,
            (color >> IM_COL32_G_SHIFT) & 0xFF,
            (color >> IM_COL32_B_SHIFT) & 0xFF,
            (int)(alpha * 255)
        );
        
        float x = center.x + cosf(angle) * radius;
        float y = center.y + sinf(angle) * radius;
        dl->AddCircleFilled(ImVec2(x, y), radius * 0.2f, col);
    }
}

// Status Indicator with pulse animation
void DrawStatusIndicator(ImDrawList* dl, ImVec2 pos, const std::string& status, float pulse) {
    ImU32 color;
    const char* text;
    
    if (status == "operational") {
        color = IM_COL32(30, 220, 100, 255);
        text = "Online";
    } else if (status == "updating") {
        color = IM_COL32(255, 180, 0, 255);
        text = "Updating";
    } else if (status == "maintenance") {
        color = IM_COL32(255, 100, 100, 255);
        text = "Offline";
    } else {
        color = IM_COL32(150, 150, 160, 255);
        text = "Unknown";
    }
    
    // Pulse effect for operational status
    float pulseRadius = 4.0f;
    if (status == "operational") {
        pulseRadius += pulse * 2.0f;
        ImU32 pulseColor = IM_COL32(30, 220, 100, (int)((1.0f - pulse) * 60));
        dl->AddCircleFilled(pos, pulseRadius + 2, pulseColor);
    }
    
    // Main dot
    dl->AddCircleFilled(pos, pulseRadius, color);
    
    // Text
    ImVec2 textPos(pos.x + 12, pos.y - 7);
    dl->AddText(textPos, IM_COL32(200, 200, 210, 255), text);
}

// Modern Game Card with gradient and hover effect  
void DrawGameCard(ImDrawList* dl, ImVec2 pos, ImVec2 size, const GameInfo& game, bool hovered, float pulse) {
    // Shadow
    dl->AddRectFilled(
        ImVec2(pos.x + 3, pos.y + 3),
        ImVec2(pos.x + size.x + 3, pos.y + size.y + 3),
        IM_COL32(0, 0, 0, 60),
        12.0f
    );
    
    // Background with gradient
    ImU32 bgColor1 = hovered ? IM_COL32(24, 24, 35, 255) : IM_COL32(18, 18, 28, 255);
    ImU32 bgColor2 = hovered ? IM_COL32(30, 28, 42, 255) : IM_COL32(22, 22, 32, 255);
    
    dl->AddRectFilledMultiColor(
        pos,
        ImVec2(pos.x + size.x, pos.y + size.y),
        bgColor1, bgColor1, bgColor2, bgColor2
    );
    
    // Top accent bar (gradient)
    dl->AddRectFilledMultiColor(
        pos,
        ImVec2(pos.x + size.x, pos.y + 4),
        IM_COL32(140, 90, 245, 255),
        IM_COL32(80, 150, 255, 255),
        IM_COL32(80, 150, 255, 255),
        IM_COL32(140, 90, 245, 255)
    );
    
    // Border with glow on hover
    ImU32 borderColor = hovered ? IM_COL32(140, 90, 245, 200) : IM_COL32(60, 60, 80, 100);
    if (hovered) {
        dl->AddRect(
            ImVec2(pos.x - 1, pos.y - 1),
            ImVec2(pos.x + size.x + 1, pos.y + size.y + 1),
            IM_COL32(140, 90, 245, (int)(pulse * 100)),
            12.0f, 0, 3.0f
        );
    }
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), borderColor, 12.0f, 0, 1.5f);
}

void StyledInput(const char* label, char* buf, size_t size, bool password = false) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14, 12));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::surface);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, theme::surfaceHover);
    ImGui::PushStyleColor(ImGuiCol_Border, theme::border);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::InputText(label, buf, size, password ? ImGuiInputTextFlags_Password : 0);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(3);
}

bool StyledButton(const char* label, ImVec2 size, bool enabled = true) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    if (!enabled) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, theme::accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::accentHover);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    }
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    return clicked && enabled;
}

void DrawWindowControls(ImDrawList* dl, ImVec2 ws) {
    // Minimize button
    ImVec2 minBtn(ws.x - 70, 10);
    ImVec2 minBtnSize(25, 25);
    
    ImGui::SetCursorPos(minBtn);
    bool minHovered = false;
    ImVec2 mousePos = ImGui::GetMousePos();
    if (mousePos.x >= minBtn.x && mousePos.x <= minBtn.x + minBtnSize.x &&
        mousePos.y >= minBtn.y && mousePos.y <= minBtn.y + minBtnSize.y) minHovered = true;
        
    dl->AddRectFilled(minBtn, ImVec2(minBtn.x + minBtnSize.x, minBtn.y + minBtnSize.y), 
        minHovered ? IM_COL32(80, 80, 100, 255) : IM_COL32(50, 50, 65, 255), 6.0f);
        
    ImVec2 minTextSize = ImGui::CalcTextSize("_");
    dl->AddText(ImVec2(minBtn.x + (minBtnSize.x - minTextSize.x) * 0.5f, minBtn.y + (minBtnSize.y - minTextSize.y) * 0.5f - 2), 
        IM_COL32(255, 255, 255, 255), "_");
        
    if (ImGui::InvisibleButton("##min_ctrl", minBtnSize)) {
        ShowWindow(g_hwnd, SW_MINIMIZE);
    }
    
    // Close button
    ImVec2 closeBtn(ws.x - 38, 10);
    
    ImGui::SetCursorPos(closeBtn);
    bool closeHovered = false;
    if (mousePos.x >= closeBtn.x && mousePos.x <= closeBtn.x + minBtnSize.x &&
        mousePos.y >= closeBtn.y && mousePos.y <= closeBtn.y + minBtnSize.y) closeHovered = true;
        
    dl->AddRectFilled(closeBtn, ImVec2(closeBtn.x + minBtnSize.x, closeBtn.y + minBtnSize.y), 
        closeHovered ? IM_COL32(200, 60, 60, 255) : IM_COL32(140, 40, 40, 200), 8.0f);
        
    ImVec2 closeTextSize = ImGui::CalcTextSize("X");
    dl->AddText(ImVec2(closeBtn.x + (minBtnSize.x - closeTextSize.x) * 0.5f, closeBtn.y + (minBtnSize.y - closeTextSize.y) * 0.5f), 
        IM_COL32(255, 255, 255, 255), "X");
        
    if (ImGui::InvisibleButton("##close_ctrl", minBtnSize)) {
        PostQuitMessage(0);
    }
}

// ============================================
// Screens
// ============================================
void RenderSplash() {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImVec2 ws = ImGui::GetIO().DisplaySize;
    ImVec2 center(ws.x * 0.5f, ws.y * 0.5f);
    
    dl->AddRectFilledMultiColor(ImVec2(0, 0), ws, 
        IM_COL32(10, 10, 18, 255), IM_COL32(10, 10, 18, 255),
        IM_COL32(5, 5, 10, 255), IM_COL32(5, 5, 10, 255));
    
    float scale = fminf(g_splashTimer / 0.5f, 1.0f);
    dl->AddCircleFilled(ImVec2(center.x, center.y - 30), 40.0f * scale, theme::gradientA, 32);
    
    if (g_splashTimer > 0.3f) {
        float alpha = fminf((g_splashTimer - 0.3f) / 0.4f, 1.0f);
        const char* title = PROJECT_NAME;
        ImVec2 ts = ImGui::CalcTextSize(title);
        dl->AddText(ImVec2(center.x - ts.x * 0.5f, center.y + 30), 
            IM_COL32(255, 255, 255, (int)(alpha * 255)), title);
    }
    
    float progress = fminf(g_splashTimer / 2.0f, 1.0f);
    ImVec2 barPos(center.x - 90, center.y + 70);
    dl->AddRectFilled(barPos, ImVec2(barPos.x + 180, barPos.y + 4), IM_COL32(40, 40, 60, 255), 2.0f);
    dl->AddRectFilled(barPos, ImVec2(barPos.x + 180 * progress, barPos.y + 4), theme::gradientA, 2.0f);
    
    if (g_splashTimer > 2.2f) {
        LoadCredentials();
        if (LoadSession()) {
            FetchUserLicenses();
            FetchGameStatus();
            g_currentScreen = Screen::Main;
        } else {
            g_currentScreen = Screen::Login;
        }
        g_fadeAlpha = 0.0f;
    }
}

void RenderLogin() {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImVec2 ws = ImGui::GetIO().DisplaySize;
    
    // Background: soft radial gradient (website-like)
    dl->AddRectFilledMultiColor(ImVec2(0, 0), ws, 
        IM_COL32(8, 8, 16, 255), IM_COL32(8, 8, 16, 255),
        IM_COL32(4, 4, 10, 255), IM_COL32(4, 4, 10, 255));
    
    // Subtle diagonal glow
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(ws.x, ws.y * 0.5f),
        IM_COL32(40, 20, 80, 40));
    
    g_fadeAlpha = fminf(g_fadeAlpha + ImGui::GetIO().DeltaTime * 3.0f, 1.0f);
    
    float w = 300.0f;
    float x = (ws.x - w) * 0.5f;
    
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ws);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    
    if (ImGui::Begin("##login", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_fadeAlpha);
        
        // Window controls
        DrawWindowControls(dl, ws);
        
        // Logo + Project name (Higher up) with halo
        ImVec2 logoCenter(ws.x * 0.5f, 70);
        for (int r = 42; r >= 34; r -= 2) {
            dl->AddCircleFilled(logoCenter, (float)r, IM_COL32(70, 40, 120, (42 - r) * 4), 48);
        }
        dl->AddCircleFilled(logoCenter, 32.0f, theme::gradientA, 32);
        
        ImGui::SetCursorPos(ImVec2(0, 115));
        float tw = ImGui::CalcTextSize(PROJECT_NAME).x;
        ImGui::SetCursorPosX((ws.x - tw) * 0.5f);
        ImGui::TextColored(theme::text, PROJECT_NAME);
        
        ImVec2 subSize = ImGui::CalcTextSize("Sign in to continue");
        ImGui::SetCursorPosX((ws.x - subSize.x) * 0.5f);
        ImGui::TextColored(theme::textDim, "Sign in to continue");
        
        ImGui::Dummy(ImVec2(0, 10));
        
        // Semi-transparent form card (like a section on the website)
        float cardWidth = w + 60.0f;
        ImVec2 cardPos((ws.x - cardWidth) * 0.5f, 135.0f);
        ImVec2 cardSize(cardWidth, 235.0f);
        dl->AddRectFilled(cardPos, ImVec2(cardPos.x + cardSize.x, cardPos.y + cardSize.y),
            IM_COL32(12, 12, 22, 230), 18.0f);
        dl->AddRect(cardPos, ImVec2(cardPos.x + cardSize.x, cardPos.y + cardSize.y),
            IM_COL32(90, 70, 160, 180), 18.0f);
        
        // Thin accent line on top of the card
        dl->AddRectFilled(ImVec2(cardPos.x + 16, cardPos.y + 4),
                          ImVec2(cardPos.x + cardSize.x - 16, cardPos.y + 6),
                          theme::gradientA, 3.0f);
        
        // Inputs (Reduced spacing)
        ImGui::SetCursorPosX(x);
        ImGui::SetCursorPosY(150.0f);
        ImGui::TextColored(theme::textSec, "Username");
        ImGui::SetCursorPosX(x);
        ImGui::SetNextItemWidth(w);
        StyledInput("##user", g_username, sizeof(g_username));
        
        ImGui::Dummy(ImVec2(0, 6));
        
        ImGui::SetCursorPosX(x);
        ImGui::TextColored(theme::textSec, "Password");
        ImGui::SetCursorPosX(x);
        ImGui::SetNextItemWidth(w);
        StyledInput("##pass", g_password, sizeof(g_password), true);
        
        // Remember me
        ImGui::Dummy(ImVec2(0, 6));
        ImGui::SetCursorPosX(x);
        ImGui::Checkbox("Remember me", &g_rememberLogin);
        
        ImGui::Dummy(ImVec2(0, 12));
        
        // Login button
        ImGui::SetCursorPosX(x);
        if (StyledButton(g_isLoading ? "  Signing in...  " : "  SIGN IN  ", ImVec2(w, 46), !g_isLoading)) {
            DoLogin();
        }
        
        // Register link (centered below button with spacing)
        ImGui::Dummy(ImVec2(0, 12));
        std::string regText = "No account? Register";
        float regWidth = ImGui::CalcTextSize(regText.c_str()).x;
        ImGui::SetCursorPosX((ws.x - regWidth) * 0.5f);
        ImGui::TextColored(theme::textDim, "No account? ");
        ImGui::SameLine(0, 0);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::accent);
        if (ImGui::SmallButton("Register")) {
            g_currentScreen = Screen::Register;
            g_fadeAlpha = 0.0f;
            g_errorMsg = "";
            g_successMsg = "";
        }
        ImGui::PopStyleColor(2);
        
        // Version (absolute bottom)
        std::string verText = "v" LAUNCHER_VERSION;
        ImVec2 verSize = ImGui::CalcTextSize(verText.c_str());
        ImGui::SetCursorPos(ImVec2((ws.x - verSize.x) * 0.5f, ws.y - 25));
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.4f), "%s", verText.c_str());
        
        // Error/success messages (overlay)
        if (!g_errorMsg.empty()) {
            ImVec2 msgSize = ImGui::CalcTextSize(g_errorMsg.c_str());
            ImGui::SetCursorPos(ImVec2((ws.x - msgSize.x) * 0.5f, 380));
            ImGui::TextColored(theme::error, "%s", g_errorMsg.c_str());
        }
        if (!g_successMsg.empty()) {
            ImVec2 msgSize = ImGui::CalcTextSize(g_successMsg.c_str());
            ImGui::SetCursorPos(ImVec2((ws.x - msgSize.x) * 0.5f, 380));
            ImGui::TextColored(theme::success, "%s", g_successMsg.c_str());
        }
        
        ImGui::PopStyleVar();
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void RenderRegister() {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImVec2 ws = ImGui::GetIO().DisplaySize;
    
    dl->AddRectFilledMultiColor(ImVec2(0, 0), ws, 
        IM_COL32(10, 10, 18, 255), IM_COL32(10, 10, 18, 255),
        IM_COL32(5, 5, 10, 255), IM_COL32(5, 5, 10, 255));
    
    g_fadeAlpha = fminf(g_fadeAlpha + ImGui::GetIO().DeltaTime * 3.0f, 1.0f);
    
    float w = 300.0f;
    float x = (ws.x - w) * 0.5f;
    
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ws);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    
    if (ImGui::Begin("##reg", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_fadeAlpha);
        
        // Window controls
        DrawWindowControls(dl, ws);
        
        // Back button
        ImGui::SetCursorPos(ImVec2(15, 15));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.3f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        if (ImGui::Button("<", ImVec2(30, 30))) {
            g_currentScreen = Screen::Login;
            g_fadeAlpha = 0.0f;
            g_errorMsg = "";
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        
        // Logo + Name
        dl->AddCircleFilled(ImVec2(ws.x * 0.5f, 70), 28.0f, theme::gradientA, 32);
        
        ImGui::SetCursorPos(ImVec2(0, 110));
        float tw = ImGui::CalcTextSize(PROJECT_NAME).x;
        ImGui::SetCursorPosX((ws.x - tw) * 0.5f);
        ImGui::TextColored(theme::text, PROJECT_NAME);
        
        ImGui::SetCursorPosX((ws.x - ImGui::CalcTextSize("Create new account").x) * 0.5f);
        ImGui::TextColored(theme::textDim, "Create new account");
        
        ImGui::Dummy(ImVec2(0, 15));
        
        // Inputs
        ImGui::SetCursorPosX(x);
        ImGui::TextColored(theme::textSec, "License Key");
        ImGui::SetCursorPosX(x);
        ImGui::SetNextItemWidth(w);
        StyledInput("##key", g_licenseKey, sizeof(g_licenseKey));
        
        ImGui::Dummy(ImVec2(0, 6));
        
        ImGui::SetCursorPosX(x);
        ImGui::TextColored(theme::textSec, "Username (min 3 characters)");
        ImGui::SetCursorPosX(x);
        ImGui::SetNextItemWidth(w);
        StyledInput("##ruser", g_username, sizeof(g_username));
        
        ImGui::Dummy(ImVec2(0, 6));
        
        ImGui::SetCursorPosX(x);
        ImGui::TextColored(theme::textSec, "Password (min 6 characters)");
        ImGui::SetCursorPosX(x);
        ImGui::SetNextItemWidth(w);
        StyledInput("##rpass", g_password, sizeof(g_password), true);
        
        ImGui::Dummy(ImVec2(0, 15));
        
        // Register button
        ImGui::SetCursorPosX(x);
        if (StyledButton(g_isLoading ? "  Creating...  " : "  CREATE ACCOUNT  ", ImVec2(w, 46), !g_isLoading)) {
            DoRegister();
        }
        
        // Error message
        if (!g_errorMsg.empty()) {
            ImGui::Dummy(ImVec2(0, 8));
            ImGui::SetCursorPosX((ws.x - ImGui::CalcTextSize(g_errorMsg.c_str()).x) * 0.5f);
            ImGui::TextColored(theme::error, "%s", g_errorMsg.c_str());
        }
        
        ImGui::PopStyleVar();
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void RenderMainOld() {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImVec2 ws = ImGui::GetIO().DisplaySize;
    
    g_fadeAlpha = fminf(g_fadeAlpha + ImGui::GetIO().DeltaTime * 3.0f, 1.0f);
    
    // Background: deep gradient with subtle purple glow
    for (int y = 0; y < (int)ws.y; y += 3) {
        float t = (float)y / ws.y;
        dl->AddRectFilled(ImVec2(0, (float)y), ImVec2(ws.x, (float)y + 3),
            IM_COL32(6 + (int)(t * 6), 6 + (int)(t * 4), 12 + (int)(t * 10), 255));
    }
    // Diagonal soft highlight on top (website-like)
    dl->AddRectFilledMultiColor(
        ImVec2(0, 0), ImVec2(ws.x, ws.y * 0.5f),
        IM_COL32(40, 20, 80, 90), IM_COL32(20, 30, 90, 60),
        IM_COL32(10, 10, 30, 0),  IM_COL32(10, 10, 30, 0)
    );
    
    // Sidebar with gradient (slightly more "glass-like")
    float sidebarW = 185.0f;
    for (int y = 0; y < (int)ws.y; y += 3) {
        float t = (float)y / ws.y;
        dl->AddRectFilled(ImVec2(0, (float)y), ImVec2(sidebarW, (float)y + 3),
            IM_COL32(14 + (int)(t * 4), 14 + (int)(t * 3), 24 + (int)(t * 7), 235));
    }
    // Sidebar border with glow
    dl->AddRectFilledMultiColor(ImVec2(sidebarW - 2, 0), ImVec2(sidebarW, ws.y),
        IM_COL32(80, 60, 140, 40), IM_COL32(80, 60, 140, 10),
        IM_COL32(80, 60, 140, 10), IM_COL32(80, 60, 140, 40));
    dl->AddLine(ImVec2(sidebarW, 0), ImVec2(sidebarW, ws.y), IM_COL32(60, 50, 90, 100));
    
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ws);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    
    if (ImGui::Begin("##main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_fadeAlpha);
        
        float pulse = (sinf(g_animTimer * 1.5f) + 1.0f) * 0.5f;
        
        // ===== SIDEBAR =====
        // Logo with animated glow
        float logoX = 35, logoY = 32;
        for (int r = 24; r >= 16; r -= 2) {
            dl->AddCircleFilled(ImVec2(logoX, logoY), (float)r, 
                IM_COL32(140, 90, 245, (24 - r) * 3 + (int)(pulse * 10)), 32);
        }
        dl->AddCircleFilled(ImVec2(logoX, logoY), 16, theme::gradientA, 32);
        dl->AddCircle(ImVec2(logoX, logoY), 17, IM_COL32(255, 255, 255, 40), 32, 1.5f);
        dl->AddText(ImVec2(logoX - 4, logoY - 8), IM_COL32(255, 255, 255, 255), "S");
        
        ImGui::SetCursorPos(ImVec2(58, 18));
        ImGui::TextColored(theme::text, "Single");
        ImGui::SetCursorPos(ImVec2(58, 34));
        ImGui::TextColored(theme::accent, "Project");
        
        // User card with gradient
        ImVec2 userCardPos(8, 62);
        ImVec2 userCardSize(sidebarW - 16, 48);
        dl->AddRectFilled(userCardPos, ImVec2(userCardPos.x + userCardSize.x, userCardPos.y + userCardSize.y),
            IM_COL32(28, 26, 40, 255), 10.0f);
        dl->AddRect(userCardPos, ImVec2(userCardPos.x + userCardSize.x, userCardPos.y + userCardSize.y),
            IM_COL32(60, 50, 90, 60), 10.0f);
        
        // User avatar
        dl->AddCircleFilled(ImVec2(userCardPos.x + 22, userCardPos.y + 24), 12, IM_COL32(100, 80, 160, 255), 20);
        // User icon (simple circle with letter)
        dl->AddText(ImVec2(userCardPos.x + 18, userCardPos.y + 16), IM_COL32(255, 255, 255, 255), "U");
        
        ImGui::SetCursorPos(ImVec2(userCardPos.x + 42, userCardPos.y + 8));
        ImGui::TextColored(theme::textDim, "Welcome");
        ImGui::SetCursorPos(ImVec2(userCardPos.x + 42, userCardPos.y + 24));
        std::string truncName = g_displayName.length() > 12 ? g_displayName.substr(0, 11) + ".." : g_displayName;
        ImGui::TextColored(theme::text, "%s", truncName.c_str());
        
        // Games section header
        ImGui::SetCursorPos(ImVec2(12, 122));
        ImGui::TextColored(theme::textDim, "-- GAMES --");
        
        float gameY = 145;
        for (int i = 0; i < (int)g_games.size(); i++) {
            const auto& game = g_games[i];
            bool selected = (i == g_selectedGame);
            bool hovered = false;
            
            ImVec2 btnPos(8, gameY);
            ImVec2 btnSize(sidebarW - 16, 52);
            
            // Check hover
            ImVec2 mousePos = ImGui::GetMousePos();
            if (mousePos.x >= btnPos.x && mousePos.x <= btnPos.x + btnSize.x &&
                mousePos.y >= btnPos.y && mousePos.y <= btnPos.y + btnSize.y) {
                hovered = true;
            }
            
            // Background
            if (selected) {
                dl->AddRectFilled(btnPos, ImVec2(btnPos.x + btnSize.x, btnPos.y + btnSize.y),
                    IM_COL32(100, 70, 180, 50), 10.0f);
                dl->AddRect(btnPos, ImVec2(btnPos.x + btnSize.x, btnPos.y + btnSize.y),
                    IM_COL32(140, 100, 220, 100), 10.0f);
                // Selection indicator
                dl->AddRectFilled(ImVec2(0, btnPos.y + 6), ImVec2(4, btnPos.y + btnSize.y - 6), 
                    theme::gradientA, 2.0f);
            } else if (hovered) {
                dl->AddRectFilled(btnPos, ImVec2(btnPos.x + btnSize.x, btnPos.y + btnSize.y),
                    IM_COL32(45, 42, 60, 255), 10.0f);
            }
            
            // Game icon with ring
            float iconX = btnPos.x + 24;
            float iconY = btnPos.y + 26;
            ImU32 iconBg = game.available ? IM_COL32(100, 70, 180, 255) : IM_COL32(55, 55, 70, 255);
            ImU32 iconRing = game.available ? IM_COL32(140, 100, 220, 150) : IM_COL32(70, 70, 85, 100);
            
            dl->AddCircleFilled(ImVec2(iconX, iconY), 15, iconBg, 24);
            dl->AddCircle(ImVec2(iconX, iconY), 16, iconRing, 24, 1.5f);
            
            ImVec2 iconSize = ImGui::CalcTextSize(game.icon.c_str());
            dl->AddText(ImVec2(iconX - iconSize.x * 0.5f, iconY - iconSize.y * 0.5f),
                IM_COL32(255, 255, 255, game.available ? 255 : 120), game.icon.c_str());
            
            // Game name
            ImU32 nameCol = game.available ? IM_COL32(240, 240, 250, 255) : IM_COL32(120, 120, 135, 255);
            dl->AddText(ImVec2(btnPos.x + 48, btnPos.y + 11), nameCol, game.name.c_str());
            
            // Status with dot indicator
            const char* status;
            ImU32 statusCol, dotCol;
            if (game.hasLicense) {
                status = game.daysRemaining < 0 ? "Lifetime" : "Active";
                statusCol = IM_COL32(80, 220, 140, 255);
                dotCol = IM_COL32(80, 220, 140, 255);
            } else if (game.available) {
                status = "Not active";
                statusCol = IM_COL32(255, 180, 80, 255);
                dotCol = IM_COL32(255, 180, 80, 255);
            } else {
                status = "Soon";
                statusCol = IM_COL32(100, 100, 115, 255);
                dotCol = IM_COL32(100, 100, 115, 255);
            }
            
            dl->AddCircleFilled(ImVec2(btnPos.x + 52, btnPos.y + 35), 3, dotCol, 12);
            dl->AddText(ImVec2(btnPos.x + 60, btnPos.y + 28), statusCol, status);
            
            // Click handler
            ImGui::SetCursorPos(btnPos);
            ImGui::InvisibleButton(("game" + std::to_string(i)).c_str(), btnSize);
            if (ImGui::IsItemClicked()) {
                g_selectedGame = i;
                g_errorMsg = "";
                g_successMsg = "";
            }
            
            gameY += 56;
        }
        
        // Logout button - premium style
        ImVec2 logoutPos(8, ws.y - 55);
        ImVec2 logoutSize(sidebarW - 16, 42);
        
        bool logoutHovered = false;
        ImVec2 mousePos = ImGui::GetMousePos();
        if (mousePos.x >= logoutPos.x && mousePos.x <= logoutPos.x + logoutSize.x &&
            mousePos.y >= logoutPos.y && mousePos.y <= logoutPos.y + logoutSize.y) {
            logoutHovered = true;
        }
        
        dl->AddRectFilled(logoutPos, ImVec2(logoutPos.x + logoutSize.x, logoutPos.y + logoutSize.y),
            logoutHovered ? IM_COL32(70, 35, 35, 255) : IM_COL32(45, 30, 30, 255), 10.0f);
        dl->AddRect(logoutPos, ImVec2(logoutPos.x + logoutSize.x, logoutPos.y + logoutSize.y),
            IM_COL32(120, 60, 60, logoutHovered ? 150 : 80), 10.0f);
        
        ImVec2 logoutText = ImGui::CalcTextSize("< Logout");
        dl->AddText(ImVec2(logoutPos.x + (logoutSize.x - logoutText.x) * 0.5f, logoutPos.y + 12),
            IM_COL32(255, 180, 180, 255), "< Logout");
        
        ImGui::SetCursorPos(logoutPos);
        if (ImGui::InvisibleButton("##logout", logoutSize)) {
            DoLogout();
            ImGui::PopStyleVar();
            ImGui::End();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            return;
        }
        
        // ===== CONTENT AREA =====
        float contentX = sidebarW + 25;
        float contentW = ws.x - sidebarW - 50;
        
        GameInfo& game = g_games[g_selectedGame];

        // Large "glass" panel behind main content (like a section on the website)
        ImVec2 panelPos(contentX - 15.0f, 82.0f);
        ImVec2 panelSize(contentW + 30.0f, ws.y - 120.0f);
        dl->AddRectFilled(panelPos, ImVec2(panelPos.x + panelSize.x, panelPos.y + panelSize.y),
            IM_COL32(10, 10, 20, 210), 22.0f);
        dl->AddRect(panelPos, ImVec2(panelPos.x + panelSize.x, panelPos.y + panelSize.y),
            IM_COL32(90, 70, 160, 160), 22.0f);
        
        // ===== TOP BAR =====
        // Gradient header background
        ImVec2 headerPos(sidebarW, 0);
        ImVec2 headerSize(ws.x - sidebarW, 75);
        for (int i = 0; i < (int)headerSize.x; i += 2) {
            float t = (float)i / headerSize.x;
            ImU32 col = IM_COL32(
                (int)(18 + t * 8),
                (int)(18 + t * 5),
                (int)(28 + t * 12),
                255
            );
            dl->AddRectFilled(
                ImVec2(headerPos.x + i, headerPos.y),
                ImVec2(headerPos.x + i + 2, headerPos.y + headerSize.y),
                col
            );
        }
        dl->AddLine(ImVec2(sidebarW, 75), ImVec2(ws.x, 75), IM_COL32(60, 50, 90, 100));
        
        // Window controls (top right) - Manually drawn for perfect centering
        float btnSize = 26.0f;
        float btnY = 10.0f;
        
        // Refresh
        ImVec2 refBtn(ws.x - 110, btnY);
        bool refHovered = false;
        if (mousePos.x >= refBtn.x && mousePos.x <= refBtn.x + btnSize &&
            mousePos.y >= refBtn.y && mousePos.y <= refBtn.y + btnSize) refHovered = true;
            
        dl->AddRectFilled(refBtn, ImVec2(refBtn.x + btnSize, refBtn.y + btnSize), 
            refHovered ? IM_COL32(60, 90, 60, 255) : IM_COL32(45, 55, 45, 240), 13.0f);
            
        ImVec2 refTextSize = ImGui::CalcTextSize("@");
        dl->AddText(ImVec2(refBtn.x + (btnSize - refTextSize.x) * 0.5f, refBtn.y + (btnSize - refTextSize.y) * 0.5f - 1), 
            IM_COL32(255, 255, 255, 255), "@");
            
        ImGui::SetCursorPos(refBtn);
        if (ImGui::InvisibleButton("##ref", ImVec2(btnSize, btnSize))) { RefreshData(); FetchGameStatus(); }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Refresh");
        
        // Minimize
        ImVec2 minBtn(ws.x - 78, btnY);
        bool minHovered = false;
        if (mousePos.x >= minBtn.x && mousePos.x <= minBtn.x + btnSize &&
            mousePos.y >= minBtn.y && mousePos.y <= minBtn.y + btnSize) minHovered = true;
            
        dl->AddRectFilled(minBtn, ImVec2(minBtn.x + btnSize, minBtn.y + btnSize), 
            minHovered ? IM_COL32(80, 80, 110, 255) : IM_COL32(55, 55, 70, 240), 13.0f);
            
        ImVec2 minTextSize = ImGui::CalcTextSize("_");
        dl->AddText(ImVec2(minBtn.x + (btnSize - minTextSize.x) * 0.5f, minBtn.y + (btnSize - minTextSize.y) * 0.5f - 2), 
            IM_COL32(255, 255, 255, 255), "_");
            
        ImGui::SetCursorPos(minBtn);
        if (ImGui::InvisibleButton("##min", ImVec2(btnSize, btnSize))) { ShowWindow(g_hwnd, SW_MINIMIZE); }
        
        // Close
        ImVec2 clsBtn(ws.x - 46, btnY);
        bool clsHovered = false;
        if (mousePos.x >= clsBtn.x && mousePos.x <= clsBtn.x + btnSize &&
            mousePos.y >= clsBtn.y && mousePos.y <= clsBtn.y + btnSize) clsHovered = true;
            
        dl->AddRectFilled(clsBtn, ImVec2(clsBtn.x + btnSize, clsBtn.y + btnSize), 
            clsHovered ? IM_COL32(180, 60, 60, 255) : IM_COL32(115, 45, 45, 240), 13.0f);
            
        ImVec2 clsTextSize = ImGui::CalcTextSize("X");
        dl->AddText(ImVec2(clsBtn.x + (btnSize - clsTextSize.x) * 0.5f, clsBtn.y + (btnSize - clsTextSize.y) * 0.5f), 
            IM_COL32(255, 255, 255, 255), "X");
            
        ImGui::SetCursorPos(clsBtn);
        if (ImGui::InvisibleButton("##cls", ImVec2(btnSize, btnSize))) { PostQuitMessage(0); }
        
        // Game icon circle with glow
        float iconX = contentX + 28;
        float iconY = 38;
        ImU32 glowCol = game.available ? IM_COL32(140, 90, 245, (int)(40 + pulse * 30)) : IM_COL32(80, 80, 100, 40);
        for (int r = 30; r >= 22; r -= 2) {
            dl->AddCircleFilled(ImVec2(iconX, iconY), (float)r, IM_COL32(140, 90, 245, (30 - r) * 4), 32);
        }
        dl->AddCircleFilled(ImVec2(iconX, iconY), 22, game.available ? theme::gradientA : IM_COL32(60, 60, 80, 255), 32);
        dl->AddCircle(ImVec2(iconX, iconY), 23, IM_COL32(255, 255, 255, 30), 32, 2.0f);
        
        ImVec2 iconTextSize = ImGui::CalcTextSize(game.icon.c_str());
        dl->AddText(ImVec2(iconX - iconTextSize.x * 0.5f, iconY - iconTextSize.y * 0.5f),
            IM_COL32(255, 255, 255, game.available ? 255 : 150), game.icon.c_str());
        
        // Game name & status
        ImGui::SetCursorPos(ImVec2(contentX + 60, 18));
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Use default font for now
        ImGui::TextColored(theme::text, "%s", game.name.c_str());
        ImGui::PopFont();
        
        ImGui::SetCursorPos(ImVec2(contentX + 60, 38));
        ImGui::TextColored(theme::textDim, "v%s", game.version.c_str());
        ImGui::SameLine();
        
        // Status badge
        ImVec4 statusCol;
        const char* statusText;
        if (g_gameStatus == "operational") {
            statusCol = ImVec4(0.15f, 0.85f, 0.45f, 1.0f);
            statusText = "ONLINE";
        } else if (g_gameStatus == "updating") {
            statusCol = ImVec4(1.0f, 0.75f, 0.0f, 1.0f);
            statusText = "UPDATING";
        } else if (g_gameStatus == "offline") {
            statusCol = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            statusText = "OFFLINE";
        } else {
            statusCol = ImVec4(0.5f, 0.5f, 0.55f, 1.0f);
            statusText = "UNKNOWN";
        }
        
        ImGui::TextColored(statusCol, "%s", statusText);
        
        // Show last refresh time or loading indicator
        ImGui::SameLine();
        if (g_statusRefreshing) {
            // Animated loading dots
            int dots = ((int)(g_animTimer * 3)) % 4;
            const char* loadingText[] = {"   ", ".  ", ".. ", "..."};
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.3f, 1.0f), " syncing%s", loadingText[dots]);
        } else {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_lastRefresh).count();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.55f, 0.7f), " (%llds ago)", elapsed);
        }
        
        // ===== MAIN CARDS AREA =====
        float cardY = 90;
        float cardSpacing = 12;
        float cardRadius = 14.0f;
        
        // License Card - premium gradient border
        ImVec2 licPos(contentX, cardY);
        ImVec2 licSize(contentW, 80);
        
        // Card background with subtle gradient
        for (int i = 0; i < (int)licSize.y; i += 2) {
            float t = (float)i / licSize.y;
            dl->AddRectFilled(
                ImVec2(licPos.x, licPos.y + i),
                ImVec2(licPos.x + licSize.x, licPos.y + i + 2),
                IM_COL32(22 + (int)(t * 5), 22 + (int)(t * 3), 35 + (int)(t * 8), 255),
                i == 0 ? cardRadius : 0
            );
        }
        dl->AddRect(licPos, ImVec2(licPos.x + licSize.x, licPos.y + licSize.y), 
            IM_COL32(70, 60, 100, 80), cardRadius);
        
        // License icon
        float licIconX = licPos.x + 30;
        float licIconY = licPos.y + 40;
        dl->AddCircleFilled(ImVec2(licIconX, licIconY), 18, 
            game.hasLicense ? IM_COL32(40, 180, 100, 255) : IM_COL32(180, 60, 60, 255), 24);
        dl->AddText(ImVec2(licIconX - 6, licIconY - 8), IM_COL32(255, 255, 255, 255), 
            game.hasLicense ? "+" : "-");
        
        ImGui::SetCursorPos(ImVec2(licPos.x + 58, licPos.y + 16));
        ImGui::TextColored(theme::textSec, "LICENSE STATUS");
        
        ImGui::SetCursorPos(ImVec2(licPos.x + 58, licPos.y + 38));
        if (game.hasLicense) {
            ImGui::TextColored(theme::success, "ACTIVE");
            ImGui::SameLine();
            if (game.daysRemaining < 0) {
                ImGui::TextColored(ImVec4(0.9f, 0.75f, 0.3f, 1.0f), "LIFETIME");
            } else {
                ImGui::TextColored(theme::text, "%d days remaining", game.daysRemaining);
            }
        } else {
            ImGui::TextColored(theme::error, "NOT ACTIVATED");
        }
        
        // Progress bar for license time (if has license and not lifetime)
        if (game.hasLicense && game.daysRemaining > 0 && game.daysRemaining <= 365) {
            float progress = (float)game.daysRemaining / 30.0f;
            if (progress > 1.0f) progress = 1.0f;
            ImVec2 barPos(licPos.x + 58, licPos.y + 60);
            ImVec2 barSize(licSize.x - 80, 6);
            dl->AddRectFilled(barPos, ImVec2(barPos.x + barSize.x, barPos.y + barSize.y), 
                IM_COL32(40, 40, 55, 255), 3.0f);
            dl->AddRectFilled(barPos, ImVec2(barPos.x + barSize.x * progress, barPos.y + barSize.y), 
                progress > 0.3f ? IM_COL32(60, 200, 120, 255) : IM_COL32(255, 150, 50, 255), 3.0f);
        }
        
        cardY = licPos.y + licSize.y + cardSpacing;
        
        // Activate License Card (if no license)
        if (!game.hasLicense && game.available) {
            ImVec2 actPos(contentX, cardY);
            ImVec2 actSize(contentW, 75);
            
            // Gradient background
            for (int i = 0; i < (int)actSize.y; i += 2) {
                float t = (float)i / actSize.y;
                dl->AddRectFilled(
                    ImVec2(actPos.x, actPos.y + i),
                    ImVec2(actPos.x + actSize.x, actPos.y + i + 2),
                    IM_COL32(30 + (int)(t * 5), 22 + (int)(t * 3), 45 + (int)(t * 10), 255),
                    i == 0 ? cardRadius : 0
                );
            }
            dl->AddRect(actPos, ImVec2(actPos.x + actSize.x, actPos.y + actSize.y), 
                IM_COL32(140, 90, 200, 60), cardRadius);
            
            ImGui::SetCursorPos(ImVec2(actPos.x + 16, actPos.y + 12));
            ImGui::TextColored(theme::accent, "ACTIVATE LICENSE");
            
            ImGui::SetCursorPos(ImVec2(actPos.x + 16, actPos.y + 38));
            ImGui::SetNextItemWidth(actSize.x - 140);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.18f, 1.0f));
            ImGui::InputTextWithHint("##actkey", "Enter license key...", g_activateKey, sizeof(g_activateKey));
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            
            ImGui::SameLine();
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.28f, 0.85f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.38f, 0.95f, 1.0f));
            if (ImGui::Button("ACTIVATE", ImVec2(100, 32)) && !g_isLoading) {
                ActivateLicense(game.id);
            }
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
            
            cardY = actPos.y + actSize.y + cardSpacing;
        }
        
        // Features Card
        ImVec2 featPos(contentX, cardY);
        ImVec2 featSize(contentW, 70);
        
        // Card bg
        dl->AddRectFilled(featPos, ImVec2(featPos.x + featSize.x, featPos.y + featSize.y), 
            IM_COL32(22, 22, 35, 255), cardRadius);
        dl->AddRect(featPos, ImVec2(featPos.x + featSize.x, featPos.y + featSize.y), 
            IM_COL32(50, 50, 70, 80), cardRadius);
        
        ImGui::SetCursorPos(ImVec2(featPos.x + 16, featPos.y + 12));
        ImGui::TextColored(theme::textSec, "FEATURES");
        
        // Feature badges
        float badgeX = featPos.x + 16;
        float badgeY = featPos.y + 38;
        
        if (game.available && !game.description.empty()) {
            // Parse features (comma separated)
            std::string desc = game.description;
            std::vector<std::string> features;
            size_t pos = 0;
            while ((pos = desc.find(",")) != std::string::npos) {
                std::string feat = desc.substr(0, pos);
                if (!feat.empty()) features.push_back(feat);
                desc.erase(0, pos + 1);
            }
            if (!desc.empty()) features.push_back(desc);
            
            for (const auto& feat : features) {
                // Trim whitespace
                std::string trimmed = feat;
                while (!trimmed.empty() && trimmed[0] == ' ') trimmed.erase(0, 1);
                while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
                
                ImVec2 textSize = ImGui::CalcTextSize(trimmed.c_str());
                float badgeW = textSize.x + 20;
                
                if (badgeX + badgeW > featPos.x + featSize.x - 16) {
                    badgeX = featPos.x + 16;
                    badgeY += 26;
                }
                
                dl->AddRectFilled(ImVec2(badgeX, badgeY), ImVec2(badgeX + badgeW, badgeY + 22),
                    IM_COL32(60, 45, 100, 200), 6.0f);
                dl->AddRect(ImVec2(badgeX, badgeY), ImVec2(badgeX + badgeW, badgeY + 22),
                    IM_COL32(140, 100, 200, 100), 6.0f);
                dl->AddText(ImVec2(badgeX + 10, badgeY + 3), IM_COL32(220, 200, 255, 255), trimmed.c_str());
                
                badgeX += badgeW + 8;
            }
        } else {
            dl->AddText(ImVec2(badgeX, badgeY), IM_COL32(100, 100, 120, 255), "Coming soon...");
        }
        
        cardY = featPos.y + featSize.y + cardSpacing + 5;
        
        // ===== LAUNCH BUTTON =====
        ImVec2 launchPos(contentX, cardY);
        ImVec2 launchSize(contentW, 55);
        
        if (g_isDownloading) {
            // Download progress bar
            dl->AddRectFilled(launchPos, ImVec2(launchPos.x + launchSize.x, launchPos.y + launchSize.y),
                IM_COL32(25, 25, 40, 255), 12.0f);
            
            float prog = g_downloadProgress.load();
            dl->AddRectFilled(launchPos, ImVec2(launchPos.x + launchSize.x * prog, launchPos.y + launchSize.y),
                IM_COL32(100, 70, 200, 255), 12.0f);
            
            char progText[32];
            sprintf_s(progText, "Downloading... %.0f%%", prog * 100.0f);
            ImVec2 ptSize = ImGui::CalcTextSize(progText);
            dl->AddText(ImVec2(launchPos.x + (launchSize.x - ptSize.x) * 0.5f, launchPos.y + 18),
                IM_COL32(255, 255, 255, 255), progText);
        } else {
            bool canLaunch = game.hasLicense && game.available && !g_cheatRunning && g_gameStatus == "operational";
            
            // Button gradient
            ImU32 btnColA = canLaunch ? IM_COL32(120, 80, 220, 255) : IM_COL32(50, 50, 65, 255);
            ImU32 btnColB = canLaunch ? IM_COL32(90, 50, 180, 255) : IM_COL32(40, 40, 55, 255);
            
            for (int i = 0; i < (int)launchSize.y; i += 2) {
                float t = (float)i / launchSize.y;
                ImU32 col = IM_COL32(
                    (int)((1 - t) * ((btnColA >> 0) & 0xFF) + t * ((btnColB >> 0) & 0xFF)),
                    (int)((1 - t) * ((btnColA >> 8) & 0xFF) + t * ((btnColB >> 8) & 0xFF)),
                    (int)((1 - t) * ((btnColA >> 16) & 0xFF) + t * ((btnColB >> 16) & 0xFF)),
                    255
                );
                dl->AddRectFilled(
                    ImVec2(launchPos.x, launchPos.y + i),
                    ImVec2(launchPos.x + launchSize.x, launchPos.y + i + 2),
                    col, i == 0 ? 12.0f : 0
                );
            }
            
            // Glow effect when can launch
            if (canLaunch) {
                dl->AddRect(launchPos, ImVec2(launchPos.x + launchSize.x, launchPos.y + launchSize.y),
                    IM_COL32(180, 140, 255, (int)(60 + pulse * 40)), 12.0f, 0, 2.0f);
            }
            
            const char* btnText = g_cheatRunning ? "RUNNING" : (canLaunch ? "> LAUNCH" : "LAUNCH");
            ImVec2 textSize = ImGui::CalcTextSize(btnText);
            dl->AddText(ImVec2(launchPos.x + (launchSize.x - textSize.x) * 0.5f, launchPos.y + 18),
                canLaunch ? IM_COL32(255, 255, 255, 255) : IM_COL32(120, 120, 140, 255), btnText);
            
            // Invisible button for click
            ImGui::SetCursorPos(launchPos);
            if (ImGui::InvisibleButton("##launch", launchSize) && canLaunch) {
                LaunchGame(g_selectedGame);
            }
        }
        
        // ===== MESSAGES =====
        cardY = launchPos.y + launchSize.y + 12;
        if (!g_errorMsg.empty()) {
            ImVec2 msgPos(contentX, cardY);
            dl->AddRectFilled(msgPos, ImVec2(msgPos.x + contentW, msgPos.y + 30),
                IM_COL32(80, 30, 30, 200), 8.0f);
            ImGui::SetCursorPos(ImVec2(msgPos.x + 12, msgPos.y + 6));
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 1.0f), "! %s", g_errorMsg.c_str());
        }
        if (!g_successMsg.empty()) {
            ImVec2 msgPos(contentX, cardY);
            dl->AddRectFilled(msgPos, ImVec2(msgPos.x + contentW, msgPos.y + 30),
                IM_COL32(30, 80, 50, 200), 8.0f);
            ImGui::SetCursorPos(ImVec2(msgPos.x + 12, msgPos.y + 6));
            ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.7f, 1.0f), "%s", g_successMsg.c_str());
        }
        
        // Footer with gradient line
        dl->AddRectFilledMultiColor(
            ImVec2(sidebarW, ws.y - 35), ImVec2(ws.x, ws.y - 34),
            IM_COL32(60, 50, 90, 0), IM_COL32(60, 50, 90, 60),
            IM_COL32(60, 50, 90, 60), IM_COL32(60, 50, 90, 0)
        );
        
        ImGui::SetCursorPos(ImVec2(contentX, ws.y - 25));
        ImGui::TextColored(theme::textDim, PROJECT_NAME);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.55f, 0.45f, 0.80f, 0.8f), " v" LAUNCHER_VERSION);
        
        // ===== FORCED UPDATE NOTIFICATION =====
        if (g_updateAvailable && !g_updateDownloading) {
            // Dim the entire background - forced update
            dl->AddRectFilled(ImVec2(0, 0), ws, IM_COL32(0, 0, 0, 180));
            
            // Center popup
            ImVec2 popupSize(320, 140);
            ImVec2 popupPos((ws.x - popupSize.x) / 2, (ws.y - popupSize.y) / 2);
            
            // Background
            dl->AddRectFilled(popupPos, ImVec2(popupPos.x + popupSize.x, popupPos.y + popupSize.y), 
                IM_COL32(25, 25, 40, 255), 14.0f);
            dl->AddRect(popupPos, ImVec2(popupPos.x + popupSize.x, popupPos.y + popupSize.y), 
                IM_COL32(100, 80, 200, 255), 14.0f, 0, 2.0f);
            
            // Title
            const char* title = "Update Required";
            ImVec2 titleSize = ImGui::CalcTextSize(title);
            dl->AddText(ImVec2(popupPos.x + (popupSize.x - titleSize.x) / 2, popupPos.y + 20), 
                IM_COL32(255, 255, 255, 255), title);
            
            // Version info
            char verInfo[64];
            sprintf_s(verInfo, "v%s  ->  v%s", LAUNCHER_VERSION, g_newVersion.c_str());
            ImVec2 verSize = ImGui::CalcTextSize(verInfo);
            dl->AddText(ImVec2(popupPos.x + (popupSize.x - verSize.x) / 2, popupPos.y + 50), 
                IM_COL32(150, 200, 150, 255), verInfo);
            
            // Message
            const char* msg = "A new version is available. Please update.";
            ImVec2 msgSize = ImGui::CalcTextSize(msg);
            dl->AddText(ImVec2(popupPos.x + (popupSize.x - msgSize.x) / 2, popupPos.y + 75), 
                IM_COL32(180, 180, 200, 255), msg);
            
            // Update button (centered, no close button - forced update!)
            ImGui::SetCursorPos(ImVec2(popupPos.x + (popupSize.x - 200) / 2, popupPos.y + 100));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.30f, 0.85f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.40f, 0.95f, 1.0f));
            if (ImGui::Button("UPDATE NOW", ImVec2(200, 32))) {
                std::thread([]() {
                    if (DownloadUpdate()) {
                        ApplyUpdate();
                    } else {
                        g_errorMsg = "Update download failed. Restart the launcher.";
                        g_updateAvailable = false;
                    }
                }).detach();
            }
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
        }
        
        // ===== UPDATE PROGRESS =====
        if (g_updateDownloading) {
            ImVec2 popupPos(ws.x - 270, ws.y - 80);
            ImVec2 popupSize(260, 70);
            
            dl->AddRectFilled(popupPos, ImVec2(popupPos.x + popupSize.x, popupPos.y + popupSize.y), 
                IM_COL32(20, 60, 100, 240), 10.0f);
            
            ImGui::SetCursorPos(ImVec2(popupPos.x + 15, popupPos.y + 12));
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Downloading update...");
            
            // Progress bar
            float progress = g_updateProgress.load();
            ImVec2 barPos(popupPos.x + 15, popupPos.y + 38);
            ImVec2 barSize(230, 18);
            dl->AddRectFilled(barPos, ImVec2(barPos.x + barSize.x, barPos.y + barSize.y), 
                IM_COL32(30, 30, 50, 255), 6.0f);
            dl->AddRectFilled(barPos, ImVec2(barPos.x + barSize.x * progress, barPos.y + barSize.y), 
                IM_COL32(80, 180, 255, 255), 6.0f);
            
            char progressText[32];
            sprintf_s(progressText, "%.0f%%", progress * 100);
            ImVec2 textSize = ImGui::CalcTextSize(progressText);
            dl->AddText(ImVec2(barPos.x + barSize.x / 2 - textSize.x / 2, barPos.y + 2), 
                IM_COL32(255, 255, 255, 255), progressText);
        }
        
        ImGui::PopStyleVar();
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================
// NEW MODERN UI - Tab-based interface
// ============================================
void RenderMain() {
    ImGui::SetNextWindowSize(ImVec2((float)WINDOW_WIDTH, (float)WINDOW_HEIGHT));
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme::bg_deep);
    
    ImGui::Begin("##EXTERNA_MAIN", nullptr, 
        ImGuiWindowFlags_NoTitleBar | 
        ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse);
    
    // Get draw list for particles
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    
    // Render particles first (background layer)
    ui::RenderParticles(draw, windowPos);
    
    // Top bar
    ui::RenderTopBar(g_username[0] ? g_username : "Guest");
    
    // Main layout: Sidebar + Content
    ImGui::SetCursorPos(ImVec2(0, 60));
    ImGui::BeginGroup();
    {
        // Sidebar navigation
        ui::RenderSidebar();
        
        ImGui::SameLine();
        
        // Content area based on selected tab
        ImGui::BeginChild("##content_area", ImVec2(1120, 640), false);
        
        switch (ui::g_currentTab) {
            case ui::Tab::Home: {
                ui::RenderHomeTab({});
                
                // Show selected game info in Home tab
                if (g_selectedGame >= 0 && g_selectedGame < (int)g_games.size()) {
                    GameInfo& game = g_games[g_selectedGame];
                    
                    ImGui::SetCursorPos(ImVec2(40, 200));
                    ImGui::BeginChild("##game_details", ImVec2(700, 400), true);
                    
                    // Game header
                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
                    ImGui::TextColored(theme::text, "%s %s", game.icon.c_str(), game.name.c_str());
                    ImGui::PopFont();
                    
                    ImGui::Spacing();
                    ImGui::TextColored(theme::text_secondary, "%s", game.description.c_str());
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    // Status indicator
                    theme::StatusIndicator(
                        g_gameStatusMsg.c_str(),
                        g_gameStatus == "operational",
                        g_gameStatus == "operational" ? theme::success : theme::warning
                    );
                    
                    ImGui::Spacing();
                    
                    // Version info
                    ImGui::TextColored(theme::text_secondary, "Version: ");
                    ImGui::SameLine();
                    ImGui::TextColored(theme::text, "%s", game.version.c_str());
                    
                    // License status
                    ImGui::TextColored(theme::text_secondary, "License: ");
                    ImGui::SameLine();
                    if (game.hasLicense) {
                        if (game.daysRemaining < 0) {
                            ImGui::TextColored(theme::success, "Lifetime");
                        } else {
                            ImGui::TextColored(theme::success, "Active (%d days left)", game.daysRemaining);
                        }
                    } else {
                        ImGui::TextColored(theme::error, "Not activated");
                    }
                    
                    ImGui::Spacing();
                    ImGui::Spacing();
                    
                    // Action buttons
                    if (game.hasLicense && g_gameStatus == "operational") {
                        if (g_cheatRunning) {
                            ImGui::PushStyleColor(ImGuiCol_Button, theme::error);
                            if (ImGui::Button("🛑 STOP CHEAT", ImVec2(200, 50))) {
                                g_cheatRunning = false; // Simple stop
                            }
                            ImGui::PopStyleColor();
                        } else {
                            ImGui::PushStyleColor(ImGuiCol_Button, theme::success);
                            if (ImGui::Button("🚀 LAUNCH", ImVec2(200, 50))) {
                                LaunchGame(g_selectedGame); // Use existing function
                            }
                            ImGui::PopStyleColor();
                        }
                    } else if (!game.hasLicense) {
                        ImGui::PushStyleColor(ImGuiCol_Button, theme::warning);
                        ImGui::Button("[!] LICENSE REQUIRED", ImVec2(200, 50));
                        ImGui::PopStyleColor();
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Button, theme::text_secondary);
                        ImGui::Button("[X] NOT AVAILABLE", ImVec2(200, 50));
                        ImGui::PopStyleColor();
                    }
                    
                    // Download progress
                    if (g_isDownloading) {
                        ImGui::Spacing();
                        theme::ProgressBar(g_downloadProgress, ImVec2(400, 30), "Downloading...");
                    }
                    
                    // Loading spinner
                    if (g_isLoading) {
                        ImGui::SameLine();
                        theme::Spinner("##loading", 15, 6, ImGui::ColorConvertFloat4ToU32(theme::primary));
                    }
                    
                    ImGui::EndChild();
                }
                break;
            }
            
            case ui::Tab::Games:
                ui::RenderGamesTab();
                break;
            
            case ui::Tab::Stats:
                ui::RenderStatsTab();
                break;
            
            case ui::Tab::Settings:
                ui::RenderSettingsTab();
                break;
            
            case ui::Tab::Profile:
                ui::RenderProfileTab(g_username);
                break;
            
            case ui::Tab::News:
                ui::RenderNewsTab();
                break;
        }
        
        ImGui::EndChild();
    }
    ImGui::EndGroup();
    
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void SetupStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0;
    style.FrameRounding = 8.0f;
    style.WindowPadding = ImVec2(0, 0);
    style.FramePadding = ImVec2(12, 10);
    style.ItemSpacing = ImVec2(10, 8);
    
    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg] = theme::bg;
    c[ImGuiCol_FrameBg] = theme::surface;
    c[ImGuiCol_FrameBgHovered] = theme::surfaceHover;
    c[ImGuiCol_Button] = theme::accent;
    c[ImGuiCol_ButtonHovered] = theme::accentHover;
    c[ImGuiCol_Text] = theme::text;
    c[ImGuiCol_CheckMark] = theme::accent;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // ============================================
    // LOAD CONFIGURATION
    // ============================================
    Config::Load(); // Load saved settings
    
    // ============================================
    // PROTECTION CHECKS
    // ============================================
    int protResult = Protection::RunProtectionChecks();
    
    if (protResult == 1) {
        // Debugger detected - silent exit
        Protection::SecureExit(0);
    }
    
    if (protResult == 2 && !g_bypassVM) {
        // VM detected - show warning (can be bypassed for dev)
        MessageBoxA(NULL, "This software cannot run in virtual machines.", 
                   "Security", MB_ICONWARNING | MB_OK);
        Protection::SecureExit(0);
    }
    
    if (protResult == 3 || protResult == 4) {
        // Integrity/hooks failed - silent exit
        Protection::SecureExit(0);
    }
    
    g_protectionPassed = true;
    
    // ============================================
    // Normal startup
    // ============================================
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, 
        GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"SingleProject", nullptr };
    RegisterClassExW(&wc);
    
    g_hwnd = CreateWindowExW(WS_EX_TOPMOST, wc.lpszClassName, L"Single-Project",
        WS_POPUP,
        (GetSystemMetrics(SM_CXSCREEN) - WINDOW_WIDTH) / 2,
        (GetSystemMetrics(SM_CYSCREEN) - WINDOW_HEIGHT) / 2,
        WINDOW_WIDTH, WINDOW_HEIGHT, nullptr, nullptr, wc.hInstance, nullptr);
    
    DWM_WINDOW_CORNER_PREFERENCE pref = DWMWCP_ROUND;
    DwmSetWindowAttribute(g_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
    
    if (!CreateDeviceD3D(g_hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    
    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hwnd);
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.0f);
    
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    
    // Apply modern theme
    theme::ApplyModernTheme();
    
    // Initialize particle system
    ui::InitParticles(30);
    
    g_hwid = GetHWID();
    
    // Check for updates on startup
    CheckForUpdates();
    
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    auto lastTime = std::chrono::high_resolution_clock::now();
    
    static int protCheckCounter = 0;
    
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }
        
        // Periodic protection check (every ~5 seconds at 60fps)
        if (++protCheckCounter > 300) {
            protCheckCounter = 0;
            if (Protection::IsDebuggerAttached() || Protection::HasDebuggerWindows()) {
                Protection::SecureExit(0);
            }
        }
        
        // Auto-refresh data every 10 seconds (600 frames at 60fps)
        // Auto-refresh status every 1 second (60 frames at 60fps) for instant updates
        static int statusRefreshCounter = 0;
        if (g_currentScreen == Screen::Main && ++statusRefreshCounter > 60) {
            statusRefreshCounter = 0;
            FetchGameStatus();
        }
        
        // Refresh licenses every 5 seconds
        static int licenseRefreshCounter = 0;
        if (g_currentScreen == Screen::Main && ++licenseRefreshCounter > 300) {
            licenseRefreshCounter = 0;
            FetchUserLicenses();
        }
        
        // Check for launcher updates every 30 seconds (1800 frames at 60fps)
        static int updateCheckCounter = 0;
        if (++updateCheckCounter > 1800) {
            updateCheckCounter = 0;
            CheckForUpdates();
        }
        
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        
        g_animTimer += dt;
        if (g_currentScreen == Screen::Splash) g_splashTimer += dt;
        
        // Update toasts
        UpdateToasts(dt);
        
        // Update particles
        ui::UpdateParticles(dt);
        
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        
        switch (g_currentScreen) {
            case Screen::Splash: RenderSplash(); break;
            case Screen::Login: RenderLogin(); break;
            case Screen::Register: RenderRegister(); break;
            case Screen::Main: RenderMain(); break; // Now using NEW modern UI
        }
        
        // Render toasts on top
        RenderToasts();
        
        ImGui::Render();
        const float clear[4] = { 0.04f, 0.04f, 0.06f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }
    
    // ============================================
    // CLEANUP & SAVE CONFIG
    // ============================================
    Config::Save(); // Save settings on exit
    
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(g_hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}

// ============================================
// DirectX
// ============================================
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate = {60, 1};
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    
    D3D_FEATURE_LEVEL levels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL featureLevel;
    
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, 
        levels, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, 
        &featureLevel, &g_pd3dDeviceContext) != S_OK) return false;
    
    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    
    switch (msg) {
        case WM_SIZE:
            if (g_pd3dDevice && wParam != SIZE_MINIMIZED) {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
        case WM_NCHITTEST: {
            POINT pt = { LOWORD(lParam), HIWORD(lParam) };
            ScreenToClient(hWnd, &pt);
            if (pt.y < 50 && pt.x > 180 && pt.x < WINDOW_WIDTH - 80) return HTCAPTION;
            break;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) PostQuitMessage(0);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}