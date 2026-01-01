/*
 * Kernel Shellcode Generator & Utilities
 * 
 * Features:
 * - Virtual address translation (CR3 walking)
 * - Process/Thread enumeration
 * - EPROCESS/ETHREAD utilities
 * - Shellcode generation for kernel functions
 */

#pragma once

#include <cstdint>
#include <vector>
#include <string>

// Forward declarations
namespace mapper {
namespace intel { class IntelDriver; }
}

namespace mapper {
namespace shellcode {

// ============================================
// Windows Kernel Offsets
// Update these for different Windows versions
// Current: Windows 10/11 22H2+
// ============================================

namespace offsets {
    // EPROCESS offsets
    constexpr uint64_t EPROCESS_DirectoryTableBase = 0x28;
    constexpr uint64_t EPROCESS_UniqueProcessId = 0x440;
    constexpr uint64_t EPROCESS_ActiveProcessLinks = 0x448;
    constexpr uint64_t EPROCESS_ImageFileName = 0x5A8;
    constexpr uint64_t EPROCESS_ThreadListHead = 0x5E0;
    constexpr uint64_t EPROCESS_Peb = 0x550;
    
    // ETHREAD offsets
    constexpr uint64_t ETHREAD_ThreadListEntry = 0x4E8;
    constexpr uint64_t ETHREAD_Cid = 0x478;
    constexpr uint64_t ETHREAD_StartAddress = 0x450;
    constexpr uint64_t ETHREAD_Win32StartAddress = 0x7C0;
    constexpr uint64_t ETHREAD_Tcb = 0x0;
    
    // KTHREAD offsets (Tcb)
    constexpr uint64_t KTHREAD_KernelStack = 0x58;
    constexpr uint64_t KTHREAD_State = 0x184;
    constexpr uint64_t KTHREAD_ApcState = 0x98;
    constexpr uint64_t KTHREAD_ApcQueueable = 0x1C4;
}

// ============================================
// Virtual Address Translation
// ============================================

namespace vat {

// Initialize the virtual address translator
bool Initialize(intel::IntelDriver& driver);

// Translate virtual address to physical using specified CR3
uint64_t VirtualToPhysical(uint64_t virtualAddr, uint64_t directoryBase);

// Get the kernel CR3 (from System process)
uint64_t GetKernelCR3();

// Get process's directory base (CR3)
uint64_t GetDirectoryBase(uint64_t eprocessPhys);

} // namespace vat

// ============================================
// Process Utilities
// ============================================

namespace process {

// Find System EPROCESS (PID 4) - returns physical address
uint64_t FindSystemProcess(intel::IntelDriver& driver);

// Find EPROCESS by PID - returns physical address
uint64_t FindEprocessByPid(intel::IntelDriver& driver, uint32_t targetPid);

// Find EPROCESS by name - returns physical address
uint64_t FindEprocessByName(intel::IntelDriver& driver, const char* processName);

// Get System process using kernel base
uint64_t GetSystemProcess(intel::IntelDriver& driver, uint64_t kernelBase);

// Enumerate all processes - returns vector of physical addresses
std::vector<uint64_t> EnumerateProcesses(intel::IntelDriver& driver);

// Process info structure
struct ProcessInfo {
    uint64_t eprocessPhys;
    uint64_t directoryBase;
    uint32_t pid;
    char imageName[16];
};

// Get full process info
bool GetProcessInfo(intel::IntelDriver& driver, uint64_t eprocessPhys, ProcessInfo& info);

} // namespace process

// ============================================
// Thread Utilities
// ============================================

namespace thread {

// Find a system thread (alertable, good for APC/work item)
uint64_t FindSystemThread(intel::IntelDriver& driver, uint64_t eprocessPhys = 0);

// Queue kernel APC to thread
bool QueueKernelApc(
    intel::IntelDriver& driver,
    uint64_t ethreadPhys,
    uint64_t apcRoutine,
    uint64_t apcContext
);

// Get thread's kernel stack
uint64_t GetThreadKernelStack(intel::IntelDriver& driver, uint64_t ethreadPhys);

// Check if thread is alertable
bool IsThreadAlertable(intel::IntelDriver& driver, uint64_t ethreadPhys);

// Thread info structure
struct ThreadInfo {
    uint64_t ethreadPhys;
    uint64_t startAddress;
    uint64_t kernelStack;
    uint32_t threadId;
    uint8_t state;
    bool isAlertable;
};

// Get thread info
bool GetThreadInfo(intel::IntelDriver& driver, uint64_t ethreadPhys, ThreadInfo& info);

} // namespace thread

// ============================================
// Shellcode Generators
// ============================================

namespace gen {

// Pool allocation shellcode
std::vector<uint8_t> ExAllocatePoolWithTag(
    uint64_t funcAddr,
    uint32_t poolType,
    uint64_t size,
    uint32_t tag,
    uint64_t resultAddr,
    uint64_t statusAddr
);

// Pool free shellcode
std::vector<uint8_t> ExFreePoolWithTag(
    uint64_t funcAddr,
    uint64_t address,
    uint32_t tag,
    uint64_t statusAddr
);

// Memory copy shellcode
std::vector<uint8_t> MmCopyVirtualMemory(
    uint64_t funcAddr,
    uint64_t srcProcess,
    uint64_t srcAddr,
    uint64_t dstProcess,
    uint64_t dstAddr,
    uint64_t size,
    uint64_t resultAddr,
    uint64_t statusAddr
);

// Generic function call shellcode
std::vector<uint8_t> CallFunction(
    uint64_t funcAddr,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t resultAddr,
    uint64_t statusAddr
);

// Driver entry shellcode
std::vector<uint8_t> DriverEntry(
    uint64_t entryPoint,
    uint64_t driverObject,
    uint64_t registryPath,
    uint64_t resultAddr,
    uint64_t statusAddr
);

} // namespace gen

// ============================================
// Shellcode Executor
// ============================================

class ShellcodeExecutor {
public:
    enum class Method {
        DirectCall,      // Direct execution (not via R/W driver)
        SystemThread,    // Hijack system thread
        ApcInjection,    // Queue APC to kernel thread
        WorkItem         // Use IoQueueWorkItem (safest)
    };
    
    struct ExecuteParams {
        Method method;
        uint64_t shellcodeAddr;
        size_t shellcodeSize;
        uint32_t timeout_ms;
    };
    
    struct ExecuteResult {
        bool success;
        uint64_t returnValue;
        uint32_t errorCode;
        std::string errorMessage;
    };
    
    // Execute shellcode using specified method
    static ExecuteResult Execute(intel::IntelDriver& driver, const ExecuteParams& params);
    
    // Find a suitable system thread for execution
    static uint64_t FindSystemThread(intel::IntelDriver& driver);
    
    // Get thread's kernel stack
    static uint64_t GetThreadKernelStack(intel::IntelDriver& driver, uint64_t ethreadPhys);
};

} // namespace shellcode
} // namespace mapper
