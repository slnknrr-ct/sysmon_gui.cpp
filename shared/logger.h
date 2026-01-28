#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>

namespace SysMon {
namespace Logging {

// Log levels
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARNING = 3,
    ERROR = 4,
    CRITICAL = 5
};

// Log entry structure
struct LogEntry {
    LogLevel level;
    std::string message;
    std::string category;
    std::string file;
    int line;
    std::string function;
    std::chrono::system_clock::time_point timestamp;
    std::thread::id threadId;
    
    LogEntry() : line(0), threadId(std::this_thread::get_id()) {}
};

// Logger interface
class Logger {
public:
    virtual ~Logger() = default;
    virtual void log(const LogEntry& entry) = 0;
    virtual void flush() = 0;
};

// File logger with rotation
class FileLogger : public Logger {
public:
    FileLogger(const std::string& filename, size_t maxFileSize = 10 * 1024 * 1024, int maxFiles = 5);
    ~FileLogger() override;
    
    void log(const LogEntry& entry) override;
    void flush() override;
    
    void setMaxFileSize(size_t size) { maxFileSize_ = size; }
    void setMaxFiles(int count) { maxFiles_ = count; }
    
private:
    void rotateLogFile();
    std::string formatLogEntry(const LogEntry& entry);
    std::string getCurrentTimestamp();
    std::string logLevelToString(LogLevel level);
    
    std::string filename_;
    std::ofstream file_;
    size_t maxFileSize_;
    int maxFiles_;
    mutable std::mutex mutex_;
};

// Console logger
class ConsoleLogger : public Logger {
public:
    ConsoleLogger(bool colored = true);
    ~ConsoleLogger() override = default;
    
    void log(const LogEntry& entry) override;
    void flush() override;
    
private:
    std::string formatLogEntry(const LogEntry& entry);
    std::string getCurrentTimestamp();
    std::string logLevelToString(LogLevel level);
    std::string getColorCode(LogLevel level);
    std::string resetColorCode();
    
    bool colored_;
    mutable std::mutex mutex_;
};

// Async logger with background thread
class AsyncLogger : public Logger {
public:
    AsyncLogger(std::unique_ptr<Logger> underlyingLogger, size_t queueSize = 10000);
    ~AsyncLogger() override;
    
    void log(const LogEntry& entry) override;
    void flush() override;
    
private:
    void workerThread();
    
    std::unique_ptr<Logger> underlyingLogger_;
    std::queue<LogEntry> logQueue_;
    size_t maxQueueSize_;
    std::atomic<bool> running_;
    std::thread workerThread_;
    mutable std::mutex queueMutex_;
    std::condition_variable condition_;
};

// Composite logger (multiple outputs)
class CompositeLogger : public Logger {
public:
    void addLogger(std::unique_ptr<Logger> logger);
    void log(const LogEntry& entry) override;
    void flush() override;
    
private:
    std::vector<std::unique_ptr<Logger>> loggers_;
    mutable std::mutex mutex_;
};

// Main logging manager
class LogManager {
public:
    static LogManager& getInstance();
    
    // Configuration
    void setLogLevel(LogLevel level);
    void addLogger(std::unique_ptr<Logger> logger);
    void initialize(const std::string& logFile = "sysmon.log", LogLevel level = LogLevel::INFO);
    void shutdown();
    
    // Logging methods
    void log(LogLevel level, const std::string& message, const std::string& category = "",
             const std::string& file = "", int line = 0, const std::string& function = "");
    
    // Convenience methods
    void trace(const std::string& message, const std::string& category = "");
    void debug(const std::string& message, const std::string& category = "");
    void info(const std::string& message, const std::string& category = "");
    void warning(const std::string& message, const std::string& category = "");
    void error(const std::string& message, const std::string& category = "");
    void critical(const std::string& message, const std::string& category = "");
    
    // Flush all loggers
    void flush();
    
    // Get current log level
    LogLevel getLogLevel() const { return currentLevel_; }
    
private:
    LogManager() = default;
    ~LogManager() = default;
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;
    
    std::unique_ptr<CompositeLogger> compositeLogger_;
    LogLevel currentLevel_;
    mutable std::mutex mutex_;
    bool initialized_;
};

// Convenience macros
#define LOG_TRACE(msg) \
    SysMon::Logging::LogManager::getInstance().trace(msg, "General")

#define LOG_DEBUG(msg) \
    SysMon::Logging::LogManager::getInstance().debug(msg, "General")

#define LOG_INFO(msg) \
    SysMon::Logging::LogManager::getInstance().info(msg, "General")

#define LOG_WARNING(msg) \
    SysMon::Logging::LogManager::getInstance().warning(msg, "General")

#define LOG_ERROR(msg) \
    SysMon::Logging::LogManager::getInstance().error(msg, "General")

#define LOG_CRITICAL(msg) \
    SysMon::Logging::LogManager::getInstance().critical(msg, "General")

// Category-specific macros
#define LOG_TRACE_CAT(cat, msg) \
    SysMon::Logging::LogManager::getInstance().trace(msg, cat)

#define LOG_DEBUG_CAT(cat, msg) \
    SysMon::Logging::LogManager::getInstance().debug(msg, cat)

#define LOG_INFO_CAT(cat, msg) \
    SysMon::Logging::LogManager::getInstance().info(msg, cat)

#define LOG_WARNING_CAT(cat, msg) \
    SysMon::Logging::LogManager::getInstance().warning(msg, cat)

#define LOG_ERROR_CAT(cat, msg) \
    SysMon::Logging::LogManager::getInstance().error(msg, cat)

#define LOG_CRITICAL_CAT(cat, msg) \
    SysMon::Logging::LogManager::getInstance().critical(msg, cat)

// Function-specific macros with file/line info
#define LOG_FUNCTION_TRACE(msg) \
    SysMon::Logging::LogManager::getInstance().log( \
        SysMon::Logging::LogLevel::TRACE, msg, "Function", __FILE__, __LINE__, __FUNCTION__)

#define LOG_FUNCTION_DEBUG(msg) \
    SysMon::Logging::LogManager::getInstance().log( \
        SysMon::Logging::LogLevel::DEBUG, msg, "Function", __FILE__, __LINE__, __FUNCTION__)

#define LOG_FUNCTION_INFO(msg) \
    SysMon::Logging::LogManager::getInstance().log( \
        SysMon::Logging::LogLevel::INFO, msg, "Function", __FILE__, __LINE__, __FUNCTION__)

#define LOG_FUNCTION_WARNING(msg) \
    SysMon::Logging::LogManager::getInstance().log( \
        SysMon::Logging::LogLevel::WARNING, msg, "Function", __FILE__, __LINE__, __FUNCTION__)

#define LOG_FUNCTION_ERROR(msg) \
    SysMon::Logging::LogManager::getInstance().log( \
        SysMon::Logging::LogLevel::ERROR, msg, "Function", __FILE__, __LINE__, __FUNCTION__)

#define LOG_FUNCTION_CRITICAL(msg) \
    SysMon::Logging::LogManager::getInstance().log( \
        SysMon::Logging::LogLevel::CRITICAL, msg, "Function", __FILE__, __LINE__, __FUNCTION__)

} // namespace Logging
} // namespace SysMon
