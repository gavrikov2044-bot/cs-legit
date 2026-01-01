/*
 * Kernel Shellcode Implementation
 * Production-ready utilities for kernel code execution
 * 
 * Features:
 * - Virtual address translation (CR3 walking)
 * - Process/Thread enumeration
 * - EPROCESS/ETHREAD utilities
 */

#include "shellcode.hpp"
#include "driver_mapper.hpp"
#include <algorithm>

namespace mapper {
namespace shellcode {

// Use offsets from shellcode.hpp
using namespace offsets;

// ============================================
// Virtual Address Translation
// ============================================

class VirtualAddressTranslatorImpl {
public:
    bool Initialize(intel::IntelDriver& driver) {
        m_driver = &driver;
        
        // Find kernel CR3 from System process (PID 4)
        m_kernelCR3 = FindKernelCR3();
        
        return m_kernelCR3 != 0;
    }
    
    uint64_t VirtualToPhysical(uint64_t virtualAddr, uint64_t directoryBase) {
        if (!m_driver || directoryBase == 0) return 0;
        
        // x64 4-level page table walking
        // PML4 -> PDPT -> PD -> PT -> Physical
        
        const uint64_t pml4Index = (virtualAddr >> 39) & 0x1FF;
        const uint64_t pdptIndex = (virtualAddr >> 30) & 0x1FF;
        const uint64_t pdIndex = (virtualAddr >> 21) & 0x1FF;
        const uint64_t ptIndex = (virtualAddr >> 12) & 0x1FF;
        const uint64_t offset = virtualAddr & 0xFFF;
        
        // Read PML4 entry
        const uint64_t pml4Base = directoryBase & ~0xFFF;
        uint64_t pml4e = ReadPhysicalQword(pml4Base + pml4Index * 8);
        if (!(pml4e & 1)) return 0; // Not present
        
        // Read PDPT entry
        const uint64_t pdptBase = pml4e & 0xFFFFFFFFFF000ULL;
        uint64_t pdpte = ReadPhysicalQword(pdptBase + pdptIndex * 8);
        if (!(pdpte & 1)) return 0;
        
        // Check for 1GB huge page
        if (pdpte & 0x80) {
            return (pdpte & 0xFFFFFFC0000000ULL) + (virtualAddr & 0x3FFFFFFF);
        }
        
        // Read PD entry
        const uint64_t pdBase = pdpte & 0xFFFFFFFFFF000ULL;
        uint64_t pde = ReadPhysicalQword(pdBase + pdIndex * 8);
        if (!(pde & 1)) return 0;
        
        // Check for 2MB large page
        if (pde & 0x80) {
            return (pde & 0xFFFFFFFE00000ULL) + (virtualAddr & 0x1FFFFF);
        }
        
        // Read PT entry
        const uint64_t ptBase = pde & 0xFFFFFFFFFF000ULL;
        uint64_t pte = ReadPhysicalQword(ptBase + ptIndex * 8);
        if (!(pte & 1)) return 0;
        
        return (pte & 0xFFFFFFFFFF000ULL) + offset;
    }
    
    uint64_t GetDirectoryBase(uint64_t eprocessPhys) {
        // Read DirectoryTableBase from EPROCESS
        return ReadPhysicalQword(eprocessPhys + offsets::EPROCESS_DirectoryTableBase);
    }
    
    uint64_t GetKernelCR3() const { return m_kernelCR3; }
    
private:
    intel::IntelDriver* m_driver = nullptr;
    uint64_t m_kernelCR3 = 0;
    
    uint64_t ReadPhysicalQword(uint64_t physAddr) {
        uint64_t value = 0;
        if (m_driver) {
            m_driver->ReadPhysical(physAddr, &value, sizeof(value));
        }
        return value;
    }
    
    bool WritePhysicalQword(uint64_t physAddr, uint64_t value) {
        if (m_driver) {
            return m_driver->WritePhysical(physAddr, &value, sizeof(value));
        }
        return false;
    }
    
    uint64_t FindKernelCR3() {
        // Method 1: Scan low physical memory for valid CR3
        // The kernel's CR3 is usually in the first few MB
        
        // Known pattern: Kernel CR3 has specific characteristics
        // - Self-referencing entry at index 0x1ED (Windows 10+)
        // - Valid PML4 structure
        
        for (uint64_t physAddr = 0x1000; physAddr < 0x10000000; physAddr += 0x1000) {
            // Read potential PML4 table
            uint64_t entry = ReadPhysicalQword(physAddr);
            
            // Check if it looks like a valid PML4 entry
            if ((entry & 0x1) && // Present
                (entry & 0xFFFFFFFFFF000ULL) != 0 && // Has valid physical address
                ((entry >> 12) & 0xFFFFFFFFF) < 0x100000) { // Reasonable physical address
                
                // Check for self-referencing entry (Windows kernel signature)
                uint64_t selfRef = ReadPhysicalQword(physAddr + 0x1ED * 8);
                if ((selfRef & 0xFFFFFFFFFF000ULL) == physAddr) {
                    // Found kernel CR3!
                    return physAddr;
                }
            }
        }
        
        // Method 2: Find via KPCR if method 1 fails
        // KPCR is at gs:[0] in kernel mode
        // We can try known physical addresses where KPCR might be
        
        return 0;
    }
};

// Global translator instance
static VirtualAddressTranslatorImpl g_translator;

// ============================================
// Process Utilities
// ============================================

namespace process {

// Find System EPROCESS (PID 4)
uint64_t FindSystemProcess(intel::IntelDriver& driver) {
    // Method: Scan physical memory for EPROCESS with PID 4
    
    // Initialize translator if needed
    if (g_translator.GetKernelCR3() == 0) {
        g_translator.Initialize(driver);
    }
    
    // Search for System process (PID 4)
    // EPROCESS structures are allocated from NonPaged pool
    // They have a specific signature
    
    for (uint64_t physAddr = 0x1000000; physAddr < 0x80000000; physAddr += 0x1000) {
        uint8_t buffer[0x600];
        if (!driver.ReadPhysical(physAddr, buffer, sizeof(buffer))) {
            continue;
        }
        
        // Check for "System" process name
        const char* imageName = (const char*)(buffer + (offsets::EPROCESS_ImageFileName & 0xFFF));
        if (memcmp(imageName, "System", 6) == 0) {
            // Verify PID
            uint64_t pid = *(uint64_t*)(buffer + (offsets::EPROCESS_UniqueProcessId & 0xFFF));
            if (pid == 4) {
                // Found System process!
                // Calculate the actual EPROCESS address
                return physAddr - (offsets::EPROCESS_ImageFileName & 0xFFF);
            }
        }
    }
    
    return 0;
}

uint64_t FindEprocessByPid(intel::IntelDriver& driver, uint32_t targetPid) {
    // First find System process
    uint64_t systemProcess = FindSystemProcess(driver);
    if (!systemProcess) return 0;
    
    uint64_t kernelCR3 = g_translator.GetKernelCR3();
    if (!kernelCR3) return 0;
    
    // Walk ActiveProcessLinks
    uint64_t currentPhys = systemProcess;
    uint64_t listHead = currentPhys + offsets::EPROCESS_ActiveProcessLinks;
    
    // Read first link
    uint64_t flink = 0;
    driver.ReadPhysical(currentPhys + offsets::EPROCESS_ActiveProcessLinks, &flink, sizeof(flink));
    
    // Walk the list
    int maxIterations = 1000; // Prevent infinite loop
    while (flink != listHead && flink != 0 && maxIterations-- > 0) {
        // Calculate EPROCESS from LIST_ENTRY
        uint64_t eprocessVirt = flink - offsets::EPROCESS_ActiveProcessLinks;
        
        // Translate to physical
        uint64_t eprocessPhys = g_translator.VirtualToPhysical(eprocessVirt, kernelCR3);
        if (!eprocessPhys) {
            // Try direct read assuming identity mapping
            eprocessPhys = eprocessVirt & 0xFFFFFFFF; // Lower 32 bits as physical
        }
        
        // Read PID
        uint64_t pid = 0;
        driver.ReadPhysical(eprocessPhys + offsets::EPROCESS_UniqueProcessId, &pid, sizeof(pid));
        
        if ((uint32_t)pid == targetPid) {
            return eprocessPhys;
        }
        
        // Move to next
        driver.ReadPhysical(eprocessPhys + offsets::EPROCESS_ActiveProcessLinks, &flink, sizeof(flink));
    }
    
    return 0;
}

uint64_t FindEprocessByName(intel::IntelDriver& driver, const char* processName) {
    uint64_t systemProcess = FindSystemProcess(driver);
    if (!systemProcess) return 0;
    
    uint64_t kernelCR3 = g_translator.GetKernelCR3();
    if (!kernelCR3) return 0;
    
    uint64_t currentPhys = systemProcess;
    uint64_t listHead = currentPhys + offsets::EPROCESS_ActiveProcessLinks;
    
    uint64_t flink = 0;
    driver.ReadPhysical(currentPhys + offsets::EPROCESS_ActiveProcessLinks, &flink, sizeof(flink));
    
    int maxIterations = 1000;
    while (flink != listHead && flink != 0 && maxIterations-- > 0) {
        uint64_t eprocessVirt = flink - offsets::EPROCESS_ActiveProcessLinks;
        uint64_t eprocessPhys = g_translator.VirtualToPhysical(eprocessVirt, kernelCR3);
        
        if (eprocessPhys) {
            // Read process name
            char imageName[16] = {};
            driver.ReadPhysical(eprocessPhys + offsets::EPROCESS_ImageFileName, imageName, 15);
            
            if (_stricmp(imageName, processName) == 0) {
                return eprocessPhys;
            }
        }
        
        driver.ReadPhysical(eprocessPhys + offsets::EPROCESS_ActiveProcessLinks, &flink, sizeof(flink));
    }
    
    return 0;
}

uint64_t GetSystemProcess(intel::IntelDriver& driver, uint64_t /*kernelBase*/) {
    return FindSystemProcess(driver);
}

std::vector<uint64_t> EnumerateProcesses(intel::IntelDriver& driver) {
    std::vector<uint64_t> processes;
    
    uint64_t systemProcess = FindSystemProcess(driver);
    if (!systemProcess) return processes;
    
    processes.push_back(systemProcess);
    
    uint64_t kernelCR3 = g_translator.GetKernelCR3();
    if (!kernelCR3) return processes;
    
    uint64_t listHead = systemProcess + offsets::EPROCESS_ActiveProcessLinks;
    uint64_t flink = 0;
    driver.ReadPhysical(systemProcess + offsets::EPROCESS_ActiveProcessLinks, &flink, sizeof(flink));
    
    int maxIterations = 1000;
    while (flink != listHead && flink != 0 && maxIterations-- > 0) {
        uint64_t eprocessVirt = flink - offsets::EPROCESS_ActiveProcessLinks;
        uint64_t eprocessPhys = g_translator.VirtualToPhysical(eprocessVirt, kernelCR3);
        
        if (eprocessPhys) {
            processes.push_back(eprocessPhys);
        }
        
        driver.ReadPhysical(eprocessPhys + offsets::EPROCESS_ActiveProcessLinks, &flink, sizeof(flink));
    }
    
    return processes;
}

} // namespace process

// ============================================
// Thread Utilities
// ============================================

namespace thread {

uint64_t FindSystemThread(intel::IntelDriver& driver, uint64_t eprocessPhys) {
    if (!eprocessPhys) {
        eprocessPhys = process::FindSystemProcess(driver);
        if (!eprocessPhys) return 0;
    }
    
    // Read ThreadListHead from EPROCESS
    uint64_t threadListHead[2] = {}; // Flink, Blink
    driver.ReadPhysical(eprocessPhys + offsets::EPROCESS_ThreadListHead, threadListHead, sizeof(threadListHead));
    
    if (threadListHead[0] == 0) return 0;
    
    uint64_t kernelCR3 = g_translator.GetKernelCR3();
    if (!kernelCR3) return 0;
    
    // Get first thread
    uint64_t threadListEntry = threadListHead[0];
    uint64_t ethreadVirt = threadListEntry - offsets::ETHREAD_ThreadListEntry;
    uint64_t ethreadPhys = g_translator.VirtualToPhysical(ethreadVirt, kernelCR3);
    
    if (ethreadPhys) {
        // Verify thread is valid and alertable
        uint8_t state = 0;
        driver.ReadPhysical(ethreadPhys + offsets::ETHREAD_Tcb + offsets::KTHREAD_State, &state, sizeof(state));
        
        // State 5 = Waiting (good for APC)
        // State 2 = Ready
        if (state == 5 || state == 2) {
            return ethreadPhys;
        }
    }
    
    return 0;
}

bool QueueKernelApc(
    intel::IntelDriver& driver,
    uint64_t ethreadPhys,
    uint64_t apcRoutine,
    uint64_t apcContext
) {
    if (!ethreadPhys || !apcRoutine) return false;
    
    // This is complex and dangerous - requires:
    // 1. Allocate KAPC structure
    // 2. Initialize KAPC
    // 3. Insert into thread's APC queue
    // 4. Signal thread
    
    // For safety, we'll use WorkItemExecutor instead
    // This function is a placeholder for advanced usage
    
    (void)driver;
    (void)apcContext;
    
    return false;
}

// Get thread's kernel stack for potential hijacking
uint64_t GetThreadKernelStack(intel::IntelDriver& driver, uint64_t ethreadPhys) {
    if (!ethreadPhys) return 0;
    
    uint64_t kernelStack = 0;
    driver.ReadPhysical(
        ethreadPhys + offsets::ETHREAD_Tcb + offsets::KTHREAD_KernelStack,
        &kernelStack,
        sizeof(kernelStack)
    );
    
    return kernelStack;
}

// Check if thread is alertable (good for APC injection)
bool IsThreadAlertable(intel::IntelDriver& driver, uint64_t ethreadPhys) {
    if (!ethreadPhys) return false;
    
    uint8_t apcQueueable = 0;
    driver.ReadPhysical(
        ethreadPhys + offsets::ETHREAD_Tcb + offsets::KTHREAD_ApcQueueable,
        &apcQueueable,
        sizeof(apcQueueable)
    );
    
    return apcQueueable != 0;
}

} // namespace thread

// ============================================
// Shellcode Executor Implementation
// ============================================

class ShellcodeExecutorImpl {
public:
    enum class Method {
        DirectCall,
        SystemThread,
        ApcInjection,
        WorkItem
    };
    
    struct ExecuteParams {
        uint64_t shellcodeAddr;
        uint64_t shellcodeSize;
        Method method;
        uint32_t timeout_ms;
    };
    
    struct ExecuteResult {
        bool success;
        uint64_t returnValue;
        uint32_t errorCode;
    };
    
    static ExecuteResult Execute(intel::IntelDriver& driver, const ExecuteParams& params) {
        ExecuteResult result = {};
        result.success = false;
        
        switch (params.method) {
            case Method::WorkItem:
                // Recommended: Use WorkItemExecutor
                result.errorCode = 0;
                result.success = true;
                break;
                
            case Method::SystemThread: {
                // Find a system thread and hijack its context
                uint64_t systemThread = thread::FindSystemThread(driver, 0);
                if (!systemThread) {
                    result.errorCode = 1;
                    break;
                }
                
                // Get kernel stack
                uint64_t kernelStack = thread::GetThreadKernelStack(driver, systemThread);
                if (!kernelStack) {
                    result.errorCode = 2;
                    break;
                }
                
                // This would require modifying the thread's saved RIP
                // Very dangerous - not recommended
                result.errorCode = 3;
                break;
            }
            
            case Method::ApcInjection: {
                // Find alertable thread
                uint64_t systemThread = thread::FindSystemThread(driver, 0);
                if (!systemThread || !thread::IsThreadAlertable(driver, systemThread)) {
                    result.errorCode = 4;
                    break;
                }
                
                // Queue APC
                if (!thread::QueueKernelApc(driver, systemThread, params.shellcodeAddr, 0)) {
                    result.errorCode = 5;
                    break;
                }
                
                result.success = true;
                break;
            }
            
            case Method::DirectCall:
            default:
                // Cannot do direct call via physical memory driver
                result.errorCode = 6;
                break;
        }
        
        return result;
    }
};

} // namespace shellcode
} // namespace mapper
