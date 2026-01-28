#include "logger.h"
#include <iostream>
#include <filesystem>
#include <iomanip>

namespace SysMon {
namespace Logging {

// FileLogger implementation
FileLogger::FileLogger(const std::string& filename, size_t maxFileSize, int maxFiles)
    : filename_(filename)
    , maxFileSize_(maxFileSize)
    , maxFiles_(maxFiles) {
    
    file_.open(filename_, std::ios::app);
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open log file: " + filename_);
    }
}

FileLogger::~FileLogger() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.close();
    }
}

void FileLogger::log(const LogEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!file_.is_open()) {
        return;
    }
    
    std::string formattedEntry = formatLogEntry(entry);
    file_ << formattedEntry << std::endl;
    file_.flush();
    
    // Check if we need to rotate
    if (file_.tellp() > static_cast<std::streampos>(maxFileSize_)) {
        rotateLogFile();
    }
}

void FileLogger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.flush();
    }
}

void FileLogger::rotateLogFile() {
    file_.close();
    
    // Remove oldest log file if it exists
    std::string oldestFile = filename_ + "." + std::to_string(maxFiles_);
    if (std::filesystem::exists(oldestFile)) {
        std::filesystem::remove(oldestFile);
    }
    
    // Rotate existing files
    for (int i = maxFiles_ - 1; i >= 1; --i) {
        std::string oldFile = filename_ + "." + std::to_string(i);
        std::string newFile = filename_ + "." + std::to_string(i + 1);
        if (std::filesystem::exists(oldFile)) {
            std::filesystem::rename(oldFile, newFile);
        }
    }
    
    // Move current file to .1
    std::string backupFile = filename_ + ".1";
    if (std::filesystem::exists(filename_)) {
        std::filesystem::rename(filename_, backupFile);
    }
    
    // Open new file
    file_.open(filename_, std::ios::app);
}

std::string FileLogger::formatLogEntry(const LogEntry& entry) {
    std::ostringstream oss;
    oss << "[" << getCurrentTimestamp() << "] "
        << "[" << logLevelToString(entry.level) << "] "
        << "[" << entry.category << "] "
        << "[Thread-" << entry.threadId << "] ";
    
    if (!entry.function.empty()) {
        oss << "[" << entry.function << "()";
        if (entry.line > 0) {
            oss << ":" << entry.line;
        }
        oss << "] ";
    }
    
    oss << entry.message;
    return oss.str();
}

std::string FileLogger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string FileLogger::logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

// ConsoleLogger implementation
ConsoleLogger::ConsoleLogger(bool colored) : colored_(colored) {}

void ConsoleLogger::log(const LogEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string formattedEntry = formatLogEntry(entry);
    
    if (colored_) {
        std::cout << getColorCode(entry.level) << formattedEntry << resetColorCode() << std::endl;
    } else {
        std::cout << formattedEntry << std::endl;
    }
}

void ConsoleLogger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout.flush();
}

std::string ConsoleLogger::formatLogEntry(const LogEntry& entry) {
    std::ostringstream oss;
    oss << "[" << getCurrentTimestamp() << "] "
        << "[" << logLevelToString(entry.level) << "] "
        << "[" << entry.category << "] "
        << entry.message;
    return oss.str();
}

std::string ConsoleLogger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    return oss.str();
}

std::string ConsoleLogger::logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO ";
        case LogLevel::WARNING: return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRIT ";
        default: return "UNKN ";
    }
}

std::string ConsoleLogger::getColorCode(LogLevel level) {
    if (!colored_) return "";
    
    switch (level) {
        case LogLevel::TRACE: return "\033[37m";      // White
        case LogLevel::DEBUG: return "\033[36m";      // Cyan
        case LogLevel::INFO: return "\033[32m";       // Green
        case LogLevel::WARNING: return "\033[33m";    // Yellow
        case LogLevel::ERROR: return "\033[31m";      // Red
        case LogLevel::CRITICAL: return "\033[35m";   // Magenta
        default: return "\033[0m";
    }
}

std::string ConsoleLogger::resetColorCode() {
    return colored_ ? "\033[0m" : "";
}

// AsyncLogger implementation
AsyncLogger::AsyncLogger(std::unique_ptr<Logger> underlyingLogger, size_t queueSize)
    : underlyingLogger_(std::move(underlyingLogger))
    , maxQueueSize_(queueSize)
    , running_(true) {
    
    workerThread_ = std::thread(&AsyncLogger::workerThread, this);
}

AsyncLogger::~AsyncLogger() {
    running_ = false;
    condition_.notify_all();
    
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

void AsyncLogger::log(const LogEntry& entry) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    
    if (logQueue_.size() >= maxQueueSize_) {
        // Drop oldest entry if queue is full
        logQueue_.pop();
    }
    
    logQueue_.push(entry);
    condition_.notify_one();
}

void AsyncLogger::flush() {
    // Wait for queue to be empty
    std::unique_lock<std::mutex> lock(queueMutex_);
    condition_.wait(lock, [this] { return logQueue_.empty(); });
    
    if (underlyingLogger_) {
        underlyingLogger_->flush();
    }
}

void AsyncLogger::workerThread() {
    while (running_) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        
        condition_.wait(lock, [this] { return !logQueue_.empty() || !running_; });
        
        while (!logQueue_.empty()) {
            LogEntry entry = logQueue_.front();
            logQueue_.pop();
            lock.unlock();
            
            if (underlyingLogger_) {
                underlyingLogger_->log(entry);
            }
            
            lock.lock();
        }
    }
}

// CompositeLogger implementation
void CompositeLogger::addLogger(std::unique_ptr<Logger> logger) {
    std::lock_guard<std::mutex> lock(mutex_);
    loggers_.push_back(std::move(logger));
}

void CompositeLogger::log(const LogEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& logger : loggers_) {
        if (logger) {
            logger->log(entry);
        }
    }
}

void CompositeLogger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& logger : loggers_) {
        if (logger) {
            logger->flush();
        }
    }
}

// LogManager implementation
LogManager& LogManager::getInstance() {
    static LogManager instance;
    return instance;
}

void LogManager::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentLevel_ = level;
}

void LogManager::addLogger(std::unique_ptr<Logger> logger) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!compositeLogger_) {
        compositeLogger_ = std::make_unique<CompositeLogger>();
    }
    
    compositeLogger_->addLogger(std::move(logger));
}

void LogManager::initialize(const std::string& logFile, LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return;
    }
    
    currentLevel_ = level;
    
    // Create file logger
    auto fileLogger = std::make_unique<FileLogger>(logFile);
    
    // Create async logger wrapper
    auto asyncLogger = std::make_unique<AsyncLogger>(std::move(fileLogger));
    
    // Create composite logger
    compositeLogger_ = std::make_unique<CompositeLogger>();
    compositeLogger_->addLogger(std::move(asyncLogger));
    
    // Also add console logger for errors and above
    auto consoleLogger = std::make_unique<ConsoleLogger>(true);
    compositeLogger_->addLogger(std::move(consoleLogger));
    
    initialized_ = true;
}

void LogManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (compositeLogger_) {
        compositeLogger_->flush();
        compositeLogger_.reset();
    }
    
    initialized_ = false;
}

void LogManager::log(LogLevel level, const std::string& message, const std::string& category,
                     const std::string& file, int line, const std::string& function) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || level < currentLevel_) {
        return;
    }
    
    LogEntry entry;
    entry.level = level;
    entry.message = message;
    entry.category = category;
    entry.file = file;
    entry.line = line;
    entry.function = function;
    entry.timestamp = std::chrono::system_clock::now();
    
    if (compositeLogger_) {
        compositeLogger_->log(entry);
    }
}

void LogManager::trace(const std::string& message, const std::string& category) {
    log(LogLevel::TRACE, message, category);
}

void LogManager::debug(const std::string& message, const std::string& category) {
    log(LogLevel::DEBUG, message, category);
}

void LogManager::info(const std::string& message, const std::string& category) {
    log(LogLevel::INFO, message, category);
}

void LogManager::warning(const std::string& message, const std::string& category) {
    log(LogLevel::WARNING, message, category);
}

void LogManager::error(const std::string& message, const std::string& category) {
    log(LogLevel::ERROR, message, category);
}

void LogManager::critical(const std::string& message, const std::string& category) {
    log(LogLevel::CRITICAL, message, category);
}

void LogManager::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (compositeLogger_) {
        compositeLogger_->flush();
    }
}

} // namespace Logging
} // namespace SysMon
