#pragma once

#include "../shared/systemtypes.h"
#include "../shared/commands.h"
#include "../shared/ipcprotocol.h"
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include <vector>
#include <map>

#ifdef _WIN32
#include <winsock2.h>
#pragma push_macro("WIN32_LEAN_AND_MEAN")
#define WIN32_LEAN_AND_MEAN
#pragma pop_macro("WIN32_LEAN_AND_MEAN")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#endif

namespace SysMon {

// Forward declarations
class Logger;

// Client connection representation
struct ClientConnection {
    std::string id;
    std::string address;
    std::chrono::system_clock::time_point connectTime;
    std::chrono::system_clock::time_point lastActivity;
};

// IPC Server - handles communication with GUI clients
class IpcServer {
public:
    using CommandHandler = std::function<Response(const Command&)>;
    using EventHandler = std::function<void(const Event&)>;
    
    // Server lifecycle
    IpcServer();
    ~IpcServer();
    
    // Server lifecycle
    bool initialize(int port = 12345);
    bool start();
    void stop();
    void shutdown();
    
    // Command handling
    void setCommandHandler(CommandHandler handler);
    void setEventHandler(EventHandler handler);
    void setLogger(Logger* logger);
    
    // Status
    bool isRunning() const;
    int getConnectedClientsCount() const;
    std::vector<ClientConnection> getConnectedClients() const;
    
    // Event broadcasting
    void broadcastEvent(const Event& event);
    void sendEventToClient(const std::string& clientId, const Event& event);
    void sendResponseToClient(const std::string& clientId, const Response& response);

private:
    // Server implementation
    void serverThread();
    void acceptConnections();
    void handleClient(const std::string& clientId, int socket);
    
    // Client management
    void addClient(int socket, const std::string& address);
    void removeClient(const std::string& clientId);
    void cleanupInactiveClients();
    
    // Message handling
    void processClientMessage(const std::string& clientId, const std::string& message);
    
    // Network helpers
    bool createServerSocket(int port);
    void closeServerSocket();
    bool sendMessage(int socket, const std::string& message);
    std::string receiveMessage(int socket);
    
    // Server state
    int serverSocket_;
    int port_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    
    // Thread management
    std::thread serverThread_;
    std::vector<std::thread> clientThreads_;
    
    // Client management
    std::map<std::string, ClientConnection> clients_;
    std::map<std::string, int> clientSockets_;
    mutable std::mutex clientsMutex_;
    
    // Handlers
    CommandHandler commandHandler_;
    EventHandler eventHandler_;
    Logger* logger_;
    
    // Constants
    static constexpr int MAX_CLIENTS = 10;
    static constexpr int BUFFER_SIZE = 4096;
    static constexpr std::chrono::seconds CLIENT_TIMEOUT{300}; // 5 minutes
};

} // namespace SysMon
