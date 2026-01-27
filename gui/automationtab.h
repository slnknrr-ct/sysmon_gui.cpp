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
#include <QTextEdit>
#include <QCheckBox>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <memory>
#include "../shared/systemtypes.h"

QT_BEGIN_NAMESPACE
class QTableWidget;
class QPushButton;
class QLabel;
class QTimer;
class QGroupBox;
class QLineEdit;
class QTextEdit;
class QCheckBox;
class QDialog;
class QDialogButtonBox;
class QMessageBox;
QT_END_NAMESPACE

// Forward declarations
class IpcClient;

namespace SysMon {

// Automation Tab - manages automation rules
class AutomationTab : public QWidget {
    Q_OBJECT

public:
    AutomationTab(IpcClient* ipcClient, QWidget* parent = nullptr);
    ~AutomationTab();

protected:
    // Event handling
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    // Rule management
    void refreshRules();
    void addRule();
    void editRule();
    void deleteRule();
    void enableRule();
    void disableRule();
    void onRuleSelectionChanged();
    
    // IPC responses
    void onAutomationRulesResponse(const Response& response);
    void onRuleOperationResponse(const Response& response);
    void onError(const QString& error);

private:
    // UI initialization
    void setupUI();
    void setupRuleTable();
    void setupControlButtons();
    void setupStatusBar();
    
    // Rule table management
    void setupRuleTableHeaders();
    void updateRuleTable(const std::vector<AutomationRule>& rules);
    void addRuleToTable(const AutomationRule& rule, int row);
    void clearRuleTable();
    
    // Rule operations
    void addNewRule();
    void editSelectedRule();
    void deleteSelectedRule();
    void enableSelectedRule();
    void disableSelectedRule();
    AutomationRule getSelectedRule() const;
    bool hasValidSelection() const;
    
    // Rule dialog
    void createRuleDialog();
    void resetRuleDialog();
    void showRuleDialog(AutomationRule* rule = nullptr);
    bool validateRule(const AutomationRule& rule) const;
    AutomationRule createRuleFromDialog() const;
    void populateDialogFromRule(const AutomationRule& rule);
    
    // UI state management
    void updateButtonStates();
    void showRuleOperationResult(const QString& operation, bool success, const QString& message = "");
    
    // Rule helpers
    QString generateRuleId() const;
    bool isValidCondition(const std::string& condition) const;
    bool isValidAction(const std::string& action) const;
    QString formatRuleEnabled(bool enabled) const;
    
    // UI components
    std::unique_ptr<QVBoxLayout> mainLayout_;
    
    // Rule table section
    std::unique_ptr<QGroupBox> ruleGroup_;
    std::unique_ptr<QVBoxLayout> ruleLayout_;
    std::unique_ptr<QTableWidget> ruleTable_;
    
    // Control buttons section
    std::unique_ptr<QGroupBox> controlGroup_;
    std::unique_ptr<QHBoxLayout> controlLayout_;
    std::unique_ptr<QPushButton> refreshButton_;
    std::unique_ptr<QPushButton> addButton_;
    std::unique_ptr<QPushButton> editButton_;
    std::unique_ptr<QPushButton> deleteButton_;
    std::unique_ptr<QPushButton> enableButton_;
    std::unique_ptr<QPushButton> disableButton_;
    
    // Status bar
    std::unique_ptr<QLabel> statusLabel_;
    std::unique_ptr<QLabel> ruleCountLabel_;
    
    // Rule dialog
    std::unique_ptr<QDialog> ruleDialog_;
    std::unique_ptr<QFormLayout> dialogLayout_;
    std::unique_ptr<QLineEdit> conditionEdit_;
    std::unique_ptr<QLineEdit> actionEdit_;
    std::unique_ptr<QCheckBox> enabledCheckBox_;
    std::unique_ptr<QLineEdit> durationEdit_;
    std::unique_ptr<QLabel> durationLabel_;
    std::unique_ptr<QDialogButtonBox> dialogButtons_;
    
    // IPC client
    IpcClient* ipcClient_;
    
    // Update timer
    std::unique_ptr<QTimer> refreshTimer_;
    
    // Data
    std::vector<AutomationRule> currentRules_;
    
    // Status
    bool isActive_;
    
    // Table columns
    enum RuleTableColumns {
        COLUMN_ID = 0,
        COLUMN_CONDITION,
        COLUMN_ACTION,
        COLUMN_ENABLED,
        COLUMN_DURATION,
        COLUMN_COUNT
    };
    
    // Constants
    static constexpr int REFRESH_INTERVAL = 3000; // 3 seconds
    static constexpr const char* ADD_TEXT = "Add Rule";
    static constexpr const char* EDIT_TEXT = "Edit Rule";
    static constexpr const char* DELETE_TEXT = "Delete Rule";
    static constexpr const char* ENABLE_TEXT = "Enable";
    static constexpr const char* DISABLE_TEXT = "Disable";
};

} // namespace SysMon
