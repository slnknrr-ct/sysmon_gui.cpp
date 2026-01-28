#pragma once

#include "../shared/systemtypes.h"
#include <string>
#include <fstream>
#include <memory>
#include <thread>
#include <atomic>
#include <shared_mutex>
#include <queue>
#include <condition_variable>

namespace SysMon {

// Logger - thread-safe logging system with rotation
class Logger {
public:
    Logger();
    ~Logger();
    
    // Initialization
    bool initialize(const std::string& logFilePath, LogLevel minLevel = LogLevel::INFO);
    bool start();
    void stop();
    void shutdown();
    
    // Logging functions
    void log(LogLevel level, const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    
    // Configuration
    void setMinLevel(LogLevel level);
    LogLevel getMinLevel() const;
    void setMaxFileSize(size_t maxSizeBytes);
    void setMaxBackupFiles(size_t maxBackupFiles);
    
    // Status
    bool isRunning() const;
    size_t getCurrentLogSize() const;

private:
    // Log entry structure
    struct LogEntry {
        LogLevel level;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
        std::thread::id threadId;
    };
    
    // Logging thread
    void loggingThread();
    void processLogEntry(const LogEntry& entry);
    
    // File operations
    bool openLogFile();
    void closeLogFile();
    void rotateLogFile();
    std::string generateLogFileName() const;
    std::string generateBackupFileName(size_t backupNumber) const;
    
    // Formatting
    std::string formatLogEntry(const LogEntry& entry) const;
    std::string levelToString(LogLevel level) const;
    std::string timestampToString(const std::chrono::system_clock::time_point& timestamp) const;
    
    // Thread management
    std::thread loggingThread_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    
    // Log queue
    std::queue<LogEntry> logQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    
    // File management
    std::ofstream logFile_;
    std::string logFilePath_;
    std::string logDirectory_;
    std::string logFileName_;
    
    // Configuration
    LogLevel minLevel_;
    size_t maxFileSize_;
    size_t maxBackupFiles_;
    mutable std::shared_mutex configMutex_;
    
    // Statistics
    std::atomic<size_t> currentLogSize_;
    std::atomic<size_t> totalEntries_;
    
    // Constants
    static constexpr size_t DEFAULT_MAX_FILE_SIZE = 10 * 1024 * 1024; // 10MB
    static constexpr size_t DEFAULT_MAX_BACKUP_FILES = 5;
    static constexpr size_t MAX_QUEUE_SIZE = 10000;
};

// Convenience macros for logging (agent-specific)
#define AGENT_LOG_INFO(logger, message) (logger)->info(message)
#define AGENT_LOG_WARNING(logger, message) (logger)->warning(message)
#define AGENT_LOG_ERROR(logger, message) (logger)->error(message)

} // namespace SysMon
