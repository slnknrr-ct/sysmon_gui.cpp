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
    , port_(12345)
    , reconnecting_(false)
    , reconnectAttempts_(0)
    , connected_(false)
    , commandCounter_(0) {
    
    socket_ = std::make_unique<QTcpSocket>(this);
    
    // Connect socket signals
    connect(socket_.get(), &QTcpSocket::connected,
            this, &IpcClient::onSocketConnected);
    connect(socket_.get(), &QTcpSocket::disconnected,
            this, &IpcClient::onSocketDisconnected);
    connect(socket_.get(), QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &IpcClient::onSocketError);
    connect(socket_.get(), &QTcpSocket::readyRead,
            this, &IpcClient::onSocketReadyRead);
    
    // Set up connection timer
    connectionTimer_ = std::make_unique<QTimer>(this);
    connect(connectionTimer_.get(), &QTimer::timeout,
            this, &IpcClient::onConnectionTimer);
}

IpcClient::~IpcClient() {
    disconnectFromAgent();
}

bool IpcClient::connectToAgent(const QString& host, int port) {
    if (connected_) {
        return true;
    }
    
    host_ = host;
    port_ = port;
    
    socket_->connectToHost(host, port);
    
    // Wait for connection (with timeout)
    if (!socket_->waitForConnected(CONNECTION_TIMEOUT)) {
        lastError_ = socket_->errorString();
        return false;
    }
    
    return true;
}

void IpcClient::disconnectFromAgent() {
    if (socket_ && socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->disconnectFromHost();
        if (!socket_->waitForDisconnected(1000)) {
            socket_->abort();
        }
    }
    
    connected_ = false;
    reconnecting_ = false;
    reconnectAttempts_ = 0;
    
    if (connectionTimer_) {
        connectionTimer_->stop();
    }
}

bool IpcClient::isConnected() const {
    return connected_;
}

QString IpcClient::sendCommand(const Command& command, ResponseHandler handler) {
    QString commandId = generateCommandId();
    
    // Create a copy of the command with the new ID
    Command cmdWithId = command;
    cmdWithId.id = commandId.toStdString();
    
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

QString IpcClient::sendCommandAsync(const Command& command, ResponseHandler handler) {
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

QString IpcClient::getConnectionStatus() const {
    if (connected_) {
        return "Connected to " + host_ + ":" + QString::number(port_);
    } else {
        return "Disconnected";
    }
}

QString IpcClient::getLastError() const {
    return lastError_;
}

void IpcClient::onSocketConnected() {
    connected_ = true;
    lastError_.clear();
    reconnectAttempts_ = 0;
    reconnecting_ = false;
    
    // Stop reconnection timer
    if (connectionTimer_) {
        connectionTimer_->stop();
    }
    
    // Send pending commands
    handlePendingCommands();
    
    emit connected();
    
    if (connectionHandler_) {
        connectionHandler_(true);
    }
}

void IpcClient::onSocketDisconnected() {
    connected_ = false;
    lastError_ = "Connection lost";
    
    emit disconnected();
    
    if (connectionHandler_) {
        connectionHandler_(false);
    }
    
    // Start reconnection if not manually disconnected
    if (!reconnecting_) {
        startReconnection();
    }
}

void IpcClient::onSocketError(QAbstractSocket::SocketError error) {
    lastError_ = socket_->errorString();
    
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
            socket_->connectToHost(host_, port_);
        }
        
        emit errorOccurred(QString("Reconnection attempt %1/%2").arg(reconnectAttempts_).arg(MAX_RECONNECT_ATTEMPTS));
    } else if (reconnectAttempts_ >= MAX_RECONNECT_ATTEMPTS) {
        connectionTimer_->stop();
        emit errorOccurred("Maximum reconnection attempts reached");
    }
}

void IpcClient::processReceivedData() {
    // This method is called when data is received
    // Actual processing is done in onSocketReadyRead
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
    if (!connected_ || !socket_) {
        return;
    }
    
    std::string jsonStr = IpcProtocol::serializeCommand(command);
    
    // Send length first
    uint32_t msgLength = static_cast<uint32_t>(jsonStr.length());
    socket_->write(reinterpret_cast<const char*>(&msgLength), sizeof(msgLength));
    
    // Send message
    socket_->write(jsonStr.c_str(), jsonStr.length());
}

void IpcClient::handlePendingCommands() {
    std::lock_guard<std::mutex> lock(commandsMutex_);
    
    while (!pendingCommands_.empty()) {
        PendingCommand pending = pendingCommands_.front();
        pendingCommands_.pop();
        
        sendCommandToSocket(pending.command);
    }
}

void IpcClient::handleResponse(const Response& response) {
    QString commandId = QString::fromStdString(response.commandId);
    
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

QString IpcClient::generateCommandId() {
    return QString::number(++commandCounter_) + "_" + QString::number(std::chrono::system_clock::now().time_since_epoch().count());
}

} // namespace SysMon

#include "ipcclient.moc"
