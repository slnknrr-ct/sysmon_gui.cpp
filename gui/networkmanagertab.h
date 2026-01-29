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
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <memory>
#include "../shared/systemtypes.h"
#include "ipcclient.h"

QT_BEGIN_NAMESPACE
class QTableWidget;
class QPushButton;
class QLabel;
class QTimer;
class QGroupBox;
class QLineEdit;
class QDialog;
class QDialogButtonBox;
QT_END_NAMESPACE

namespace SysMon {

// Network Manager Tab - manages network interfaces
class NetworkManagerTab : public QWidget {
    Q_OBJECT

public:
    NetworkManagerTab(IpcClient* ipcClient, QWidget* parent = nullptr);
    ~NetworkManagerTab();

protected:
    // Event handling
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    // Network operations
    void refreshInterfaces();
    void enableInterface();
    void disableInterface();
    void setStaticIp();
    void setDhcpIp();
    void onInterfaceSelectionChanged();
    
    // IPC responses
    void onNetworkInterfacesResponse(const Response& response);
    void onInterfaceOperationResponse(const Response& response);
    void onError(const QString& error);

private:
    // UI initialization
    void setupUI();
    void setupInterfaceTable();
    void setupControlButtons();
    void setupStatusBar();
    
    // Interface table management
    void setupInterfaceTableHeaders();
    void updateInterfaceTable(const std::vector<NetworkInterface>& interfaces);
    void addInterfaceToTable(const NetworkInterface& interface, int row);
    void clearInterfaceTable();
    
    // Interface operations
    void enableSelectedInterface();
    void disableSelectedInterface();
    void showStaticIpDialog();
    void setStaticIpForSelectedInterface(const QString& ip, const QString& netmask, const QString& gateway);
    void setDhcpForSelectedInterface();
    NetworkInterface getSelectedInterface() const;
    bool hasValidSelection() const;
    
    // Formatters
    QString formatSpeed(double speed) const;
    
    // UI state management
    void updateButtonStates();
    void showInterfaceOperationResult(const QString& operation, bool success, const QString& message = "");
    
    // Static IP dialog
    void createStaticIpDialog();
    void resetStaticIpDialog();
    
    // UI components
    std::unique_ptr<QVBoxLayout> mainLayout_;
    
    // Interface table section
    std::unique_ptr<QGroupBox> interfaceGroup_;
    std::unique_ptr<QVBoxLayout> interfaceLayout_;
    std::unique_ptr<QTableWidget> interfaceTable_;
    
    // Control buttons section
    std::unique_ptr<QGroupBox> controlGroup_;
    std::unique_ptr<QHBoxLayout> controlLayout_;
    std::unique_ptr<QPushButton> refreshButton_;
    std::unique_ptr<QPushButton> enableButton_;
    std::unique_ptr<QPushButton> disableButton_;
    std::unique_ptr<QPushButton> staticIpButton_;
    std::unique_ptr<QPushButton> dhcpButton_;
    
    // Status bar
    std::unique_ptr<QLabel> statusLabel_;
    std::unique_ptr<QLabel> interfaceCountLabel_;
    
    // Static IP dialog
    std::unique_ptr<QDialog> staticIpDialog_;
    std::unique_ptr<QFormLayout> dialogLayout_;
    std::unique_ptr<QLineEdit> ipEdit_;
    std::unique_ptr<QLineEdit> netmaskEdit_;
    std::unique_ptr<QLineEdit> gatewayEdit_;
    std::unique_ptr<QDialogButtonBox> dialogButtons_;
    
    // IPC client
    IpcClient* ipcClient_;
    
    // Update timer
    std::unique_ptr<QTimer> refreshTimer_;
    
    // Data
    std::vector<NetworkInterface> currentInterfaces_;
    
    // Status
    bool isActive_;
    
    // Table columns
    enum InterfaceTableColumns {
        COLUMN_NAME = 0,
        COLUMN_IPV4,
        COLUMN_IPV6,
        COLUMN_STATUS,
        COLUMN_RX_SPEED,
        COLUMN_TX_SPEED,
        COLUMN_COUNT
    };
    
    // Constants
    static constexpr int REFRESH_INTERVAL = 2000; // 2 seconds
    static constexpr const char* ENABLE_TEXT = "Enable";
    static constexpr const char* DISABLE_TEXT = "Disable";
    static constexpr const char* STATIC_IP_TEXT = "Set Static IP";
    static constexpr const char* DHCP_IP_TEXT = "Set DHCP";
};

} // namespace SysMon
