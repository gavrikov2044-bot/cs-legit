#pragma once
#include <string>
#include <array>
#include <cstdarg>

namespace XorCompileTime {
    constexpr auto time = __TIME__;
    constexpr auto seed = static_cast<int>(time[7]) + static_cast<int>(time[6]) * 10 + static_cast<int>(time[4]) * 60 + static_cast<int>(time[3]) * 600 + static_cast<int>(time[1]) * 3600 + static_cast<int>(time[0]) * 36000;

    template <int N, const char kKey>
    class XorString {
    private:
        std::array<char, N + 1> _encrypted;

        constexpr char enc(char c) const {
            return c ^ kKey ^ (seed % 255);
        }

        constexpr char dec(char c) const {
            return c ^ kKey ^ (seed % 255);
        }

    public:
        template <size_t... Is>
        constexpr XorString(const char* str, std::index_sequence<Is...>) : _encrypted{ enc(str[Is])... } { }

        __forceinline std::string decrypt() {
            std::string s;
            s.resize(N);
            for (int i = 0; i < N; i++) {
                s[i] = dec(_encrypted[i]);
            }
            return s;
        }
        
        __forceinline const char* decrypt_c() {
            static char s[N + 1];
            for (int i = 0; i < N; i++) {
                s[i] = dec(_encrypted[i]);
            }
            s[N] = '\0';
            return s;
        }
    };
}

#define XString(str) []() { \
    constexpr auto s = str; \
    constexpr int n = sizeof(s) - 1; \
    constexpr char key = static_cast<char>(__COUNTER__ + 0x55); \
    return XorCompileTime::XorString<n, key>(s, std::make_index_sequence<n>()).decrypt_c(); \
}()

