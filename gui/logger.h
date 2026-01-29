#pragma once

#include "../shared/systemtypes.h"
#include <string>
#include <fstream>
#include <memory>
#include <QObject>
#include <QTextEdit>
#include <QTimer>
#include <QMutex>
#include <QQueue>
#include <thread>

QT_BEGIN_NAMESPACE
class QTextEdit;
class QTimer;
QT_END_NAMESPACE

namespace SysMon {

// GUI Logger - displays log messages in GUI
class GuiLogger : public QObject {
    Q_OBJECT

public:
    GuiLogger(QTextEdit* logWidget, QObject* parent = nullptr);
    ~GuiLogger();
    
    // Logging functions
    void log(LogLevel level, const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    
    // Configuration
    void setMaxLogEntries(size_t maxEntries);
    void setLogLevel(LogLevel minLevel);
    void clearLog();
    
    // Status
    size_t getLogEntryCount() const;

public slots:
    void clearLogWidget();

private slots:
    void processLogQueue();

private:
    // Log entry structure
    struct LogEntry {
        LogLevel level;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
        std::thread::id threadId;
    };
    
    // Log processing
    void addLogEntry(const LogEntry& entry);
    void displayLogEntry(const LogEntry& entry);
    
    // Formatting
    QString formatLogEntry(const LogEntry& entry) const;
    QString levelToString(LogLevel level) const;
    QString timestampToString(const std::chrono::system_clock::time_point& timestamp) const;
    QColor getLevelColor(LogLevel level) const;
    
    // Log widget
    QTextEdit* logWidget_;
    
    // Log queue for thread safety
    QQueue<LogEntry> logQueue_;
    QMutex queueMutex_;
    
    // Timer for processing queue
    std::unique_ptr<QTimer> processTimer_;
    
    // Configuration
    LogLevel minLevel_;
    size_t maxLogEntries_;
    
    // Statistics
    std::atomic<size_t> totalEntries_;
    
    // Constants
    static constexpr size_t DEFAULT_MAX_ENTRIES = 1000;
    static constexpr size_t MAX_QUEUE_SIZE = 10000;
    static constexpr int PROCESS_INTERVAL = 100; // 100ms
};

// Convenience macros for GUI logging
#define GUI_LOG_INFO(logger, message) (logger)->info(message)
#define GUI_LOG_WARNING(logger, message) (logger)->warning(message)
#define GUI_LOG_ERROR(logger, message) (logger)->error(message)

} // namespace SysMon
