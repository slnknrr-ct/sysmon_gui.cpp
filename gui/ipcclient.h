#pragma once

#include "../shared/commands.h"
#include "../shared/ipcprotocol.h"
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QHostAddress>
#include <memory>
#include <functional>
#include <queue>
#include <mutex>

QT_BEGIN_NAMESPACE
class QTcpSocket;
QT_END_NAMESPACE

namespace SysMon {

// IPC Client - communicates with the agent
class IpcClient : public QObject {
    Q_OBJECT

public:
    using ResponseHandler = std::function<void(const Response&)>;
    using EventHandler = std::function<void(const Event&)>;
    using ConnectionHandler = std::function<void(bool)>;
    
    IpcClient(QObject* parent = nullptr);
    ~IpcClient();
    
    // Connection management
    bool connectToAgent(const QString& host = "localhost", int port = 12345);
    void disconnectFromAgent();
    bool isConnected() const;
    
    // Command sending
    QString sendCommand(const Command& command, ResponseHandler handler = nullptr);
    QString sendCommandAsync(const Command& command, ResponseHandler handler = nullptr);
    
    // Handler registration
    void setDefaultResponseHandler(ResponseHandler handler);
    void setEventHandler(EventHandler handler);
    void setConnectionHandler(ConnectionHandler handler);
    
    // Status
    QString getConnectionStatus() const;
    QString getLastError() const;

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& error);
    void responseReceived(const Response& response);
    void eventReceived(const Event& event);

private slots:
    // Socket event handlers
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onSocketReadyRead();
    void onConnectionTimer();

private:
    // Message processing
    void processReceivedData();
    void processMessage(const IpcMessage& message);
    
    // Command management
    void sendCommandToSocket(const Command& command);
    void handlePendingCommands();
    
    // Response handling
    void handleResponse(const Response& response);
    void handleEvent(const Event& event);
    
    // Pending command tracking
    struct PendingCommand {
        QString id;
        Command command;
        ResponseHandler handler;
        std::chrono::system_clock::time_point timestamp;
    };
    
    std::queue<PendingCommand> pendingCommands_;
    std::map<QString, PendingCommand> activeCommands_;
    mutable std::mutex commandsMutex_;
    
    // Network components
    std::unique_ptr<QTcpSocket> socket_;
    QString host_;
    int port_;
    
    // Connection management
    std::unique_ptr<QTimer> connectionTimer_;
    bool reconnecting_;
    int reconnectAttempts_;
    
    // Data buffering
    QByteArray receiveBuffer_;
    
    // Handlers
    ResponseHandler defaultResponseHandler_;
    EventHandler eventHandler_;
    ConnectionHandler connectionHandler_;
    
    // Status
    std::atomic<bool> connected_;
    QString lastError_;
    
    // Command ID generation
    QString generateCommandId();
    std::atomic<uint64_t> commandCounter_;
    
    // Constants
    static constexpr int CONNECTION_TIMEOUT = 5000; // 5 seconds
    static constexpr int RECONNECT_INTERVAL = 3000; // 3 seconds
    static constexpr int MAX_RECONNECT_ATTEMPTS = 10;
    static constexpr int COMMAND_TIMEOUT = 10000; // 10 seconds
};

} // namespace SysMon
