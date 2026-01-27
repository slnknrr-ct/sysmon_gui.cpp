#include "logger.h"
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <sstream>
#include <shared_mutex>

namespace SysMon {

Logger::Logger() 
    : running_(false)
    , initialized_(false)
    , minLevel_(LogLevel::INFO)
    , maxFileSize_(DEFAULT_MAX_FILE_SIZE)
    , maxBackupFiles_(DEFAULT_MAX_BACKUP_FILES)
    , currentLogSize_(0)
    , totalEntries_(0) {
}

Logger::~Logger() {
    shutdown();
}

bool Logger::initialize(const std::string& logFilePath, LogLevel minLevel) {
    if (initialized_) {
        return true;
    }
    
    logFilePath_ = logFilePath;
    minLevel_ = minLevel;
    
    // Extract directory and filename
    size_t lastSlash = logFilePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        logDirectory_ = logFilePath.substr(0, lastSlash);
        logFileName_ = logFilePath.substr(lastSlash + 1);
    } else {
        logDirectory_ = ".";
        logFileName_ = logFilePath;
    }
    
    // Create directory if it doesn't exist
    std::filesystem::create_directories(logDirectory_);
    
    initialized_ = true;
    return true;
}

bool Logger::start() {
    if (!initialized_) {
        return false;
    }
    
    if (running_) {
        return true;
    }
    
    if (!openLogFile()) {
        return false;
    }
    
    running_ = true;
    loggingThread_ = std::thread(&Logger::loggingThread, this);
    
    return true;
}

void Logger::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // Wake up logging thread
    queueCondition_.notify_all();
    
    if (loggingThread_.joinable()) {
        loggingThread_.join();
    }
    
    closeLogFile();
}

void Logger::shutdown() {
    stop();
    initialized_ = false;
}

void Logger::log(LogLevel level, const std::string& message) {
    if (!initialized_ || level < minLevel_) {
        return;
    }
    
    LogEntry entry;
    entry.level = level;
    entry.message = message;
    entry.timestamp = std::chrono::system_clock::now();
    entry.threadId = std::this_thread::get_id();
    
    std::lock_guard<std::mutex> lock(queueMutex_);
    
    if (logQueue_.size() >= MAX_QUEUE_SIZE) {
        // Drop oldest entry if queue is full
        logQueue_.pop();
    }
    
    logQueue_.push(entry);
    queueCondition_.notify_one();
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void Logger::setMinLevel(LogLevel level) {
    std::lock_guard<std::shared_mutex> lock(configMutex_);
    minLevel_ = level;
}

LogLevel Logger::getMinLevel() const {
    std::shared_lock<std::shared_mutex> lock(configMutex_);
    return minLevel_;
}

void Logger::setMaxFileSize(size_t maxSizeBytes) {
    std::lock_guard<std::shared_mutex> lock(configMutex_);
    maxFileSize_ = maxSizeBytes;
}

void Logger::setMaxBackupFiles(size_t maxBackupFiles) {
    std::lock_guard<std::shared_mutex> lock(configMutex_);
    maxBackupFiles_ = maxBackupFiles;
}

bool Logger::isRunning() const {
    return running_;
}

size_t Logger::getCurrentLogSize() const {
    return currentLogSize_;
}

void Logger::loggingThread() {
    while (running_) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        
        queueCondition_.wait(lock, [this] {
            return !logQueue_.empty() || !running_;
        });
        
        while (!logQueue_.empty()) {
            LogEntry entry = logQueue_.front();
            logQueue_.pop();
            lock.unlock();
            
            processLogEntry(entry);
            
            lock.lock();
        }
    }
}

void Logger::processLogEntry(const LogEntry& entry) {
    std::string formattedEntry = formatLogEntry(entry);
    
    if (logFile_.is_open()) {
        logFile_ << formattedEntry << std::endl;
        logFile_.flush();
        
        currentLogSize_ += formattedEntry.length() + 1; // +1 for newline
        totalEntries_++;
        
        // Check if rotation is needed
        if (currentLogSize_ >= maxFileSize_) {
            rotateLogFile();
        }
    }
}

bool Logger::openLogFile() {
    logFile_.open(logFilePath_, std::ios::app);
    if (!logFile_.is_open()) {
        return false;
    }
    
    // Get current file size
    logFile_.seekp(0, std::ios::end);
    currentLogSize_ = logFile_.tellp();
    
    return true;
}

void Logger::closeLogFile() {
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

void Logger::rotateLogFile() {
    closeLogFile();
    
    // Create backup files
    for (int i = static_cast<int>(maxBackupFiles_) - 1; i > 0; --i) {
        std::string oldFile = generateBackupFileName(i);
        std::string newFile = generateBackupFileName(i + 1);
        
        if (std::filesystem::exists(oldFile)) {
            if (i == static_cast<int>(maxBackupFiles_) - 1) {
                std::filesystem::remove(oldFile);
            } else {
                std::filesystem::rename(oldFile, newFile);
            }
        }
    }
    
    // Move current log to backup
    std::string backupFile = generateBackupFileName(1);
    std::filesystem::rename(logFilePath_, backupFile);
    
    // Open new log file
    currentLogSize_ = 0;
    openLogFile();
}

std::string Logger::generateLogFileName() const {
    return logFileName_;
}

std::string Logger::generateBackupFileName(size_t backupNumber) const {
    return logDirectory_ + "/" + logFileName_ + "." + std::to_string(backupNumber);
}

std::string Logger::formatLogEntry(const LogEntry& entry) const {
    std::ostringstream oss;
    
    oss << "[" << timestampToString(entry.timestamp) << "] "
        << "[" << levelToString(entry.level) << "] "
        << "[Thread-" << entry.threadId << "] "
        << entry.message;
    
    return oss.str();
}

std::string Logger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::string Logger::timestampToString(const std::chrono::system_clock::time_point& timestamp) const {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

} // namespace SysMon
