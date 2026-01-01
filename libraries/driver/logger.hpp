/*
 * Kernel Mapper Logger
 * Thread-safe logging with multiple outputs
 */

#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include <mutex>
#include <fstream>
#include <chrono>
#include <cstdarg>
#include <sstream>
#include <iomanip>

namespace mapper {
namespace log {

// ============================================
// Log Levels
// ============================================

enum class Level {
    Trace = 0,      // Detailed tracing
    Debug = 1,      // Debug information
    Info = 2,       // General information
    Warning = 3,    // Warning conditions
    Error = 4,      // Error conditions
    Critical = 5,   // Critical errors
    None = 100      // Disable logging
};

// ============================================
// Log Output Flags
// ============================================

enum class Output : uint32_t {
    None        = 0,
    Console     = 1 << 0,   // stdout/stderr
    DebugView   = 1 << 1,   // OutputDebugString
    File        = 1 << 2,   // Log file
    Callback    = 1 << 3,   // Custom callback
    All         = 0xFFFFFFFF
};

inline Output operator|(Output a, Output b) {
    return static_cast<Output>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline Output operator&(Output a, Output b) {
    return static_cast<Output>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

// ============================================
// Log Callback Type
// ============================================

using LogCallback = void(*)(Level level, const char* module, const char* message);

// ============================================
// Logger Class
// ============================================

class Logger {
public:
    // Singleton instance
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }
    
    // Configuration
    void SetLevel(Level level) { m_level = level; }
    Level GetLevel() const { return m_level; }
    
    void SetOutput(Output output) { m_output = output; }
    void EnableOutput(Output output) { m_output = m_output | output; }
    void DisableOutput(Output output) { 
        m_output = static_cast<Output>(static_cast<uint32_t>(m_output) & ~static_cast<uint32_t>(output)); 
    }
    
    void SetLogFile(const std::wstring& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_fileStream.is_open()) {
            m_fileStream.close();
        }
        m_fileStream.open(path, std::ios::out | std::ios::app);
    }
    
    void SetCallback(LogCallback callback) { m_callback = callback; }
    
    void SetModulePrefix(const std::string& prefix) { m_modulePrefix = prefix; }
    
    // Logging methods
    void Log(Level level, const char* module, const char* format, ...) {
        if (level < m_level) return;
        
        va_list args;
        va_start(args, format);
        LogInternal(level, module, format, args);
        va_end(args);
    }
    
    void Trace(const char* module, const char* format, ...) {
        if (Level::Trace < m_level) return;
        va_list args;
        va_start(args, format);
        LogInternal(Level::Trace, module, format, args);
        va_end(args);
    }
    
    void Debug(const char* module, const char* format, ...) {
        if (Level::Debug < m_level) return;
        va_list args;
        va_start(args, format);
        LogInternal(Level::Debug, module, format, args);
        va_end(args);
    }
    
    void Info(const char* module, const char* format, ...) {
        if (Level::Info < m_level) return;
        va_list args;
        va_start(args, format);
        LogInternal(Level::Info, module, format, args);
        va_end(args);
    }
    
    void Warning(const char* module, const char* format, ...) {
        if (Level::Warning < m_level) return;
        va_list args;
        va_start(args, format);
        LogInternal(Level::Warning, module, format, args);
        va_end(args);
    }
    
    void Error(const char* module, const char* format, ...) {
        if (Level::Error < m_level) return;
        va_list args;
        va_start(args, format);
        LogInternal(Level::Error, module, format, args);
        va_end(args);
    }
    
    void Critical(const char* module, const char* format, ...) {
        if (Level::Critical < m_level) return;
        va_list args;
        va_start(args, format);
        LogInternal(Level::Critical, module, format, args);
        va_end(args);
    }
    
    // Flush all outputs
    void Flush() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_fileStream.is_open()) {
            m_fileStream.flush();
        }
    }
    
private:
    Logger() 
        : m_level(Level::Info)
        , m_output(Output::DebugView)
        , m_callback(nullptr)
    {}
    
    ~Logger() {
        if (m_fileStream.is_open()) {
            m_fileStream.close();
        }
    }
    
    // Non-copyable
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    void LogInternal(Level level, const char* module, const char* format, va_list args) {
        // Format message
        char messageBuffer[2048];
        vsnprintf(messageBuffer, sizeof(messageBuffer), format, args);
        
        // Get timestamp
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::tm tm;
        localtime_s(&tm, &time);
        
        // Build full log line
        char fullBuffer[2560];
        snprintf(fullBuffer, sizeof(fullBuffer),
            "[%02d:%02d:%02d.%03lld] [%s] [%s%s] %s",
            tm.tm_hour, tm.tm_min, tm.tm_sec, (long long)ms.count(),
            LevelToString(level),
            m_modulePrefix.c_str(),
            module,
            messageBuffer
        );
        
        // Output to destinations
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Console output
        if ((m_output & Output::Console) != Output::None) {
            FILE* stream = (level >= Level::Error) ? stderr : stdout;
            fprintf(stream, "%s\n", fullBuffer);
            fflush(stream);
        }
        
        // DebugView output
        if ((m_output & Output::DebugView) != Output::None) {
            OutputDebugStringA(fullBuffer);
            OutputDebugStringA("\n");
        }
        
        // File output
        if ((m_output & Output::File) != Output::None && m_fileStream.is_open()) {
            m_fileStream << fullBuffer << std::endl;
        }
        
        // Callback output
        if ((m_output & Output::Callback) != Output::None && m_callback) {
            std::string moduleStr = m_modulePrefix + module;
            m_callback(level, moduleStr.c_str(), messageBuffer);
        }
    }
    
    const char* LevelToString(Level level) {
        switch (level) {
            case Level::Trace:    return "TRACE";
            case Level::Debug:    return "DEBUG";
            case Level::Info:     return "INFO ";
            case Level::Warning:  return "WARN ";
            case Level::Error:    return "ERROR";
            case Level::Critical: return "CRIT ";
            default:              return "?????";
        }
    }
    
    Level m_level;
    Output m_output;
    LogCallback m_callback;
    std::mutex m_mutex;
    std::ofstream m_fileStream;
    std::string m_modulePrefix;
};

// ============================================
// Convenience Macros
// ============================================

#define MAPPER_LOG(level, module, ...) \
    mapper::log::Logger::Instance().Log(level, module, __VA_ARGS__)

#define MAPPER_TRACE(module, ...) \
    mapper::log::Logger::Instance().Trace(module, __VA_ARGS__)

#define MAPPER_DEBUG(module, ...) \
    mapper::log::Logger::Instance().Debug(module, __VA_ARGS__)

#define MAPPER_INFO(module, ...) \
    mapper::log::Logger::Instance().Info(module, __VA_ARGS__)

#define MAPPER_WARN(module, ...) \
    mapper::log::Logger::Instance().Warning(module, __VA_ARGS__)

#define MAPPER_ERROR(module, ...) \
    mapper::log::Logger::Instance().Error(module, __VA_ARGS__)

#define MAPPER_CRITICAL(module, ...) \
    mapper::log::Logger::Instance().Critical(module, __VA_ARGS__)

// ============================================
// Scoped Timer
// ============================================

class ScopedTimer {
public:
    ScopedTimer(const char* module, const char* operation)
        : m_module(module)
        , m_operation(operation)
        , m_start(std::chrono::high_resolution_clock::now())
    {}
    
    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - m_start);
        
        if (duration.count() >= 1000) {
            MAPPER_DEBUG(m_module, "%s completed in %.2f ms",
                m_operation, duration.count() / 1000.0);
        } else {
            MAPPER_TRACE(m_module, "%s completed in %lld us",
                m_operation, (long long)duration.count());
        }
    }
    
private:
    const char* m_module;
    const char* m_operation;
    std::chrono::high_resolution_clock::time_point m_start;
};

#define MAPPER_TIMER(module, op) \
    mapper::log::ScopedTimer _timer##__LINE__(module, op)

// ============================================
// Error Reporting
// ============================================

class ErrorReporter {
public:
    struct ErrorInfo {
        uint32_t code;
        std::string message;
        std::string module;
        std::string function;
        int line;
    };
    
    static ErrorReporter& Instance() {
        static ErrorReporter instance;
        return instance;
    }
    
    void ReportError(uint32_t code, const char* msg, 
                     const char* module, const char* func, int line) {
        ErrorInfo info;
        info.code = code;
        info.message = msg;
        info.module = module;
        info.function = func;
        info.line = line;
        
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = info;
        m_errors.push_back(info);
        
        MAPPER_ERROR(module, "Error %u at %s:%d: %s", code, func, line, msg);
    }
    
    const ErrorInfo& GetLastError() const { return m_lastError; }
    
    void ClearErrors() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_errors.clear();
    }
    
    size_t GetErrorCount() const { return m_errors.size(); }
    
private:
    ErrorReporter() = default;
    
    std::mutex m_mutex;
    ErrorInfo m_lastError;
    std::vector<ErrorInfo> m_errors;
};

#define MAPPER_REPORT_ERROR(code, msg) \
    mapper::log::ErrorReporter::Instance().ReportError(code, msg, __FILE__, __FUNCTION__, __LINE__)

} // namespace log
} // namespace mapper

