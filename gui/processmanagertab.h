#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QGroupBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QInputDialog>
#include <memory>
#include "../shared/systemtypes.h"

QT_BEGIN_NAMESPACE
class QTableWidget;
class QPushButton;
class QLabel;
class QTimer;
class QGroupBox;
class QLineEdit;
class QMessageBox;
QT_END_NAMESPACE

// Forward declarations
class IpcClient;

namespace SysMon {

// Process Manager Tab - manages system processes
class ProcessManagerTab : public QWidget {
    Q_OBJECT

public:
    ProcessManagerTab(IpcClient* ipcClient, QWidget* parent = nullptr);
    ~ProcessManagerTab();

protected:
    // Event handling
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    // Process operations
    void refreshProcesses();
    void terminateProcess();
    void killProcess();
    void searchProcesses();
    void onProcessSelectionChanged();
    void onSearchTextChanged();
    
    // IPC responses
    void onProcessListResponse(const Response& response);
    void onProcessOperationResponse(const Response& response);
    void onError(const QString& error);

private:
    // UI initialization
    void setupUI();
    void setupProcessTable();
    void setupControlButtons();
    void setupSearchBar();
    void setupStatusBar();
    
    // Process table management
    void setupProcessTableHeaders();
    void updateProcessTable(const std::vector<ProcessInfo>& processes);
    void addProcessToTable(const ProcessInfo& process, int row);
    void clearProcessTable();
    void filterProcesses();
    
    // Process operations
    void terminateSelectedProcess();
    void killSelectedProcess();
    ProcessInfo getSelectedProcess() const;
    bool hasValidSelection() const;
    bool isCriticalProcess(uint32_t pid) const;
    void confirmProcessTermination(const ProcessInfo& process, bool kill);
    
    // UI state management
    void updateButtonStates();
    void showProcessOperationResult(const QString& operation, bool success, const QString& message = "");
    void updateStatusBar();
    
    // Formatters
    QString formatBytes(uint64_t bytes) const;
    QString formatPercentage(double value) const;
    QString formatProcessStatus(const std::string& status) const;
    
    // UI components
    std::unique_ptr<QVBoxLayout> mainLayout_;
    
    // Search bar section
    std::unique_ptr<QGroupBox> searchGroup_;
    std::unique_ptr<QHBoxLayout> searchLayout_;
    std::unique_ptr<QLabel> searchLabel_;
    std::unique_ptr<QLineEdit> searchEdit_;
    std::unique_ptr<QPushButton> clearSearchButton_;
    
    // Process table section
    std::unique_ptr<QGroupBox> processGroup_;
    std::unique_ptr<QVBoxLayout> processLayout_;
    std::unique_ptr<QTableWidget> processTable_;
    
    // Control buttons section
    std::unique_ptr<QGroupBox> controlGroup_;
    std::unique_ptr<QHBoxLayout> controlLayout_;
    std::unique_ptr<QPushButton> refreshButton_;
    std::unique_ptr<QPushButton> terminateButton_;
    std::unique_ptr<QPushButton> killButton_;
    
    // Status bar
    std::unique_ptr<QLabel> statusLabel_;
    std::unique_ptr<QLabel> processCountLabel_;
    
    // IPC client
    IpcClient* ipcClient_;
    
    // Update timer
    std::unique_ptr<QTimer> refreshTimer_;
    
    // Data
    std::vector<ProcessInfo> currentProcesses_;
    std::vector<ProcessInfo> filteredProcesses_;
    QString currentSearchText_;
    
    // Status
    bool isActive_;
    
    // Table columns
    enum ProcessTableColumns {
        COLUMN_PID = 0,
        COLUMN_NAME,
        COLUMN_CPU,
        COLUMN_MEMORY,
        COLUMN_STATUS,
        COLUMN_USER,
        COLUMN_PARENT_PID,
        COLUMN_COUNT
    };
    
    // Constants
    static constexpr int REFRESH_INTERVAL = 2000; // 2 seconds
    static constexpr int MAX_PROCESSES_DISPLAY = 200;
    static constexpr const char* TERMINATE_TEXT = "Terminate";
    static constexpr const char* KILL_TEXT = "Kill";
};

} // namespace SysMon
