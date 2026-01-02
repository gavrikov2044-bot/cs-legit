/*
 * MANUAL MAP INJECTOR
 * Stealth injection technique for CS2
 */

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>

// Definitions for manual mapping
using f_LoadLibraryA = HINSTANCE(WINAPI*)(const char* lpLibFileName);
using f_GetProcAddress = UINT_PTR(WINAPI*)(HINSTANCE hModule, const char* lpProcName);
using f_DLL_ENTRY_POINT = BOOL(WINAPI*)(void* hDll, DWORD dwReason, void* pReserved);

#ifdef _WIN64
using f_RtlAddFunctionTable = BOOL(WINAPIV*)(PRUNTIME_FUNCTION FunctionTable, DWORD EntryCount, DWORD64 BaseAddress);
#endif

// Missing Macros
#ifndef IMAGE_REL_BASED_DIR64
#define IMAGE_REL_BASED_DIR64 10
#endif

struct MANUAL_MAPPING_DATA {
    f_LoadLibraryA pLoadLibraryA;
    f_GetProcAddress pGetProcAddress;
#ifdef _WIN64
    f_RtlAddFunctionTable pRtlAddFunctionTable;
#endif
    BYTE* pbase;
    HINSTANCE hMod;
    DWORD fdwReasonParam;
    LPVOID reservedParam;
    BOOL SEHExceptionSupport;
};

// Shellcode to run in target process
void __stdcall Shellcode(MANUAL_MAPPING_DATA* pData) {
    if (!pData) return;

    BYTE* pBase = pData->pbase;
    auto* pOpt = &reinterpret_cast<IMAGE_NT_HEADERS*>(pBase + reinterpret_cast<IMAGE_DOS_HEADER*>(pBase)->e_lfanew)->OptionalHeader;

    // 1. Relocations
    auto* pOptHeader = &reinterpret_cast<IMAGE_NT_HEADERS*>(pBase + reinterpret_cast<IMAGE_DOS_HEADER*>(pBase)->e_lfanew)->OptionalHeader;
    BYTE* LocationDelta = pBase - pOptHeader->ImageBase;
    
    if (LocationDelta) {
        if (pOptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size) {
            auto* pRelocData = reinterpret_cast<IMAGE_BASE_RELOCATION*>(pBase + pOptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
            while (pRelocData->VirtualAddress) {
                UINT AmountOfEntries = (pRelocData->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                WORD* pRelativeInfo = reinterpret_cast<WORD*>(pRelocData + 1);

                for (UINT i = 0; i != AmountOfEntries; ++i, ++pRelativeInfo) {
                    // Fix: Check high 4 bits for type
                    if ((*pRelativeInfo >> 12) == IMAGE_REL_BASED_DIR64) {
                        UINT_PTR* pPatch = reinterpret_cast<UINT_PTR*>(pBase + pRelocData->VirtualAddress + ((*pRelativeInfo) & 0xFFF));
                        *pPatch += reinterpret_cast<UINT_PTR>(LocationDelta);
                    }
                }
                pRelocData = reinterpret_cast<IMAGE_BASE_RELOCATION*>(reinterpret_cast<BYTE*>(pRelocData) + pRelocData->SizeOfBlock);
            }
        }
    }

    // 2. Imports
    if (pOptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size) {
        auto* pImportDescr = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(pBase + pOptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        while (pImportDescr->Name) {
            char* szMod = reinterpret_cast<char*>(pBase + pImportDescr->Name);
            HINSTANCE hDll = pData->pLoadLibraryA(szMod);

            ULONG_PTR* pThunkRef = reinterpret_cast<ULONG_PTR*>(pBase + pImportDescr->OriginalFirstThunk);
            ULONG_PTR* pFuncRef = reinterpret_cast<ULONG_PTR*>(pBase + pImportDescr->FirstThunk);

            if (!pThunkRef) pThunkRef = pFuncRef;

            for (; *pThunkRef; ++pThunkRef, ++pFuncRef) {
                if (IMAGE_SNAP_BY_ORDINAL(*pThunkRef)) {
                    *pFuncRef = pData->pGetProcAddress(hDll, reinterpret_cast<char*>(*pThunkRef & 0xFFFF));
                } else {
                    auto* pImport = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(pBase + (*pThunkRef));
                    *pFuncRef = pData->pGetProcAddress(hDll, pImport->Name);
                }
            }
            ++pImportDescr;
        }
    }

    // 3. Exception Support (SEH)
    if (pData->SEHExceptionSupport) {
        auto* pImgDir = &pOptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
        if (pImgDir->Size) {
            pData->pRtlAddFunctionTable(
                reinterpret_cast<IMAGE_RUNTIME_FUNCTION_ENTRY*>(pBase + pImgDir->VirtualAddress),
                pImgDir->Size / sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY),
                reinterpret_cast<DWORD64>(pBase)
            );
        }
    }

    // 4. Call Entry Point (DllMain)
    if (pOptHeader->AddressOfEntryPoint) {
        auto EntryPoint = reinterpret_cast<f_DLL_ENTRY_POINT>(pBase + pOptHeader->AddressOfEntryPoint);
        EntryPoint(reinterpret_cast<void*>(pBase), DLL_PROCESS_ATTACH, pData->reservedParam);
    }
}

// Stub to mark end of shellcode
void __stdcall ShellcodeEnd() {}


// ==========================================
// Host Process Logic
// ==========================================

DWORD GetProcessId(const wchar_t* processName) {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snap, &entry)) {
            do {
                if (_wcsicmp(entry.szExeFile, processName) == 0) {
                    pid = entry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snap, &entry));
        }
        CloseHandle(snap);
    }
    return pid;
}

bool ManualMap(HANDLE hProc, const std::string& dllPath) {
    BYTE* pSrcData = nullptr;
    IMAGE_NT_HEADERS* pOldNtHeader = nullptr;
    IMAGE_OPTIONAL_HEADER* pOldOptHeader = nullptr;
    IMAGE_FILE_HEADER* pOldFileHeader = nullptr;
    BYTE* pTargetBase = nullptr;

    // 1. Read DLL File
    std::ifstream file(dllPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cout << "[!] Failed to open DLL file" << std::endl;
        return false;
    }
    std::streamsize fileSize = file.tellg();
    pSrcData = new BYTE[(UINT_PTR)fileSize];
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(pSrcData), fileSize);
    file.close();

    // 2. Parse Headers
    auto* pDosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(pSrcData);
    if (pDosHeader->e_magic != 0x5A4D) { 
        std::cout << "[!] Invalid file type" << std::endl;
        delete[] pSrcData; 
        return false; 
    }

    pOldNtHeader = reinterpret_cast<IMAGE_NT_HEADERS*>(pSrcData + pDosHeader->e_lfanew);
    pOldOptHeader = &pOldNtHeader->OptionalHeader;
    pOldFileHeader = &pOldNtHeader->FileHeader;

#ifdef _WIN64
    if (pOldFileHeader->Machine != IMAGE_FILE_MACHINE_AMD64) {
        std::cout << "[!] Invalid platform (need x64)" << std::endl;
        delete[] pSrcData;
        return false;
    }
#endif

    // 3. Allocate Memory in Target
    pTargetBase = reinterpret_cast<BYTE*>(VirtualAllocEx(hProc, nullptr, pOldOptHeader->SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!pTargetBase) {
        std::cout << "[!] Failed to allocate memory in target" << std::endl;
        delete[] pSrcData;
        return false;
    }
    std::cout << "[+] Allocated image at: " << (void*)pTargetBase << std::endl;

    // 4. Map Sections
    // Copy headers first
    if (!WriteProcessMemory(hProc, pTargetBase, pSrcData, 0x1000, nullptr)) {
        std::cout << "[!] Failed to write headers" << std::endl;
        VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
        delete[] pSrcData;
        return false;
    }

    auto* pSectionHeader = IMAGE_FIRST_SECTION(pOldNtHeader);
    for (UINT i = 0; i != pOldFileHeader->NumberOfSections; ++i, ++pSectionHeader) {
        if (pSectionHeader->SizeOfRawData) {
            if (!WriteProcessMemory(hProc, pTargetBase + pSectionHeader->VirtualAddress, pSrcData + pSectionHeader->PointerToRawData, pSectionHeader->SizeOfRawData, nullptr)) {
                std::cout << "[!] Failed to map section: " << i << std::endl;
                VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
                delete[] pSrcData;
                return false;
            }
        }
    }

    // 5. Prepare Mapping Data for Shellcode
    MANUAL_MAPPING_DATA data{};
    data.pLoadLibraryA = LoadLibraryA;
    data.pGetProcAddress = reinterpret_cast<f_GetProcAddress>(GetProcAddress);
#ifdef _WIN64
    data.pRtlAddFunctionTable = (f_RtlAddFunctionTable)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlAddFunctionTable");
#else 
    data.pRtlAddFunctionTable = nullptr; 
#endif
    data.pbase = pTargetBase;
    data.fdwReasonParam = DLL_PROCESS_ATTACH;
    data.reservedParam = nullptr;
    data.SEHExceptionSupport = true;

    // 6. Allocate Shellcode memory
    BYTE* pShellcode = reinterpret_cast<BYTE*>(VirtualAllocEx(hProc, nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!pShellcode) {
        std::cout << "[!] Failed to allocate shellcode memory" << std::endl;
        VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
        delete[] pSrcData;
        return false;
    }

    // Write Data structure
    if (!WriteProcessMemory(hProc, pShellcode, &data, sizeof(MANUAL_MAPPING_DATA), nullptr)) {
        std::cout << "[!] Failed to write mapping data" << std::endl;
        VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
        VirtualFreeEx(hProc, pShellcode, 0, MEM_RELEASE);
        delete[] pSrcData;
        return false;
    }

    // Write Shellcode code (after data)
    void* pShellcodeFunc = pShellcode + sizeof(MANUAL_MAPPING_DATA);
    size_t shellcodeSize = (uintptr_t)ShellcodeEnd - (uintptr_t)Shellcode;
    
    if (!WriteProcessMemory(hProc, pShellcodeFunc, (void*)Shellcode, shellcodeSize, nullptr)) {
        std::cout << "[!] Failed to write shellcode" << std::endl;
        VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
        VirtualFreeEx(hProc, pShellcode, 0, MEM_RELEASE);
        delete[] pSrcData;
        return false;
    }

    // 7. Execute Shellcode
    HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0, (LPTHREAD_START_ROUTINE)pShellcodeFunc, pShellcode, 0, nullptr);
    if (!hThread) {
        std::cout << "[!] Failed to create remote thread" << std::endl;
        VirtualFreeEx(hProc, pTargetBase, 0, MEM_RELEASE);
        VirtualFreeEx(hProc, pShellcode, 0, MEM_RELEASE);
        delete[] pSrcData;
        return false;
    }

    std::cout << "[+] Shellcode executing..." << std::endl;
    WaitForSingleObject(hThread, INFINITE);

    // 8. Cleanup (Wipe headers to hide trace)
    BYTE* emptyBuffer = new BYTE[0x1000];
    memset(emptyBuffer, 0, 0x1000);
    WriteProcessMemory(hProc, pTargetBase, emptyBuffer, 0x1000, nullptr);
    delete[] emptyBuffer;

    std::cout << "[+] Headers wiped for stealth." << std::endl;

    CloseHandle(hThread);
    VirtualFreeEx(hProc, pShellcode, 0, MEM_RELEASE);
    delete[] pSrcData;

    return true;
}

int main(int argc, char* argv[]) {
    std::cout << R"(
  __  __ _____ _____ ___   ___ ___ 
 |  \/  |  _  |_   _|   \ | __| _ \
 | |\/| | |_| | | | | |) || _||   /
 |_|  |_|_| |_| |_| |___/ |___|_|_\
   Manual Map Injector - Stealth
)" << std::endl;
    
    // Get DLL path
    std::string dllPath;
    if (argc > 1) {
        dllPath = argv[1];
    } else {
        std::filesystem::path exePath = std::filesystem::current_path();
        std::filesystem::path dllFile = exePath / "interna.dll";
        if (std::filesystem::exists(dllFile)) {
            dllPath = dllFile.string();
        } else {
            std::cout << "[?] Enter DLL path: ";
            std::getline(std::cin, dllPath);
        }
    }

    if (!std::filesystem::exists(dllPath)) {
        std::cout << "[!] DLL not found: " << dllPath << std::endl;
        std::cin.get();
        return 1;
    }

    std::cout << "[*] Waiting for CS2..." << std::endl;
    DWORD pid = 0;
    while (!(pid = GetProcessId(L"cs2.exe"))) {
        Sleep(1000);
        std::cout << ".";
    }
    std::cout << "\n[+] Found CS2 PID: " << pid << std::endl;

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        std::cout << "[!] Failed to open process" << std::endl;
        return 1;
    }

    std::cout << "[*] Injecting via Manual Map..." << std::endl;
    if (ManualMap(hProc, dllPath)) {
        std::cout << "[+] Injection Successful!" << std::endl;
        std::cout << "[+] DLL is mapped and headers are wiped." << std::endl;
    } else {
        std::cout << "[!] Injection Failed." << std::endl;
    }

    CloseHandle(hProc);
    std::cout << "\nPress Enter to exit...";
    std::cin.get();
    return 0;
}
