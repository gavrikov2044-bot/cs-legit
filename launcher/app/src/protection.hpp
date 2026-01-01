/*
 * Single-Project Launcher Protection
 * Anti-Debug, Anti-VM, String Encryption, Integrity Check
 */

#pragma once
#include <Windows.h>
#include <intrin.h>
#include <string>
#include <vector>

namespace Protection {

// ============================================
// XOR String Encryption
// ============================================
constexpr char XOR_KEY = 0x5A;

inline std::string XorDecrypt(const std::vector<char>& encrypted) {
    std::string result;
    result.reserve(encrypted.size());
    for (char c : encrypted) {
        result += (c ^ XOR_KEY);
    }
    return result;
}

// Encrypted strings (XOR with 0x5A)
// Use this to hide sensitive strings from static analysis
#define ENCRYPT_STR(str) []() -> std::string { \
    constexpr char key = 0x5A; \
    std::string s = str; \
    for (char& c : s) c ^= key; \
    return s; \
}()

#define DECRYPT_STR(str) []() -> std::string { \
    std::string s = str; \
    for (char& c : s) c ^= 0x5A; \
    return s; \
}()

// ============================================
// Anti-Debugging
// ============================================
inline bool IsDebuggerAttached() {
    // Method 1: IsDebuggerPresent API
    if (IsDebuggerPresent()) {
        return true;
    }
    
    // Method 2: CheckRemoteDebuggerPresent
    BOOL remoteDebugger = FALSE;
    if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &remoteDebugger)) {
        if (remoteDebugger) return true;
    }
    
    // Method 3: NtGlobalFlag check (PEB)
    // Check via NtQueryInformationProcess would be more reliable
    // but this is a simple check
    #ifdef _WIN64
    DWORD64 peb = __readgsqword(0x60);
    DWORD ntGlobalFlag = *(DWORD*)(peb + 0xBC);
    if (ntGlobalFlag & 0x70) return true;
    #else
    DWORD peb = __readfsdword(0x30);
    DWORD ntGlobalFlag = *(DWORD*)(peb + 0x68);
    if (ntGlobalFlag & 0x70) return true;
    #endif
    
    // Method 4: Timing check
    LARGE_INTEGER start, end, freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    
    // Simple operation that should be fast
    volatile int x = 0;
    for (int i = 0; i < 1000; i++) x += i;
    
    QueryPerformanceCounter(&end);
    double elapsed = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart * 1000.0;
    
    // If too slow, likely being debugged (step-through)
    if (elapsed > 50.0) return true;
    
    return false;
}

inline bool HasDebuggerWindows() {
    // Check for common debugger windows
    const char* debuggerWindows[] = {
        "OLLYDBG", "x64dbg", "x32dbg", "IDA", "Immunity Debugger",
        "WinDbgFrameClass", "Scylla", "Cheat Engine"
    };
    
    for (const char* wnd : debuggerWindows) {
        if (FindWindowA(wnd, NULL) != NULL || FindWindowA(NULL, wnd) != NULL) {
            return true;
        }
    }
    return false;
}

// ============================================
// Anti-VM Detection
// ============================================
inline bool IsRunningInVM() {
    // Method 1: Check CPUID hypervisor bit
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 1);
    if ((cpuInfo[2] >> 31) & 1) {
        // Hypervisor present, check vendor
        __cpuid(cpuInfo, 0x40000000);
        char vendor[13] = {0};
        memcpy(vendor, &cpuInfo[1], 4);
        memcpy(vendor + 4, &cpuInfo[2], 4);
        memcpy(vendor + 8, &cpuInfo[3], 4);
        
        // Known VM vendors
        if (strstr(vendor, "VMwareVMware") ||
            strstr(vendor, "VBoxVBoxVBox") ||
            strstr(vendor, "Microsoft Hv") ||
            strstr(vendor, "KVMKVMKVM")) {
            return true;
        }
    }
    
    // Method 2: Check for VM-specific registry keys
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, 
        "SOFTWARE\\VMware, Inc.\\VMware Tools", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Oracle\\VirtualBox Guest Additions", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }
    
    // Method 3: Check for VM-specific files
    if (GetFileAttributesA("C:\\Windows\\System32\\drivers\\vmhgfs.sys") != INVALID_FILE_ATTRIBUTES ||
        GetFileAttributesA("C:\\Windows\\System32\\drivers\\VBoxMouse.sys") != INVALID_FILE_ATTRIBUTES) {
        return true;
    }
    
    // Method 4: Check MAC address prefix (common VM MACs)
    // Skipped for simplicity
    
    return false;
}

// ============================================
// Integrity Check
// ============================================
inline DWORD CalculateChecksum(const void* data, size_t size) {
    DWORD checksum = 0;
    const BYTE* bytes = (const BYTE*)data;
    for (size_t i = 0; i < size; i++) {
        checksum = ((checksum << 5) + checksum) + bytes[i];
    }
    return checksum;
}

inline bool VerifyModuleIntegrity() {
    HMODULE hModule = GetModuleHandleA(NULL);
    if (!hModule) return false;
    
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hModule;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return false;
    
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hModule + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return false;
    
    // Check for common patching (entry point modification)
    BYTE* entryPoint = (BYTE*)hModule + ntHeaders->OptionalHeader.AddressOfEntryPoint;
    
    // Check if entry point has been hooked (JMP or INT3)
    if (entryPoint[0] == 0xE9 || entryPoint[0] == 0xCC) {
        return false;
    }
    
    return true;
}

// ============================================
// Anti-Tamper
// ============================================
inline bool CheckForHooks() {
    // Check critical API functions for hooks
    BYTE* pIsDebuggerPresent = (BYTE*)GetProcAddress(
        GetModuleHandleA("kernel32.dll"), "IsDebuggerPresent");
    
    if (pIsDebuggerPresent && pIsDebuggerPresent[0] == 0xE9) {
        return true; // JMP hook detected
    }
    
    BYTE* pVirtualProtect = (BYTE*)GetProcAddress(
        GetModuleHandleA("kernel32.dll"), "VirtualProtect");
    
    if (pVirtualProtect && pVirtualProtect[0] == 0xE9) {
        return true;
    }
    
    return false;
}

// ============================================
// Main Protection Check
// ============================================
inline int RunProtectionChecks() {
    // Returns:
    // 0 = OK
    // 1 = Debugger detected
    // 2 = VM detected
    // 3 = Integrity failed
    // 4 = Hooks detected
    
    if (IsDebuggerAttached() || HasDebuggerWindows()) {
        return 1;
    }
    
    if (IsRunningInVM()) {
        return 2;
    }
    
    if (!VerifyModuleIntegrity()) {
        return 3;
    }
    
    if (CheckForHooks()) {
        return 4;
    }
    
    return 0;
}

// ============================================
// Secure Exit
// ============================================
inline void SecureExit(int code) {
    // Clear sensitive memory before exit
    SecureZeroMemory(GetCommandLineA(), strlen(GetCommandLineA()));
    
    // Random delay to confuse analysis
    Sleep(rand() % 100);
    
    // Exit
    ExitProcess(code);
}

} // namespace Protection

