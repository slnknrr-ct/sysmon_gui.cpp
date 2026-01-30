#include "ipcclient.h"
#include "../shared/ipcprotocol.h"
#include <QTcpSocket>
#include <QTimer>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace SysMon {

IpcClient::IpcClient(QObject* parent)
    : QObject(parent)
    , socket_(nullptr)
    , port_(8081)
    , reconnecting_(false)
    , reconnectAttempts_(0)
    , connected_(false)
    , commandCounter_(0)
    , authTimer_(nullptr)
    , heartbeatTimer_(nullptr) {
    
    socket_ = std::make_unique<QTcpSocket>(this);
    
    // Connect socket signals
    connect(socket_.get(), &QTcpSocket::connected,
            this, &IpcClient::onSocketConnected);
    connect(socket_.get(), &QTcpSocket::disconnected,
            this, &IpcClient::onSocketDisconnected);
    connect(socket_.get(), &QAbstractSocket::errorOccurred,
            this, &IpcClient::onSocketError);
    connect(socket_.get(), &QTcpSocket::readyRead,
            this, &IpcClient::onSocketReadyRead);
    
    // Set up connection timer
    connectionTimer_ = new QTimer(this);
    connect(connectionTimer_, &QTimer::timeout,
            this, &IpcClient::onConnectionTimer);
    
    // Set up heartbeat timer
    heartbeatTimer_ = new QTimer(this);
    connect(heartbeatTimer_, &QTimer::timeout,
            this, &IpcClient::onHeartbeatTimer);
}

IpcClient::~IpcClient() {
    disconnectFromAgent();
}

bool IpcClient::connectToAgent(const std::string& host, int port) {
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        qDebug() << "Socket already connecting or connected, disconnecting first...";
        socket_->disconnectFromHost();
        socket_->waitForDisconnected(1000);
    }
    
    host_ = host;
    port_ = port;
    
    // Debug output
    qDebug() << "Attempting to connect to" << QString::fromStdString(host) << "port" << port;
    
    socket_->connectToHost(QString::fromStdString(host), port);
    
    // Wait for connection (with timeout)
    if (!socket_->waitForConnected(CONNECTION_TIMEOUT)) {
        lastError_ = socket_->errorString().toStdString();
        
        qDebug() << "Connection failed:" << socket_->errorString();
        
        // Add detailed error information
        QString detailedError = "Failed to connect to " + QString::fromStdString(host) + 
                               ":" + QString::number(port) + " - " + socket_->errorString();
        
        switch (socket_->error()) {
            case QAbstractSocket::ConnectionRefusedError:
                detailedError += " (Connection refused - agent may not be running)";
                break;
            case QAbstractSocket::HostNotFoundError:
                detailedError += " (Host not found)";
                break;
            case QAbstractSocket::NetworkError:
                detailedError += " (Network error - check network connectivity)";
                break;
            case QAbstractSocket::SocketTimeoutError:
                detailedError += " (Connection timeout - agent may be busy)";
                break;
            default:
                detailedError += " (Error code: " + QString::number(socket_->error()) + ")";
                break;
        }
        
        lastError_ = detailedError.toStdString();
        return false;
    }
    
    qDebug() << "Socket connection established, waiting for authentication...";
    return true;
}

void IpcClient::disconnectFromAgent() {
    // Stop all timers
    if (connectionTimer_) {
        connectionTimer_->stop();
    }
    if (authTimer_) {
        authTimer_->stop();
        authTimer_->deleteLater();
        authTimer_ = nullptr;
    }
    if (heartbeatTimer_) {
        heartbeatTimer_->stop();
    }
    
    reconnecting_ = false;
    reconnectAttempts_ = 0;
    
    if (socket_ && socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->disconnectFromHost();
        if (!socket_->waitForDisconnected(1000)) {
            socket_->abort();
        }
    }
    
    connected_ = false;
}

bool IpcClient::isConnected() const {
    return connected_;
}

std::string IpcClient::sendCommand(const Command& command, ResponseHandler handler) {
    std::string commandId = generateCommandId();
    
    // Create a copy of the command with the new ID
    Command cmdWithId = command;
    cmdWithId.id = commandId;
    
    // Store pending command
    PendingCommand pending;
    pending.id = commandId;
    pending.command = cmdWithId;
    pending.handler = handler;
    pending.timestamp = std::chrono::system_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(commandsMutex_);
        pendingCommands_.push(pending);
        activeCommands_[commandId] = pending;
    }
    
    // Send command if connected
    if (connected_) {
        sendCommandToSocket(cmdWithId);
    }
    
    return commandId;
}

std::string IpcClient::sendCommandAsync(const Command& command, ResponseHandler handler) {
    return sendCommand(command, handler);
}

void IpcClient::setDefaultResponseHandler(ResponseHandler handler) {
    defaultResponseHandler_ = handler;
}

void IpcClient::setEventHandler(EventHandler handler) {
    eventHandler_ = handler;
}

void IpcClient::setConnectionHandler(ConnectionHandler handler) {
    connectionHandler_ = handler;
}

std::string IpcClient::getConnectionStatus() const {
    if (connected_) {
        return "Connected to " + host_ + ":" + std::to_string(port_);
    } else {
        return "Disconnected";
    }
}

std::string IpcClient::getLastError() const {
    return lastError_;
}

void IpcClient::onSocketConnected() {
    qDebug() << "Socket connected! Sending authentication...";
    
    // Don't set connected_ = true yet - wait for authentication
    lastError_.clear();
    reconnectAttempts_ = 0;
    reconnecting_ = false;
    
    // Stop reconnection timer
    if (connectionTimer_) {
        connectionTimer_->stop();
    }
    
    // Send authentication command first
    sendAuthenticationRequest();
}

void IpcClient::onSocketDisconnected() {
    bool wasConnected = connected_;
    connected_ = false;
    
    qDebug() << "=== SOCKET DISCONNECTED ===";
    qDebug() << "Was connected:" << wasConnected;
    qDebug() << "Socket error:" << (socket_ ? socket_->errorString() : "No socket");
    qDebug() << "Socket state:" << (socket_ ? socket_->state() : -1);
    
    // Stop heartbeat timer
    if (heartbeatTimer_) {
        heartbeatTimer_->stop();
        qDebug() << "Heartbeat timer stopped";
    }
    
    emit disconnected();
    
    if (connectionHandler_) {
        connectionHandler_(false);
    }
    
    // Only start reconnection if:
    // 1. We were previously fully connected (not just during authentication)
    // 2. We're not manually disconnecting
    // 3. We're not already reconnecting
    if (wasConnected && !reconnecting_) {
        qDebug() << "Connection lost, starting reconnection...";
        startReconnection();
    } else {
        qDebug() << "Socket disconnected during authentication or manual disconnect, not reconnecting";
    }
    
    qDebug() << "=== SOCKET DISCONNECTED HANDLED ===";
}

void IpcClient::onSocketError(QAbstractSocket::SocketError error) {
    lastError_ = socket_->errorString().toStdString();
    
    if (error != QAbstractSocket::RemoteHostClosedError) {
        emit errorOccurred(lastError_);
    }
}

void IpcClient::onSocketReadyRead() {
    receiveBuffer_.append(socket_->readAll());
    
    // Process complete messages
    while (receiveBuffer_.size() >= sizeof(uint32_t)) {
        // Read message length
        uint32_t msgLength = *reinterpret_cast<const uint32_t*>(receiveBuffer_.constData());
        
        if (receiveBuffer_.size() >= sizeof(uint32_t) + msgLength) {
            // Extract complete message
            QByteArray messageData = receiveBuffer_.mid(sizeof(uint32_t), msgLength);
            receiveBuffer_.remove(0, sizeof(uint32_t) + msgLength);
            
            // Process message
            std::string messageStr(messageData.constData(), messageData.size());
            processReceivedData(messageStr);
        } else {
            // Incomplete message, wait for more data
            break;
        }
    }
}

void IpcClient::onConnectionTimer() {
    if (!connected_ && reconnectAttempts_ < MAX_RECONNECT_ATTEMPTS) {
        reconnectAttempts_++;
        
        if (socket_->state() == QAbstractSocket::UnconnectedState) {
            socket_->connectToHost(QString::fromStdString(host_), port_);
        }
        
        emit errorOccurred("Reconnection attempt " + std::to_string(reconnectAttempts_) + "/" + std::to_string(MAX_RECONNECT_ATTEMPTS));
    } else if (reconnectAttempts_ >= MAX_RECONNECT_ATTEMPTS) {
        connectionTimer_->stop();
        emit errorOccurred("Maximum reconnection attempts reached");
    }
}

void IpcClient::onAuthenticationTimeout() {
    qDebug() << "=== AUTHENTICATION TIMEOUT ===";
    
    connected_ = false;
    lastError_ = "Authentication timeout";
    
    emit errorOccurred("Authentication timeout - no response from agent");
    
    // Close the socket
    if (socket_) {
        socket_->disconnectFromHost();
    }
    
    // Clean up auth timer
    if (authTimer_) {
        authTimer_->deleteLater();
        authTimer_ = nullptr;
    }
    
    qDebug() << "=== AUTHENTICATION TIMEOUT HANDLED ===";
}

void IpcClient::onHeartbeatTimer() {
    if (!connected_ || !socket_) {
        qDebug() << "Heartbeat: Not connected, stopping timer";
        heartbeatTimer_->stop();
        return;
    }
    
    // Send heartbeat ping
    Command heartbeatCmd;
    heartbeatCmd.type = CommandType::PING;
    heartbeatCmd.id = generateCommandId();
    heartbeatCmd.parameters["heartbeat"] = "true";
    
    qDebug() << "Sending heartbeat to agent";
    sendCommandToSocket(heartbeatCmd);
}

void IpcClient::processReceivedData(const std::string& data) {
    // Parse message from data
    try {
        IpcProtocol::MessageType msgType = IpcProtocol::getMessageType(data);
        
        switch (msgType) {
            case IpcProtocol::MessageType::RESPONSE: {
                Response response = IpcProtocol::deserializeResponse(data);
                handleResponse(response);
                break;
            }
            case IpcProtocol::MessageType::EVENT: {
                Event event = IpcProtocol::deserializeEvent(data);
                handleEvent(event);
                break;
            }
            default:
                emit errorOccurred("Unknown message type received");
                break;
        }
    } catch (const std::exception& e) {
        emit errorOccurred("Failed to parse message: " + std::string(e.what()));
    }
}

void IpcClient::processMessage(const IpcMessage& message) {
    switch (message.getType()) {
        case IpcMessage::Type::RESPONSE: {
            const Response* response = message.asResponse();
            if (response) {
                handleResponse(*response);
            }
            break;
        }
        case IpcMessage::Type::EVENT: {
            const Event* event = message.asEvent();
            if (event) {
                handleEvent(*event);
            }
            break;
        }
        default:
            break;
    }
}

void IpcClient::sendCommandToSocket(const Command& command) {
    if (!socket_) {
        return;
    }
    
    // For authentication (PING with auth_token), we allow sending even if not fully connected yet
    // For other commands, we require full connection
    if (!connected_ && !(command.type == CommandType::PING && command.parameters.find("auth_token") != command.parameters.end())) {
        qDebug() << "Command blocked - not connected and not authentication request";
        return;
    }
    
    std::string jsonStr = IpcProtocol::serializeCommand(command);
    
    // Send length first
    uint32_t msgLength = static_cast<uint32_t>(jsonStr.length());
    socket_->write(reinterpret_cast<const char*>(&msgLength), sizeof(msgLength));
    
    // Send message
    socket_->write(jsonStr.c_str(), jsonStr.length());
    
    qDebug() << "Command sent to socket:" << QString::fromStdString(jsonStr);
}

void IpcClient::handlePendingCommands() {
    std::lock_guard<std::mutex> lock(commandsMutex_);
    
    // Only send commands if authenticated
    if (!connected_) {
        return; // Commands will be sent after successful authentication
    }
    
    while (!pendingCommands_.empty()) {
        PendingCommand pending = pendingCommands_.front();
        pendingCommands_.pop();
        
        sendCommandToSocket(pending.command);
    }
}

void IpcClient::handleResponse(const Response& response) {
    std::string commandId = response.commandId;
    
    // Check if this is an authentication response when not authenticated yet
    if (!connected_) {
        handleAuthenticationResponse(response);
        return;
    }
    
    // Find and remove from active commands
    std::lock_guard<std::mutex> lock(commandsMutex_);
    auto it = activeCommands_.find(commandId);
    if (it != activeCommands_.end()) {
        PendingCommand pending = it->second;
        activeCommands_.erase(it);
        
        // Call handler if available
        if (pending.handler) {
            pending.handler(response);
        } else if (defaultResponseHandler_) {
            defaultResponseHandler_(response);
        }
    }
    
    emit responseReceived(response);
}

void IpcClient::handleEvent(const Event& event) {
    if (eventHandler_) {
        eventHandler_(event);
    }
    
    emit eventReceived(event);
}

void IpcClient::startReconnection() {
    reconnecting_ = true;
    reconnectAttempts_ = 0;
    
    if (connectionTimer_) {
        connectionTimer_->start(RECONNECT_INTERVAL);
    }
}

std::string IpcClient::generateCommandId() {
    return std::to_string(++commandCounter_) + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
}

void IpcClient::sendAuthenticationRequest() {
    qDebug() << "=== SENDING AUTHENTICATION REQUEST ===";
    qDebug() << "Socket state:" << socket_->state();
    qDebug() << "Connected status:" << connected_;
    
    // Set up authentication timeout
    authTimer_ = new QTimer(this);
    authTimer_->setSingleShot(true);
    connect(authTimer_, &QTimer::timeout, this, &IpcClient::onAuthenticationTimeout);
    authTimer_->start(AUTH_TIMEOUT);
    
    Command authCommand;
    authCommand.type = CommandType::PING;
    authCommand.id = generateCommandId();
    authCommand.parameters["auth_token"] = "gui_client_token"; // Simple token for now
    
    qDebug() << "Auth command ID:" << QString::fromStdString(authCommand.id);
    qDebug() << "Auth command token:" << QString::fromStdString(authCommand.parameters["auth_token"]);
    
    sendCommandToSocket(authCommand);
    qDebug() << "=== AUTHENTICATION REQUEST SENT ===";
}

void IpcClient::handleAuthenticationResponse(const Response& response) {
    qDebug() << "=== RECEIVED AUTHENTICATION RESPONSE ===";
    qDebug() << "Response ID:" << QString::fromStdString(response.commandId);
    qDebug() << "Response status:" << static_cast<int>(response.status);
    qDebug() << "Response message:" << QString::fromStdString(response.message);
    qDebug() << "Current connected status:" << connected_;
    qDebug() << "Socket state:" << (socket_ ? socket_->state() : -1);
    
    // Stop authentication timeout
    if (authTimer_) {
        authTimer_->stop();
        authTimer_->deleteLater();
        authTimer_ = nullptr;
    }
    
    if (response.status == CommandStatus::SUCCESS) {
        // Authentication successful
        qDebug() << "✓ Authentication successful!";
        connected_ = true;
        
        // Start heartbeat timer
        if (heartbeatTimer_) {
            heartbeatTimer_->start(HEARTBEAT_INTERVAL);
            qDebug() << "Heartbeat timer started with interval:" << HEARTBEAT_INTERVAL << "ms";
        } else {
            qDebug() << "ERROR: heartbeatTimer_ is null!";
        }
        
        // Now send pending commands
        handlePendingCommands();
        
        qDebug() << "Emitting connected() signal";
        emit connected();
        
        if (connectionHandler_) {
            qDebug() << "Calling connection handler";
            connectionHandler_(true);
        } else {
            qDebug() << "No connection handler set";
        }
        
        qDebug() << "=== AUTHENTICATION SUCCESSFUL ===";
    } else {
        // Authentication failed
        qDebug() << "✗ Authentication failed:" << QString::fromStdString(response.message);
        connected_ = false;
        lastError_ = response.message;
        
        emit errorOccurred("Authentication failed: " + response.message);
        
        // Don't immediately reconnect on authentication failure
        // Let the user manually reconnect or wait for auto-reconnect with delay
        QTimer::singleShot(5000, this, [this]() {
            if (!connected_ && socket_->state() == QAbstractSocket::UnconnectedState) {
                qDebug() << "Attempting reconnection after authentication failure...";
                startReconnection();
            }
        });
        
        qDebug() << "=== AUTHENTICATION FAILED ===";
    }
}

} // namespace SysMon
