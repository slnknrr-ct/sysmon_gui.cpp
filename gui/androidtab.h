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
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QProgressBar>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QPixmap>
#include <memory>
#include "../shared/systemtypes.h"
#include "ipcclient.h"

QT_BEGIN_NAMESPACE
class QTableWidget;
class QPushButton;
class QLabel;
class QTimer;
class QGroupBox;
class QComboBox;
class QLineEdit;
class QTextEdit;
class QProgressBar;
class QDialog;
class QDialogButtonBox;
class QPixmap;
QT_END_NAMESPACE

namespace SysMon {

// Android Tab - manages Android devices
class AndroidTab : public QWidget {
    Q_OBJECT

public:
    AndroidTab(IpcClient* ipcClient, QWidget* parent = nullptr);
    ~AndroidTab();

protected:
    // Event handling
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    // Device operations
    void refreshDevices();
    void onDeviceSelectionChanged();
    
    // Device control
    void turnScreenOn();
    void turnScreenOff();
    void lockDevice();
    void refreshDeviceInfo();
    
    // App management
    void refreshApps();
    void launchApp();
    void stopApp();
    
    // System operations
    void takeScreenshot();
    void getOrientation();
    void getLogcat();
    
    // IPC responses
    void onAndroidDevicesResponse(const Response& response);
    void onDeviceInfoResponse(const Response& response);
    void onAppListResponse(const Response& response);
    void onDeviceOperationResponse(const Response& response);
    void onScreenshotResponse(const Response& response);
    void onOrientationResponse(const Response& response);
    void onLogcatResponse(const Response& response);
    void onError(const QString& error);

private:
    // UI initialization
    void setupUI();
    void setupDeviceSection();
    void setupDeviceInfoSection();
    void setupDeviceControlSection();
    void setupAppManagementSection();
    void setupSystemOperationsSection();
    void setupStatusBar();
    
    // Device management
    void updateDeviceList(const std::vector<AndroidDeviceInfo>& devices);
    void updateDeviceInfo(const AndroidDeviceInfo& device);
    void updateAppList(const std::vector<std::string>& apps);
    AndroidDeviceInfo getSelectedDevice() const;
    bool hasValidDeviceSelection() const;
    
    // Device operations
    void performDeviceOperation(const std::string& operation);
    void performAppOperation(const std::string& operation);
    void showScreenshot(const QString& imageData);
    void showOrientation(const QString& orientation);
    void showLogcat(const std::vector<std::string>& logcat);
    
    // UI state management
    void updateButtonStates();
    void showDeviceOperationResult(const QString& operation, bool success, const QString& message = "");
    void clearDeviceInfo();
    void clearAppList();
    
    // Dialog creation
    void createScreenshotDialog();
    void createLogcatDialog();
    
    // Formatters
    QString formatBatteryLevel(int level) const;
    QString formatBool(bool value) const;
    
    // UI components
    std::unique_ptr<QVBoxLayout> mainLayout_;
    
    // Device selection section
    std::unique_ptr<QGroupBox> deviceGroup_;
    std::unique_ptr<QHBoxLayout> deviceLayout_;
    std::unique_ptr<QLabel> deviceLabel_;
    std::unique_ptr<QComboBox> deviceCombo_;
    std::unique_ptr<QPushButton> refreshDevicesButton_;
    
    // Device info section
    std::unique_ptr<QGroupBox> infoGroup_;
    std::unique_ptr<QFormLayout> infoLayout_;
    std::unique_ptr<QLabel> modelLabel_;
    std::unique_ptr<QLabel> versionLabel_;
    std::unique_ptr<QLabel> serialLabel_;
    std::unique_ptr<QLabel> batteryLabel_;
    std::unique_ptr<QProgressBar> batteryBar_;
    std::unique_ptr<QLabel> screenLabel_;
    std::unique_ptr<QLabel> lockLabel_;
    std::unique_ptr<QLabel> foregroundLabel_;
    
    // Device control section
    std::unique_ptr<QGroupBox> controlGroup_;
    std::unique_ptr<QHBoxLayout> controlLayout_;
    std::unique_ptr<QPushButton> screenOnButton_;
    std::unique_ptr<QPushButton> screenOffButton_;
    std::unique_ptr<QPushButton> lockButton_;
    std::unique_ptr<QPushButton> refreshInfoButton_;
    
    // App management section
    std::unique_ptr<QGroupBox> appGroup_;
    std::unique_ptr<QVBoxLayout> appLayout_;
    std::unique_ptr<QHBoxLayout> appControlLayout_;
    std::unique_ptr<QPushButton> refreshAppsButton_;
    std::unique_ptr<QLineEdit> appEdit_;
    std::unique_ptr<QPushButton> launchAppButton_;
    std::unique_ptr<QPushButton> stopAppButton_;
    std::unique_ptr<QTableWidget> appTable_;
    
    // System operations section
    std::unique_ptr<QGroupBox> systemGroup_;
    std::unique_ptr<QHBoxLayout> systemLayout_;
    std::unique_ptr<QPushButton> screenshotButton_;
    std::unique_ptr<QPushButton> orientationButton_;
    std::unique_ptr<QPushButton> logcatButton_;
    
    // Status bar
    std::unique_ptr<QLabel> statusLabel_;
    std::unique_ptr<QLabel> deviceCountLabel_;
    
    // Dialogs
    std::unique_ptr<QDialog> screenshotDialog_;
    std::unique_ptr<QLabel> screenshotLabel_;
    std::unique_ptr<QDialog> logcatDialog_;
    std::unique_ptr<QTextEdit> logcatEdit_;
    
    // IPC client
    IpcClient* ipcClient_;
    
    // Update timers
    std::unique_ptr<QTimer> deviceRefreshTimer_;
    std::unique_ptr<QTimer> infoRefreshTimer_;
    
    // Data
    std::vector<AndroidDeviceInfo> currentDevices_;
    std::vector<std::string> currentApps_;
    AndroidDeviceInfo currentDeviceInfo_;
    
    // Status
    bool isActive_;
    
    // Table columns
    enum AppTableColumns {
        COLUMN_PACKAGE_NAME = 0,
        COLUMN_COUNT
    };
    
    // Constants
    static constexpr int DEVICE_REFRESH_INTERVAL = 5000; // 5 seconds
    static constexpr int INFO_REFRESH_INTERVAL = 2000; // 2 seconds
    static constexpr int MAX_LOGCAT_LINES = 100;
};

} // namespace SysMon
