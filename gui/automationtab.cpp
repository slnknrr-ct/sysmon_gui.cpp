#include "automationtab.h"
#include "../shared/commands.h"
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
#include <QShowEvent>
#include <QHideEvent>
#include <QDateTime>
#include <QRandomGenerator>
#include <sstream>
#include <algorithm>
#include <QDebug>

namespace SysMon {

AutomationTab::AutomationTab(IpcClient* ipcClient, QWidget* parent)
    : QWidget(parent)
    , ipcClient_(ipcClient)
    , isActive_(false) {
    
    setupUI();
    
    // Create refresh timer
    refreshTimer_ = std::make_unique<QTimer>(this);
    connect(refreshTimer_.get(), &QTimer::timeout,
            this, &AutomationTab::refreshRules);
    
    // Connect IPC client signals
    connect(ipcClient_, &IpcClient::responseReceived,
            this, [this](const Response& response) {
                if (response.commandId.find("rules") != std::string::npos) {
                    onAutomationRulesResponse(response);
                } else {
                    onRuleOperationResponse(response);
                }
            });
    connect(ipcClient_, &IpcClient::errorOccurred,
            this, &AutomationTab::onError);
}

AutomationTab::~AutomationTab() {
    // Timer is automatically deleted by Qt
}

void AutomationTab::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    
    if (!isActive_) {
        isActive_ = true;
        
        // Start timer
        refreshTimer_->start(REFRESH_INTERVAL);
        
        // Request initial data
        refreshRules();
    }
}

void AutomationTab::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    
    if (isActive_) {
        isActive_ = false;
        
        // Stop timer
        refreshTimer_->stop();
    }
}

void AutomationTab::refreshRules() {
    if (!ipcClient_ || !isActive_) {
        return;
    }
    
    // Create command to get automation rules
    Command command = createCommand(CommandType::GET_AUTOMATION_RULES, Module::AUTOMATION);
    command.id = "rules_" + command.id;
    
    // Send command
    ipcClient_->sendCommand(command);
}

void AutomationTab::addRule() {
    showRuleDialog();
}

void AutomationTab::editRule() {
    if (!hasValidSelection()) {
        return;
    }
    
    AutomationRule rule = getSelectedRule();
    showRuleDialog(&rule);
}

void AutomationTab::deleteRule() {
    if (!hasValidSelection()) {
        return;
    }
    
    AutomationRule rule = getSelectedRule();
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Delete Rule",
        QString("Are you sure you want to delete rule '%1'?").arg(QString::fromStdString(rule.id)),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        // Create command to delete rule
        Command command = createCommand(CommandType::REMOVE_AUTOMATION_RULE, Module::AUTOMATION);
        command.parameters["rule_id"] = rule.id;
        
        // Send command
        ipcClient_->sendCommand(command);
    }
}

void AutomationTab::enableRule() {
    if (!hasValidSelection()) {
        return;
    }
    
    AutomationRule rule = getSelectedRule();
    
    // Create command to enable rule
    Command command = createCommand(CommandType::ENABLE_AUTOMATION_RULE, Module::AUTOMATION);
    command.parameters["rule_id"] = rule.id;
    
    // Send command
    ipcClient_->sendCommand(command);
}

void AutomationTab::disableRule() {
    if (!hasValidSelection()) {
        return;
    }
    
    AutomationRule rule = getSelectedRule();
    
    // Create command to disable rule
    Command command = createCommand(CommandType::DISABLE_AUTOMATION_RULE, Module::AUTOMATION);
    command.parameters["rule_id"] = rule.id;
    
    // Send command
    ipcClient_->sendCommand(command);
}

void AutomationTab::onRuleSelectionChanged() {
    updateButtonStates();
}

void AutomationTab::onAutomationRulesResponse(const Response& response) {
    if (response.status != CommandStatus::SUCCESS) {
        onError(QString("Failed to get automation rules: %1").arg(QString::fromStdString(response.message)));
        return;
    }
    
    // Parse automation rules from response data
    std::vector<AutomationRule> rules;
    
    try {
        // Expect rule data in format: "id1,condition1,action1,enabled1,duration1;id2,condition2,action2,enabled2,duration2;..."
        std::string ruleData = response.data.count("rules") ? response.data.at("rules") : "";
        
        if (!ruleData.empty()) {
            std::stringstream ss(ruleData);
            std::string ruleEntry;
            
            while (std::getline(ss, ruleEntry, ';')) {
                std::stringstream entryStream(ruleEntry);
                std::string field;
                std::vector<std::string> fields;
                
                while (std::getline(entryStream, field, ',')) {
                    fields.push_back(field);
                }
                
                if (fields.size() >= 5) {
                    AutomationRule rule;
                    rule.id = fields[0];
                    rule.condition = fields[1];
                    rule.action = fields[2];
                    rule.isEnabled = (fields[3] == "1" || fields[3] == "true");
                    
                    try {
                        rule.duration = std::chrono::seconds(std::stoi(fields[4]));
                    } catch (const std::exception& e) {
                        rule.duration = std::chrono::seconds(0); // Default duration
                    }
                    
                    rules.push_back(rule);
                }
            }
        }
    } catch (const std::exception& e) {
        onError(QString("Failed to parse rule data: %1").arg(e.what()));
        return;
    }
    
    // If no rules from IPC, use placeholder data for development
    if (rules.empty()) {
        AutomationRule rule;
        rule.id = "rule1";
        rule.condition = "CPU_LOAD > 80%";
        rule.action = "DISABLE_USB 1234:5678";
        rule.isEnabled = true;
        rule.duration = std::chrono::seconds(10);
        rules.push_back(rule);
        
        rule.id = "rule2";
        rule.condition = "ANDROID_BATTERY < 20%";
        rule.action = "LOCK_ANDROID_DEVICE";
        rule.isEnabled = false;
        rule.duration = std::chrono::seconds(5);
        rules.push_back(rule);
    }
    
    currentRules_ = rules;
    
    // Update display
    updateRuleTable(rules);
    updateButtonStates();
}

void AutomationTab::onRuleOperationResponse(const Response& response) {
    QString operation = "Unknown operation";
    
    if (response.status == CommandStatus::SUCCESS) {
        operation = QString::fromStdString(response.message);
        showRuleOperationResult(operation, true);
        
        // Refresh rules
        refreshRules();
    } else {
        showRuleOperationResult(operation, false, QString::fromStdString(response.message));
    }
}

void AutomationTab::onError(const QString& error) {
    qWarning() << "AutomationTab error:" << error;
}

void AutomationTab::setupUI() {
    mainLayout_ = std::make_unique<QVBoxLayout>();
    
    setupRuleTable();
    setupControlButtons();
    setupStatusBar();
    
    mainLayout_->addWidget(ruleGroup_.get());
    mainLayout_->addWidget(controlGroup_.get());
    mainLayout_->addStretch();
    
    setLayout(mainLayout_.get());
}

void AutomationTab::setupRuleTable() {
    ruleGroup_ = std::make_unique<QGroupBox("Automation Rules");
    ruleLayout_ = std::make_unique<QVBoxLayout>();
    
    ruleTable_ = std::make_unique<QTableWidget>();
    setupRuleTableHeaders();
    
    // Connect selection change signal
    connect(ruleTable_.get(), &QTableWidget::itemSelectionChanged,
            this, &AutomationTab::onRuleSelectionChanged);
    
    ruleLayout_->addWidget(ruleTable_.get());
    ruleGroup_->setLayout(ruleLayout_.get());
}

void AutomationTab::setupControlButtons() {
    controlGroup_ = std::make_unique<QGroupBox("Rule Control");
    controlLayout_ = std::make_unique<QHBoxLayout>();
    
    refreshButton_ = std::make_unique<QPushButton("Refresh");
    connect(refreshButton_.get(), &QPushButton::clicked,
            this, &AutomationTab::refreshRules);
    
    addButton_ = std::make_unique<QPushButton(ADD_TEXT);
    connect(addButton_.get(), &QPushButton::clicked,
            this, &AutomationTab::addRule);
    
    editButton_ = std::make_unique<QPushButton(EDIT_TEXT);
    connect(editButton_.get(), &QPushButton::clicked,
            this, &AutomationTab::editRule);
    
    deleteButton_ = std::make_unique<QPushButton(DELETE_TEXT);
    connect(deleteButton_.get(), &QPushButton::clicked,
            this, &AutomationTab::deleteRule);
    
    enableButton_ = std::make_unique<QPushButton(ENABLE_TEXT);
    connect(enableButton_.get(), &QPushButton::clicked,
            this, &AutomationTab::enableRule);
    
    disableButton_ = std::make_unique<QPushButton(DISABLE_TEXT);
    connect(disableButton_.get(), &QPushButton::clicked,
            this, &AutomationTab::disableRule);
    
    controlLayout_->addWidget(refreshButton_.get());
    controlLayout_->addWidget(addButton_.get());
    controlLayout_->addWidget(editButton_.get());
    controlLayout_->addWidget(deleteButton_.get());
    controlLayout_->addWidget(enableButton_.get());
    controlLayout_->addWidget(disableButton_.get());
    controlLayout_->addStretch();
    
    controlGroup_->setLayout(controlLayout_.get());
}

void AutomationTab::setupStatusBar() {
    auto statusLayout = std::make_unique<QHBoxLayout>();
    
    statusLabel_ = std::make_unique<QLabel("Ready");
    ruleCountLabel_ = std::make_unique<QLabel("Rules: 0");
    
    statusLayout->addWidget(statusLabel_.get());
    statusLayout->addStretch();
    statusLayout->addWidget(ruleCountLabel_.get());
    
    mainLayout_->addLayout(statusLayout.release());
}

void AutomationTab::setupRuleTableHeaders() {
    QStringList headers;
    headers << "ID" << "Condition" << "Action" << "Enabled" << "Duration (s)";
    
    ruleTable_->setColumnCount(headers.size());
    ruleTable_->setHorizontalHeaderLabels(headers);
    ruleTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    ruleTable_->setSortingEnabled(true);
    
    // Adjust column widths
    ruleTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents); // ID
    ruleTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch); // Condition
    ruleTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch); // Action
    ruleTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents); // Enabled
    ruleTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents); // Duration
}

void AutomationTab::updateRuleTable(const std::vector<AutomationRule>& rules) {
    ruleTable_->setRowCount(static_cast<int>(rules.size()));
    
    for (int row = 0; row < static_cast<int>(rules.size()); ++row) {
        addRuleToTable(rules[row], row);
    }
    
    ruleCountLabel_->setText(QString("Rules: %1").arg(rules.size()));
}

void AutomationTab::addRuleToTable(const AutomationRule& rule, int row) {
    ruleTable_->setItem(row, COLUMN_ID, new QTableWidgetItem(QString::fromStdString(rule.id)));
    ruleTable_->setItem(row, COLUMN_CONDITION, new QTableWidgetItem(QString::fromStdString(rule.condition)));
    ruleTable_->setItem(row, COLUMN_ACTION, new QTableWidgetItem(QString::fromStdString(rule.action)));
    ruleTable_->setItem(row, COLUMN_ENABLED, new QTableWidgetItem(formatRuleEnabled(rule.isEnabled)));
    ruleTable_->setItem(row, COLUMN_DURATION, new QTableWidgetItem(QString::number(rule.duration.count())));
}

void AutomationTab::clearRuleTable() {
    ruleTable_->setRowCount(0);
}

AutomationRule AutomationTab::getSelectedRule() const {
    QList<QTableWidgetItem*> selectedItems = ruleTable_->selectedItems();
    if (selectedItems.isEmpty()) {
        return AutomationRule{};
    }
    
    int row = selectedItems.first()->row();
    if (row >= 0 && row < static_cast<int>(currentRules_.size())) {
        return currentRules_[row];
    }
    
    return AutomationRule{};
}

bool AutomationTab::hasValidSelection() const {
    return ruleTable_->currentRow() >= 0 && ruleTable_->currentRow() < static_cast<int>(currentRules_.size());
}

void AutomationTab::updateButtonStates() {
    bool hasSelection = hasValidSelection();
    
    editButton_->setEnabled(hasSelection);
    deleteButton_->setEnabled(hasSelection);
    enableButton_->setEnabled(hasSelection);
    disableButton_->setEnabled(hasSelection);
    
    if (hasSelection) {
        AutomationRule rule = getSelectedRule();
        
        // Update button text based on rule state
        if (rule.isEnabled) {
            enableButton_->setEnabled(false);
            disableButton_->setEnabled(true);
        } else {
            enableButton_->setEnabled(true);
            disableButton_->setEnabled(false);
        }
    }
}

void AutomationTab::showRuleOperationResult(const QString& operation, bool success, const QString& message) {
    if (success) {
        statusLabel_->setText(QString("%1 successful").arg(operation));
        statusLabel_->setStyleSheet("color: green;");
    } else {
        statusLabel_->setText(QString("%1 failed: %2").arg(operation, message));
        statusLabel_->setStyleSheet("color: red;");
    }
    
    // Clear status after 5 seconds
    QTimer::singleShot(5000, [this]() {
        statusLabel_->setText("Ready");
        statusLabel_->setStyleSheet("");
    });
}

void AutomationTab::showRuleDialog(AutomationRule* rule) {
    createRuleDialog();
    
    if (rule) {
        // Edit existing rule
        populateDialogFromRule(*rule);
    } else {
        // Add new rule
        resetRuleDialog();
        conditionEdit_->setText("CPU_LOAD > 80%");
        actionEdit_->setText("DISABLE_USB 1234:5678");
        enabledCheckBox_->setChecked(true);
        durationEdit_->setText("10");
    }
    
    // Show dialog
    if (ruleDialog_->exec() == QDialog::Accepted) {
        AutomationRule newRule = createRuleFromDialog();
        
        if (validateRule(newRule)) {
            if (rule) {
                // Update existing rule
                newRule.id = rule->id;
                Command updateCommand = createCommand(CommandType::UPDATE_AUTOMATION_RULE, Module::AUTOMATION);
                updateCommand.parameters["rule_id"] = newRule.id;
                updateCommand.parameters["condition"] = newRule.condition;
                updateCommand.parameters["action"] = newRule.action;
                updateCommand.parameters["enabled"] = newRule.isEnabled ? "true" : "false";
                updateCommand.parameters["duration"] = std::to_string(newRule.duration.count());
                
                // Send update command
                sendCommand(updateCommand, [this](const Response& response) {
                    if (response.status == CommandStatus::SUCCESS) {
                        showStatusMessage("Rule updated successfully");
                        refreshRules();
                    } else {
                        onError(QString("Failed to update rule: %1").arg(QString::fromStdString(response.message)));
                    }
                });
            } else {
                // Add new rule
                Command command = createCommand(CommandType::ADD_AUTOMATION_RULE, Module::AUTOMATION);
                command.parameters["rule_id"] = newRule.id;
                command.parameters["condition"] = newRule.condition;
                command.parameters["action"] = newRule.action;
                command.parameters["enabled"] = newRule.isEnabled ? "true" : "false";
                command.parameters["duration"] = std::to_string(newRule.duration.count());
                
                // Send command
                ipcClient_->sendCommand(command);
            }
        }
    }
}

void AutomationTab::createRuleDialog() {
    if (ruleDialog_) {
        return;
    }
    
    ruleDialog_ = std::make_unique<QDialog>(this);
    ruleDialog_->setWindowTitle("Automation Rule");
    
    dialogLayout_ = std::make_unique<QFormLayout>(ruleDialog_.get());
    
    conditionEdit_ = std::make_unique<QLineEdit>();
    actionEdit_ = std::make_unique<QLineEdit>();
    enabledCheckBox_ = std::make_unique<QCheckBox>();
    durationEdit_ = std::make_unique<QLineEdit>();
    durationLabel_ = std::make_unique<QLabel("Duration (seconds):");
    
    dialogLayout_->addRow("Condition:", conditionEdit_.get());
    dialogLayout_->addRow("Action:", actionEdit_.get());
    dialogLayout_->addRow("Enabled:", enabledCheckBox_.get());
    dialogLayout_->addRow(durationLabel_.get(), durationEdit_.get());
    
    dialogButtons_ = std::make_unique<QDialogButtonBox>(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, ruleDialog_.get());
    
    dialogLayout_->addRow(dialogButtons_.get());
    
    connect(dialogButtons_.get(), &QDialogButtonBox::accepted,
            ruleDialog_.get(), &QDialog::accept);
    connect(dialogButtons_.get(), &QDialogButtonBox::rejected,
            ruleDialog_.get(), &QDialog::reject);
}

void AutomationTab::resetRuleDialog() {
    conditionEdit_->clear();
    actionEdit_->clear();
    enabledCheckBox_->setChecked(false);
    durationEdit_->clear();
}

void AutomationTab::populateDialogFromRule(const AutomationRule& rule) {
    conditionEdit_->setText(QString::fromStdString(rule.condition));
    actionEdit_->setText(QString::fromStdString(rule.action));
    enabledCheckBox_->setChecked(rule.isEnabled);
    durationEdit_->setText(QString::number(rule.duration.count()));
}

AutomationRule AutomationTab::createRuleFromDialog() const {
    AutomationRule rule;
    
    rule.id = generateRuleId().toStdString();
    
    rule.condition = conditionEdit_->text().toStdString();
    rule.action = actionEdit_->text().toStdString();
    rule.isEnabled = enabledCheckBox_->isChecked();
    rule.duration = std::chrono::seconds(durationEdit_->text().toInt());
    
    return rule;
}

bool AutomationTab::validateRule(const AutomationRule& rule) const {
    if (rule.condition.empty()) {
        QMessageBox::warning(this, "Invalid Rule", "Condition cannot be empty.");
        return false;
    }
    
    if (rule.action.empty()) {
        QMessageBox::warning(this, "Invalid Rule", "Action cannot be empty.");
        return false;
    }
    
    if (!isValidCondition(rule.condition)) {
        QMessageBox::warning(this, "Invalid Rule", "Condition format is invalid.");
        return false;
    }
    
    if (!isValidAction(rule.action)) {
        QMessageBox::warning(this, "Invalid Rule", "Action format is invalid.");
        return false;
    }
    
    return true;
}

QString AutomationTab::generateRuleId() const {
    return QString("rule_%1_%2")
        .arg(QDateTime::currentSecsSinceEpoch())
        .arg(QRandomGenerator::global()->bounded(1000));
}

bool AutomationTab::isValidCondition(const std::string& condition) const {
    // Enhanced validation for automation conditions
    std::vector<std::string> validPrefixes = {
        "CPU_LOAD", "MEMORY", "PROCESS", "DEVICE", "NETWORK", "ANDROID"
    };
    
    for (const auto& prefix : validPrefixes) {
        if (condition.find(prefix) == 0) {
            // Check if condition has valid operator
            if (condition.find(">") != std::string::npos ||
                condition.find("<") != std::string::npos ||
                condition.find("=") != std::string::npos) {
                return true;
            }
            
            // Also handle simple conditions like "ANDROID SCREEN OFF"
            if (condition.find("SCREEN OFF") != std::string::npos ||
                condition.find("SCREEN ON") != std::string::npos) {
                return true;
            }
        }
    }
    
    return false;
}

bool AutomationTab::isValidAction(const std::string& action) const {
    // Enhanced validation for automation actions
    std::vector<std::string> validPrefixes = {
        "DISABLE_USB", "ENABLE_USB", "LOCK_ANDROID_DEVICE", "SCREEN_ON_ANDROID", 
        "SCREEN_OFF_ANDROID", "TERMINATE_PROCESS", "KILL_PROCESS",
        "DISABLE_NETWORK", "ENABLE_NETWORK", "TURN_ON_ANDROID", "TURN_OFF_ANDROID"
    };
    
    for (const auto& prefix : validPrefixes) {
        if (action.find(prefix) == 0) {
            return true;
        }
    }
    
    return false;
}

QString AutomationTab::formatRuleEnabled(bool enabled) const {
    return enabled ? "Yes" : "No";
}

} // namespace SysMon
