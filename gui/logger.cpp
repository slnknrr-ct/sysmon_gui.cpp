#include "logger.h"
#include <QTextEdit>
#include <QTimer>
#include <QMutex>
#include <QQueue>
#include <QDateTime>
#include <QScrollBar>
#include <QColor>
#include <QMutexLocker>

namespace SysMon {

GuiLogger::GuiLogger(QTextEdit* logWidget, QObject* parent)
    : QObject(parent)
    , logWidget_(logWidget)
    , minLevel_(LogLevel::INFO)
    , maxLogEntries_(DEFAULT_MAX_ENTRIES)
    , totalEntries_(0) {
    
    // Create timer for processing log queue
    processTimer_ = std::make_unique<QTimer>(this);
    connect(processTimer_.get(), &QTimer::timeout,
            this, &GuiLogger::processLogQueue);
    
    processTimer_->start(PROCESS_INTERVAL);
}

GuiLogger::~GuiLogger() {
    // Timer is automatically deleted by Qt
}

void GuiLogger::log(LogLevel level, const std::string& message) {
    if (level < minLevel_) {
        return;
    }
    
    LogEntry entry;
    entry.level = level;
    entry.message = message;
    entry.timestamp = std::chrono::system_clock::now();
    entry.threadId = std::this_thread::get_id();
    
    QMutexLocker locker(&queueMutex_);
    
    if (logQueue_.size() >= MAX_QUEUE_SIZE) {
        // Drop oldest entry if queue is full
        logQueue_.dequeue();
    }
    
    logQueue_.enqueue(entry);
}

void GuiLogger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void GuiLogger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void GuiLogger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void GuiLogger::setMaxLogEntries(size_t maxEntries) {
    maxLogEntries_ = maxEntries;
}

void GuiLogger::setLogLevel(LogLevel minLevel) {
    minLevel_ = minLevel;
}

void GuiLogger::clearLog() {
    if (logWidget_) {
        logWidget_->clear();
    }
    
    totalEntries_ = 0;
}

size_t GuiLogger::getLogEntryCount() const {
    return totalEntries_;
}

void GuiLogger::processLogQueue() {
    QMutexLocker locker(&queueMutex_);
    
    while (!logQueue_.isEmpty()) {
        LogEntry entry = logQueue_.dequeue();
        locker.unlock();
        
        addLogEntry(entry);
        
        locker.relock();
    }
}

void GuiLogger::addLogEntry(const LogEntry& entry) {
    if (!logWidget_) {
        return;
    }
    
    displayLogEntry(entry);
    totalEntries_++;
    
    // Trim log if it exceeds maximum entries
    if (totalEntries_ > maxLogEntries_) {
        // Remove oldest entries (simplified approach)
        QString text = logWidget_->toPlainText();
        QStringList lines = text.split('\n');
        
        if (lines.size() > static_cast<int>(maxLogEntries_)) {
            int removeCount = lines.size() - static_cast<int>(maxLogEntries_);
            lines = lines.mid(removeCount);
            
            logWidget_->setPlainText(lines.join('\n'));
            
            // Move cursor to end
            QTextCursor cursor = logWidget_->textCursor();
            cursor.movePosition(QTextCursor::End);
            logWidget_->setTextCursor(cursor);
        }
    }
}

void GuiLogger::displayLogEntry(const LogEntry& entry) {
    if (!logWidget_) {
        return;
    }
    
    QString formattedEntry = formatLogEntry(entry);
    
    // Append to log widget
    logWidget_->append(formattedEntry);
    
    // Auto-scroll to bottom
    QScrollBar* scrollBar = logWidget_->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

QString GuiLogger::formatLogEntry(const LogEntry& entry) const {
    QString timestamp = timestampToString(entry.timestamp);
    QString level = levelToString(entry.level);
    QString color = getLevelColor(entry.level).name();
    
    return QString("<span style=\"color: %1;\">[%2] [%3] [%4] %5</span>")
        .arg(color)
        .arg(timestamp)
        .arg(level)
        .arg(QString::number(reinterpret_cast<quintptr>(entry.threadId)))
        .arg(QString::fromStdString(entry.message));
}

QString GuiLogger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

QString GuiLogger::timestampToString(const std::chrono::system_clock::time_point& timestamp) const {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()) % 1000;
    
    QDateTime dateTime = QDateTime::fromSecsSinceEpoch(time_t);
    return dateTime.toString("hh:mm:ss.zzz");
}

QColor GuiLogger::getLevelColor(LogLevel level) const {
    switch (level) {
        case LogLevel::INFO: return QColor(0, 0, 255);      // Blue
        case LogLevel::WARNING: return QColor(255, 165, 0);  // Orange
        case LogLevel::ERROR: return QColor(255, 0, 0);      // Red
        default: return QColor(0, 0, 0);                    // Black
    }
}

void GuiLogger::clearLogWidget() {
    clearLog();
}

} // namespace SysMon
