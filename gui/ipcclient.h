#pragma once

#include "../shared/commands.h"
#include "../shared/ipcprotocol.h"
#include "../shared/security.h"
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
    bool connectToAgent(const std::string& host = "localhost", int port = 8081);
    void disconnectFromAgent();
    bool isConnected() const;
    
    // Authentication
    void authenticate();
    bool isAuthenticated() const;
    
    // Command sending
    std::string sendCommand(const Command& command, ResponseHandler handler = nullptr);
    std::string sendCommandAsync(const Command& command, ResponseHandler handler = nullptr);
    
    // Handler registration
    void setDefaultResponseHandler(ResponseHandler handler);
    void setEventHandler(EventHandler handler);
    void setConnectionHandler(ConnectionHandler handler);
    
    // Status
    std::string getConnectionStatus() const;
    std::string getLastError() const;

signals:
    void connected();
    void disconnected();
    void errorOccurred(const std::string& error);
    void responseReceived(const Response& response);
    void eventReceived(const Event& event);
    void authenticationRequired();
    void authenticationSuccess();
    void authenticationFailed(const std::string& reason);

private slots:
    // Socket event handlers
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onSocketReadyRead();
    void onConnectionTimer();
    
    // Connection management
    void startReconnection();

private:
    // Message processing
    void processReceivedData(const std::string& data);
    void processMessage(const IpcMessage& message);
    
    // Command management
    void sendCommandToSocket(const Command& command);
    void handlePendingCommands();
    
    // Authentication
    void sendAuthenticationRequest();
    void handleAuthenticationResponse(const Response& response);
    
    // Response handling
    void handleResponse(const Response& response);
    void handleEvent(const Event& event);
    
    // Pending command tracking
    struct PendingCommand {
        std::string id;
        Command command;
        ResponseHandler handler;
        std::chrono::system_clock::time_point timestamp;
    };
    
    std::queue<PendingCommand> pendingCommands_;
    std::map<std::string, PendingCommand> activeCommands_;
    mutable std::mutex commandsMutex_;
    
    // Network components
    std::unique_ptr<QTcpSocket> socket_;
    std::string host_;
    int port_;
    
    // Authentication
    bool authenticated_;
    std::string authToken_;
    Security::SecurityManager* securityManager_;
    
    // Timers
    QTimer* connectionTimer_;
    QTimer* authTimer_;
    QTimer* heartbeatTimer_;
    
    // Connection management
    bool reconnecting_;
    int reconnectAttempts_;
    
    // Data buffering
    QByteArray receiveBuffer_;
    
    // Handlers
    ResponseHandler defaultResponseHandler_;
    EventHandler eventHandler_;
    ConnectionHandler connectionHandler_;
    
    // Status
    std::string lastError_;
    mutable std::mutex statusMutex_;
    std::atomic<bool> connected_;
    
    // Command ID generation
    std::string generateCommandId();
    std::atomic<uint64_t> commandCounter_;
    
    // Constants
    static constexpr int CONNECTION_TIMEOUT = 15000; // 15 seconds
    static constexpr int RECONNECT_INTERVAL = 3000; // 3 seconds
    static constexpr int MAX_RECONNECT_ATTEMPTS = 10;
    static constexpr int COMMAND_TIMEOUT = 10000; // 10 seconds
    static constexpr int RECONNECT_DELAY = 2000; // 2 seconds
    static constexpr int HEARTBEAT_INTERVAL = 30000; // 30 seconds
    static constexpr int AUTH_TIMEOUT = 10000; // 10 seconds
};

} // namespace SysMon
