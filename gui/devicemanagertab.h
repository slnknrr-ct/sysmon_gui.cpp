#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QGroupBox>
#include <QTimer>
#include <memory>
#include "../shared/systemtypes.h"
#include "ipcclient.h"

QT_BEGIN_NAMESPACE
class QTableWidget;
class QGroupBox;
class QTimer;
QT_END_NAMESPACE

namespace SysMon {

// Device Manager Tab - manages USB devices
class DeviceManagerTab : public QWidget {
    Q_OBJECT

public:
    DeviceManagerTab(IpcClient* ipcClient, QWidget* parent = nullptr);
    ~DeviceManagerTab();

protected:
    // Event handling
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    // Device operations
    void refreshDevices();
    void enableDevice();
    void disableDevice();
    void toggleAutoConnect();
    void onDeviceSelectionChanged();
    
    // IPC responses
    void onUsbDevicesResponse(const Response& response);
    void onDeviceOperationResponse(const Response& response);
    void onError(const QString& error);

private:
    // UI initialization
    void setupUI();
    void setupDeviceTable();
    void setupControlButtons();
    void setupStatusBar();
    
    // Device table management
    void setupDeviceTableHeaders();
    void updateDeviceTable(const std::vector<UsbDevice>& devices);
    void addDeviceToTable(const UsbDevice& device, int row);
    void clearDeviceTable();
    
    // Device operations
    void enableSelectedDevice();
    void disableSelectedDevice();
    void toggleAutoConnectForSelectedDevice();
    UsbDevice getSelectedDevice() const;
    bool hasValidSelection() const;
    bool isDevicePrevented(const std::string& vid, const std::string& pid) const;
    
    // UI state management
    void updateButtonStates();
    void showDeviceOperationResult(const QString& operation, bool success, const QString& message = "");
    
    // UI components
    std::unique_ptr<QVBoxLayout> mainLayout_;
    
    // Device table section
    std::unique_ptr<QGroupBox> deviceGroup_;
    std::unique_ptr<QVBoxLayout> deviceLayout_;
    std::unique_ptr<QTableWidget> deviceTable_;
    
    // Control buttons section
    std::unique_ptr<QGroupBox> controlGroup_;
    std::unique_ptr<QHBoxLayout> controlLayout_;
    std::unique_ptr<QPushButton> refreshButton_;
    std::unique_ptr<QPushButton> enableButton_;
    std::unique_ptr<QPushButton> disableButton_;
    std::unique_ptr<QPushButton> autoConnectButton_;
    
    // Status bar
    std::unique_ptr<QLabel> statusLabel_;
    std::unique_ptr<QLabel> deviceCountLabel_;
    
    // IPC client
    IpcClient* ipcClient_;
    
    // Update timer
    std::unique_ptr<QTimer> refreshTimer_;
    
    // Data
    std::vector<UsbDevice> currentDevices_;
    
    // Status
    bool isActive_;
    
    // Table columns
    enum DeviceTableColumns {
        COLUMN_VID = 0,
        COLUMN_PID,
        COLUMN_NAME,
        COLUMN_SERIAL,
        COLUMN_STATUS,
        COLUMN_AUTO_CONNECT,
        COLUMN_COUNT
    };
    
    // Constants
    static constexpr int REFRESH_INTERVAL = 3000; // 3 seconds
    static constexpr const char* ENABLE_TEXT = "Enable";
    static constexpr const char* DISABLE_TEXT = "Disable";
    static constexpr const char* PREVENT_AUTO_TEXT = "Prevent Auto";
    static constexpr const char* ALLOW_AUTO_TEXT = "Allow Auto";
};

} // namespace SysMon
