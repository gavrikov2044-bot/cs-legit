# Anti-Detection Improvements

## Current Protection Status

### Existing Protections (launcher/src/protection.hpp)
1. ✅ VM Detection (IsRunningInVM)
2. ✅ Debugger Detection (IsDebuggerAttached)
3. ✅ Debugger Window Detection (HasDebuggerWindows)
4. ✅ Secure Exit (clears memory before exit)

---

## Planned Improvements

### Phase 1: Code Obfuscation (High Priority)

#### 1. String Encryption
**Current:** Strings are plaintext in binary
```cpp
#define SERVER_HOST "138.124.0.8"  // VISIBLE in strings.exe
```

**Improved:** XOR encryption at compile-time
```cpp
constexpr char EncryptedHost[] = {0x19, 0x1F, 0x1C, ...}; // XOR with key
std::string DecryptString(const char* encrypted, size_t len, uint8_t key) {
    std::string result(len, '\0');
    for (size_t i = 0; i < len; i++) {
        result[i] = encrypted[i] ^ key;
    }
    return result;
}
```

#### 2. API Call Obfuscation
**Current:** Direct WinAPI calls (easy to hook)
```cpp
ReadProcessMemory(hProcess, addr, buffer, size, nullptr);
```

**Improved:** Dynamic API resolution
```cpp
typedef BOOL(WINAPI* pReadProcessMemory)(HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T*);
pReadProcessMemory fnReadProcessMemory = (pReadProcessMemory)GetProcAddress(GetModuleHandle("kernel32.dll"), "ReadProcessMemory");
```

#### 3. Control Flow Obfuscation
**Current:** Linear execution
```cpp
if (condition) {
    doAction();
}
```

**Improved:** Opaque predicates + fake branches
```cpp
int opaque = (x * x + x) % 2; // Always 0 or 1
if (opaque == 0) {
    doAction(); // Real code
} else {
    fakeAction(); // Never executed, confuses disassemblers
}
```

---

### Phase 2: Memory Protection (Medium Priority)

#### 4. Code Section Encryption
- Encrypt critical code sections
- Decrypt on-the-fly during execution
- Re-encrypt after use

```cpp
void ExecuteProtectedCode() {
    DecryptCodeSection();
    // Execute critical code
    EncryptCodeSection();
}
```

#### 5. Stack Encryption
- Encrypt sensitive variables on stack
- Decrypt when needed

```cpp
EncryptedVar<std::string> token;
token = g_token; // Encrypted in memory
std::string plain = token.decrypt(); // Decrypt for use
```

#### 6. Heap Spray Prevention
- Avoid predictable memory patterns
- Randomize allocation sizes

---

### Phase 3: Advanced Evasion (Low Priority)

#### 7. Timing Checks
- Detect debugger by measuring execution time
- Debuggers slow down execution

```cpp
auto start = std::chrono::high_resolution_clock::now();
// Some code
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

if (duration > expected_time * 1.5) {
    // Likely debugged
    Protection::SecureExit(0);
}
```

#### 8. Hardware Breakpoint Detection
```cpp
CONTEXT ctx = {};
ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
GetThreadContext(GetCurrentThread(), &ctx);

if (ctx.Dr0 || ctx.Dr1 || ctx.Dr2 || ctx.Dr3) {
    // Hardware breakpoint detected
    Protection::SecureExit(0);
}
```

#### 9. Parent Process Check
```cpp
DWORD GetParentProcessId() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe32 = { sizeof(PROCESSENTRY32) };
    DWORD pid = GetCurrentProcessId();
    DWORD ppid = 0;
    
    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (pe32.th32ProcessID == pid) {
                ppid = pe32.th32ParentProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }
    
    CloseHandle(hSnapshot);
    return ppid;
}

bool IsLaunchedByDebugger() {
    DWORD ppid = GetParentProcessId();
    // Check if parent is known debugger (x64dbg, ollydbg, etc.)
    return CheckProcessName(ppid, "x64dbg.exe") || 
           CheckProcessName(ppid, "ollydbg.exe");
}
```

---

### Phase 4: External Cheat Specific (Critical)

#### 10. Read/Write Obfuscation
**Current:** Direct RPM/WPM calls
```cpp
ReadProcessMemory(hProcess, addr, &value, sizeof(value), nullptr);
```

**Improved:** Multiple layers of indirection
```cpp
template<typename T>
T ReadMemory(uintptr_t addr) {
    // Layer 1: Split into multiple small reads
    std::vector<uint8_t> bytes(sizeof(T));
    for (size_t i = 0; i < sizeof(T); i++) {
        ReadProcessMemory(hProcess, (LPCVOID)(addr + i), &bytes[i], 1, nullptr);
        Sleep(rand() % 3); // Random delay
    }
    
    // Layer 2: XOR with random key
    uint8_t key = rand() % 256;
    for (auto& byte : bytes) {
        byte ^= key;
        byte ^= key; // XOR twice = original value
    }
    
    return *reinterpret_cast<T*>(bytes.data());
}
```

#### 11. Pattern Scanning Obfuscation
**Current:** Fixed signature scanning
```cpp
uintptr_t FindPattern(const char* pattern) {
    // Scan for exact pattern
}
```

**Improved:** Dynamic pattern generation
```cpp
std::vector<uint8_t> GeneratePattern() {
    // Generate pattern at runtime based on hash
    // Different pattern each session
}
```

#### 12. Handle Obfuscation
**Current:** Single process handle
```cpp
HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
```

**Improved:** Multiple handles, rotate usage
```cpp
std::vector<HANDLE> handles;
for (int i = 0; i < 5; i++) {
    handles.push_back(OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, pid));
}

// Use random handle for each operation
HANDLE GetRandomHandle() {
    return handles[rand() % handles.size()];
}
```

---

### Phase 5: Network Traffic Obfuscation (Medium Priority)

#### 13. Encrypt API Requests
**Current:** Plaintext HTTP
```cpp
POST /api/auth/login HTTP/1.1
{"username": "test", "password": "123"}
```

**Improved:** TLS + payload encryption
```cpp
POST /api/auth/login HTTP/1.1
{"data": "AES_ENCRYPTED_BASE64_PAYLOAD"}
```

#### 14. Traffic Padding
- Add random data to requests
- Makes traffic analysis harder

#### 15. Domain Fronting
- Use CDN (Cloudflare) to hide real server
- Harder to block

---

## Implementation Priority

| Feature | Priority | Difficulty | Impact | ETA |
|---------|----------|------------|--------|-----|
| String Encryption | 🔴 High | 🟢 Easy | 🟢 High | 1 day |
| API Obfuscation | 🔴 High | 🟡 Medium | 🟢 High | 2 days |
| Control Flow Obf. | 🟡 Medium | 🔴 Hard | 🟡 Medium | 1 week |
| Code Encryption | 🟡 Medium | 🔴 Hard | 🟢 High | 1 week |
| Timing Checks | 🟢 Low | 🟢 Easy | 🟡 Medium | 1 day |
| HW Breakpoint Det. | 🟢 Low | 🟢 Easy | 🟡 Medium | 1 day |
| Parent Process Check | 🟢 Low | 🟢 Easy | 🟡 Medium | 1 day |
| R/W Obfuscation | 🔴 High | 🟡 Medium | 🟢 High | 3 days |
| Pattern Obf. | 🟡 Medium | 🟡 Medium | 🟡 Medium | 2 days |
| Handle Rotation | 🟡 Medium | 🟢 Easy | 🟡 Medium | 1 day |
| Request Encryption | 🟡 Medium | 🟡 Medium | 🟢 High | 3 days |
| Traffic Padding | 🟢 Low | 🟢 Easy | 🟡 Medium | 1 day |
| Domain Fronting | 🟢 Low | 🟡 Medium | 🟡 Medium | 2 days |

**Total Estimated Time:** 3-4 weeks for all features

---

## Recommended Implementation Order

### Week 1: Quick Wins
1. ✅ String Encryption (obfuscate sensitive strings)
2. ✅ Timing Checks (detect debuggers)
3. ✅ Hardware Breakpoint Detection
4. ✅ Parent Process Check

### Week 2: API Security
5. ✅ API Call Obfuscation (dynamic resolution)
6. ✅ Request Encryption (TLS + AES)
7. ✅ Handle Rotation

### Week 3: Memory Protection
8. ✅ R/W Obfuscation (multi-layer reads)
9. ✅ Pattern Obfuscation (dynamic patterns)
10. ✅ Traffic Padding

### Week 4: Advanced
11. ✅ Control Flow Obfuscation (opaque predicates)
12. ✅ Code Section Encryption
13. ✅ Domain Fronting (Cloudflare)

---

## Tools & Libraries

### Obfuscation
- **Themida** ($$$) - Commercial protector
- **VMProtect** ($$$) - VM-based protection
- **OLLVM** (Free) - LLVM-based obfuscator
- **ADVobfuscator** (Free) - Compile-time obfuscation

### Encryption
- **Crypto++** (Free) - C++ crypto library
- **OpenSSL** (Free) - TLS + AES
- **mbedTLS** (Free) - Lightweight TLS

### Analysis Prevention
- **ScyllaHide** - Anti-anti-debug plugin (for testing)
- **x64dbg** - Debugger (for testing own protections)

---

## Testing Plan

### Manual Testing
1. ✅ Run in debugger (should detect & exit)
2. ✅ Run in VM (should detect & exit or degrade gracefully)
3. ✅ Analyze with IDA Pro (check string visibility)
4. ✅ Check with Process Hacker (handles, memory)
5. ✅ Monitor with Wireshark (network traffic)

### Automated Testing
1. ✅ Unit tests for each protection
2. ✅ Integration tests (full launch flow)
3. ✅ Performance tests (overhead < 5%)

---

## Notes

- **Don't overdo it:** Too much protection = performance impact
- **Balance:** Security vs. Usability
- **Update regularly:** Anti-cheats evolve, so must we
- **Community:** Learn from others (UC Forum, GH)

---

**Status:** Ready for implementation  
**Last Updated:** December 25, 2025  
**Next Review:** January 2026

