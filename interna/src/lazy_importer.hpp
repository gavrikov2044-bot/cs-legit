#pragma once
#include <Windows.h>
#include <winternl.h>

// Simple hash for function names
constexpr unsigned int hash_str(const char* str, unsigned int seed = 0) {
    return *str ? hash_str(str + 1, (seed ^ *str) * 16777619) : seed;
}

#define LI_FN(name) LazyImporter::get<hash_str(#name)>(#name)

namespace LazyImporter {
    template<unsigned int Hash>
    __forceinline void* get_ptr(const char* name) {
        PPEB peb = (PPEB)__readgsqword(0x60);
        PPEB_LDR_DATA ldr = peb->Ldr;
        PLIST_ENTRY head = &ldr->InMemoryOrderModuleList;
        PLIST_ENTRY curr = head->Flink;

        while (curr != head) {
            PLDR_DATA_TABLE_ENTRY mod = CONTAINING_RECORD(curr, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
            
            if (mod->DllBase) {
                PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)mod->DllBase;
                PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)dos + dos->e_lfanew);
                
                if (nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress) {
                    PIMAGE_EXPORT_DIRECTORY exp = (PIMAGE_EXPORT_DIRECTORY)((BYTE*)dos + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
                    DWORD* names = (DWORD*)((BYTE*)dos + exp->AddressOfNames);
                    DWORD* funcs = (DWORD*)((BYTE*)dos + exp->AddressOfFunctions);
                    WORD* ords = (WORD*)((BYTE*)dos + exp->AddressOfNameOrdinals);

                    for (DWORD i = 0; i < exp->NumberOfNames; i++) {
                        const char* func_name = (const char*)((BYTE*)dos + names[i]);
                        // Simple runtime hash check (in real legit cheat we would use compile time only)
                        // But for stability we do string compare here if hash matches or if we pass name
                        // Optimized for this simple implementation:
                        
                        unsigned int h = 0;
                        const char* s = func_name;
                        while(*s) { h = (h ^ *s) * 16777619; s++; }
                        
                        if (h == Hash) {
                            return (void*)((BYTE*)dos + funcs[ords[i]]);
                        }
                    }
                }
            }
            curr = curr->Flink;
        }
        return nullptr;
    }

    template<unsigned int Hash>
    __forceinline auto get(const char* name) {
        using FnType = void* (*)(); 
        // We cast to correct type at call site, here we return void*
        return get_ptr<Hash>(name);
    }
}

