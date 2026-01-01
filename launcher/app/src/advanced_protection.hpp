/*
 * Advanced Protection - Timing checks, hardware breakpoint detection, parent process check
 * Extends protection.hpp with additional anti-debugging techniques
 */

#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <chrono>

namespace AdvancedProtection {
    
    // ============================================
    // Timing-Based Debugger Detection
    // ============================================
    
    // Measure execution time of a block
    template<typename Func>
    bool DetectDebuggerByTiming(Func func, int64_t expectedMicros, float toleranceFactor = 1.5f) {
        auto start = std::chrono::high_resolution_clock::now();
        
        func();
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        // If execution took significantly longer, likely being debugged
        return duration > (expectedMicros * toleranceFactor);
    }
    
    // Simple timing check (measures RDTSC instruction)
    bool IsDebuggedTiming() {
        uint64_t start, end;
        
        __asm {
            rdtsc
            mov dword ptr[start], eax
            mov dword ptr[start + 4], edx
        }
        
        // Do some work
        int dummy = 0;
        for (int i = 0; i < 100; i++) {
            dummy += i;
        }
        
        __asm {
            rdtsc
            mov dword ptr[end], eax
            mov dword ptr[end + 4], edx
        }
        
        uint64_t cycles = end - start;
        
        // Normal execution: < 10000 cycles
        // Debugger: > 50000 cycles
        return cycles > 50000;
    }
    
    // ============================================
    // Hardware Breakpoint Detection
    // ============================================
    
    bool IsHardwareBreakpointSet() {
        CONTEXT ctx = {};
        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        
        if (!GetThreadContext(GetCurrentThread(), &ctx)) {
            return false; // Can't check, assume safe
        }
        
        // Check if any hardware breakpoints are set
        return (ctx.Dr0 != 0 || ctx.Dr1 != 0 || ctx.Dr2 != 0 || ctx.Dr3 != 0 || ctx.Dr6 != 0 || ctx.Dr7 != 0);
    }
    
    // Clear hardware breakpoints (if possible)
    void ClearHardwareBreakpoints() {
        CONTEXT ctx = {};
        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        
        if (GetThreadContext(GetCurrentThread(), &ctx)) {
            ctx.Dr0 = 0;
            ctx.Dr1 = 0;
            ctx.Dr2 = 0;
            ctx.Dr3 = 0;
            ctx.Dr6 = 0;
            ctx.Dr7 = 0;
            
            SetThreadContext(GetCurrentThread(), &ctx);
        }
    }
    
    // ============================================
    // Parent Process Check
    // ============================================
    
    DWORD GetParentProcessId() {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) return 0;
        
        PROCESSENTRY32 pe32 = { sizeof(PROCESSENTRY32) };
        DWORD pid = GetCurrentProcessId();
        DWORD ppid = 0;
        
        if (Process32First(hSnapshot, &pe32)) {
            do {
                if (pe32.th32ProcessID == pid) {
                    ppid = pe32.th32ParentProcessID;
                    break;
                }
            } while (Process32Next(hSnapshot, &pe32));
        }
        
        CloseHandle(hSnapshot);
        return ppid;
    }
    
    std::string GetProcessName(DWORD pid) {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) return "";
        
        PROCESSENTRY32 pe32 = { sizeof(PROCESSENTRY32) };
        std::string name;
        
        if (Process32First(hSnapshot, &pe32)) {
            do {
                if (pe32.th32ProcessID == pid) {
                    name = pe32.szExeFile;
                    break;
                }
            } while (Process32Next(hSnapshot, &pe32));
        }
        
        CloseHandle(hSnapshot);
        
        // Convert to lowercase
        for (char& c : name) {
            c = tolower(c);
        }
        
        return name;
    }
    
    bool IsLaunchedByDebugger() {
        DWORD ppid = GetParentProcessId();
        if (ppid == 0) return false;
        
        std::string parentName = GetProcessName(ppid);
        
        // Check against known debuggers
        const char* debuggers[] = {
            "x64dbg.exe",
            "x32dbg.exe",
            "ollydbg.exe",
            "windbg.exe",
            "ida.exe",
            "ida64.exe",
            "idaq.exe",
            "idaq64.exe",
            "idaw.exe",
            "idaw64.exe",
            "cheatengine-x86_64.exe",
            "cheatengine-i386.exe",
            "processhacker.exe",
            "procexp.exe",
            "procexp64.exe"
        };
        
        for (const char* debugger : debuggers) {
            if (parentName == debugger) {
                return true;
            }
        }
        
        return false;
    }
    
    // ============================================
    // Comprehensive Check
    // ============================================
    
    struct ProtectionResult {
        bool isDebuggerAttached;
        bool isHardwareBreakpoint;
        bool isLaunchedByDebugger;
        bool isTimingAnomaly;
        
        bool IsSafe() const {
            return !isDebuggerAttached && 
                   !isHardwareBreakpoint && 
                   !isLaunchedByDebugger && 
                   !isTimingAnomaly;
        }
        
        std::string GetReason() const {
            if (isDebuggerAttached) return "Debugger attached";
            if (isHardwareBreakpoint) return "Hardware breakpoint detected";
            if (isLaunchedByDebugger) return "Launched by debugger";
            if (isTimingAnomaly) return "Timing anomaly detected";
            return "Safe";
        }
    };
    
    ProtectionResult CheckAll() {
        ProtectionResult result;
        
        result.isDebuggerAttached = IsDebuggerPresent();
        result.isHardwareBreakpoint = IsHardwareBreakpointSet();
        result.isLaunchedByDebugger = IsLaunchedByDebugger();
        result.isTimingAnomaly = IsDebuggedTiming();
        
        return result;
    }
    
    // ============================================
    // Anti-Dump Protection
    // ============================================
    
    // Erase PE header to prevent dumping
    void ErasePEHeader() {
        HMODULE hModule = GetModuleHandle(NULL);
        if (!hModule) return;
        
        PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hModule;
        PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hModule + pDosHeader->e_lfanew);
        
        if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE) return;
        
        DWORD oldProtect;
        VirtualProtect(pDosHeader, sizeof(IMAGE_DOS_HEADER), PAGE_READWRITE, &oldProtect);
        
        // Erase DOS header
        SecureZeroMemory(pDosHeader, sizeof(IMAGE_DOS_HEADER));
        
        VirtualProtect(pDosHeader, sizeof(IMAGE_DOS_HEADER), oldProtect, &oldProtect);
        
        // Erase NT headers
        VirtualProtect(pNtHeaders, sizeof(IMAGE_NT_HEADERS), PAGE_READWRITE, &oldProtect);
        SecureZeroMemory(pNtHeaders, sizeof(IMAGE_NT_HEADERS));
        VirtualProtect(pNtHeaders, sizeof(IMAGE_NT_HEADERS), oldProtect, &oldProtect);
    }
    
    // ============================================
    // Thread Hiding (from debuggers)
    // ============================================
    
    typedef NTSTATUS(WINAPI* pNtSetInformationThread)(HANDLE, ULONG, PVOID, ULONG);
    
    void HideThreadFromDebugger() {
        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (!hNtdll) return;
        
        pNtSetInformationThread NtSetInformationThread = 
            (pNtSetInformationThread)GetProcAddress(hNtdll, "NtSetInformationThread");
        
        if (!NtSetInformationThread) return;
        
        const ULONG ThreadHideFromDebugger = 0x11;
        NtSetInformationThread(GetCurrentThread(), ThreadHideFromDebugger, NULL, 0);
    }
    
    // ============================================
    // Initialization
    // ============================================
    
    void Initialize() {
        // Hide thread from debugger
        HideThreadFromDebugger();
        
        // Erase PE header (anti-dump)
        ErasePEHeader();
        
        // Clear any hardware breakpoints
        ClearHardwareBreakpoints();
    }
}

