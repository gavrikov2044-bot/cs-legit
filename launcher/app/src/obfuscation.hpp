/*
 * String Obfuscation - Compile-time XOR encryption
 * Prevents strings from appearing in binary analysis
 */

#pragma once
#include <string>
#include <array>

namespace Obfuscation {
    
    // Compile-time random seed (from __TIME__)
    constexpr unsigned int Seed() {
        return __TIME__[7] + __TIME__[6] * 10 + __TIME__[4] * 60 + __TIME__[3] * 600 + 
               __TIME__[1] * 3600 + __TIME__[0] * 36000;
    }
    
    // Compile-time XOR key generation
    constexpr uint8_t Key(size_t index) {
        return static_cast<uint8_t>((Seed() * (index + 1)) % 256);
    }
    
    // Compile-time string encryption
    template<size_t N>
    class ObfuscatedString {
    private:
        std::array<char, N> m_data;
        
    public:
        constexpr ObfuscatedString(const char(&str)[N]) : m_data{} {
            for (size_t i = 0; i < N; ++i) {
                m_data[i] = str[i] ^ Key(i);
            }
        }
        
        // Runtime decryption
        std::string Decrypt() const {
            std::string result;
            result.reserve(N - 1); // -1 for null terminator
            
            for (size_t i = 0; i < N - 1; ++i) {
                result.push_back(m_data[i] ^ Key(i));
            }
            
            return result;
        }
        
        // Get encrypted data (for storage)
        const char* Data() const {
            return m_data.data();
        }
        
        size_t Size() const {
            return N;
        }
    };
    
    // Helper macro for easy usage
    #define OBFSTR(str) (Obfuscation::ObfuscatedString<sizeof(str)>(str).Decrypt())
    
    // Example usage:
    // std::string host = OBFSTR("138.124.0.8");
    // const char* header = OBFSTR("single-project.duckdns.org").c_str();
    
    // Wide string version
    template<size_t N>
    class ObfuscatedWString {
    private:
        std::array<wchar_t, N> m_data;
        
    public:
        constexpr ObfuscatedWString(const wchar_t(&str)[N]) : m_data{} {
            for (size_t i = 0; i < N; ++i) {
                m_data[i] = str[i] ^ Key(i);
            }
        }
        
        std::wstring Decrypt() const {
            std::wstring result;
            result.reserve(N - 1);
            
            for (size_t i = 0; i < N - 1; ++i) {
                result.push_back(m_data[i] ^ Key(i));
            }
            
            return result;
        }
    };
    
    #define OBFWSTR(str) (Obfuscation::ObfuscatedWString<sizeof(str)/sizeof(wchar_t)>(L##str).Decrypt())
    
    // API name obfuscation
    class APIObfuscator {
    public:
        // Dynamically resolve WinAPI functions
        template<typename FuncType>
        static FuncType GetFunction(const char* module, const char* funcName) {
            HMODULE hModule = GetModuleHandleA(module);
            if (!hModule) {
                hModule = LoadLibraryA(module);
            }
            
            if (!hModule) return nullptr;
            
            return reinterpret_cast<FuncType>(GetProcAddress(hModule, funcName));
        }
    };
    
    // Example usage:
    // auto fnReadProcessMemory = APIObfuscator::GetFunction<BOOL(WINAPI*)(HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T*)>(
    //     OBFSTR("kernel32.dll").c_str(),
    //     OBFSTR("ReadProcessMemory").c_str()
    // );
}

