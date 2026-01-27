#include "ipcserver.h"
#include "logger.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#endif

namespace SysMon {

IpcServer::IpcServer() 
    : serverSocket_(-1)
    , port_(12345)
    , running_(false)
    , initialized_(false) {
    
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

IpcServer::~IpcServer() {
    shutdown();
    
#ifdef _WIN32
    WSACleanup();
#endif
}

bool IpcServer::initialize(int port) {
    if (initialized_) {
        return true;
    }
    
    port_ = port;
    
    if (!createServerSocket(port)) {
        return false;
    }
    
    initialized_ = true;
    return true;
}

bool IpcServer::start() {
    if (!initialized_) {
        return false;
    }
    
    if (running_) {
        return true;
    }
    
    running_ = true;
    serverThread_ = std::thread(&IpcServer::serverThread, this);
    
    return true;
}

void IpcServer::stop() {
    if (!running_) {
        return;
    }
    
    if (logger_) {
        logger_->info("Stopping IPC server...");
    }
    
    running_ = false;
    
    // Wait for server thread
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
    
    // Close all client connections
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (auto& client : clientSockets_) {
#ifdef _WIN32
            closesocket(client.second);
#else
            close(client.second);
#endif
        }
        clients_.clear();
        clientSockets_.clear();
    }
    
    // Wait for all client threads
    for (auto& thread : clientThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    clientThreads_.clear();
    
    if (logger_) {
        logger_->info("IPC server stopped");
    }
}

void IpcServer::shutdown() {
    stop();
    closeServerSocket();
    initialized_ = false;
}

bool IpcServer::isRunning() const {
    return running_;
}

int IpcServer::getConnectedClientsCount() const {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    return static_cast<int>(clients_.size());
}

std::vector<ClientConnection> IpcServer::getConnectedClients() const {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    std::vector<ClientConnection> result;
    for (const auto& client : clients_) {
        result.push_back(client.second);
    }
    return result;
}

void IpcServer::setCommandHandler(CommandHandler handler) {
    commandHandler_ = handler;
}

void IpcServer::setEventHandler(EventHandler handler) {
    eventHandler_ = handler;
}

void IpcServer::setLogger(Logger* logger) {
    logger_ = logger;
}

void IpcServer::broadcastEvent(const Event& event) {
    std::string eventData = IpcProtocol::serializeEvent(event);
    
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (const auto& client : clientSockets_) {
        sendMessage(client.second, eventData);
    }
}

void IpcServer::sendEventToClient(const std::string& clientId, const Event& event) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    auto it = clientSockets_.find(clientId);
    if (it != clientSockets_.end()) {
        std::string eventData = IpcProtocol::serializeEvent(event);
        sendMessage(it->second, eventData);
    }
}

void IpcServer::sendResponseToClient(const std::string& clientId, const Response& response) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    auto it = clientSockets_.find(clientId);
    if (it != clientSockets_.end()) {
        std::string responseData = IpcProtocol::serializeResponse(response);
        sendMessage(it->second, responseData);
    }
}

void IpcServer::serverThread() {
    if (logger_) {
        logger_->info("IPC server thread started");
    }
    
    while (running_) {
        try {
            acceptConnections();
            cleanupInactiveClients();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->error("Exception in server thread: " + std::string(e.what()));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    if (logger_) {
        logger_->info("IPC server thread stopped");
    }
}

void IpcServer::acceptConnections() {
    if (serverSocket_ == -1) {
        return;
    }
    
    sockaddr_in clientAddr;
#ifdef _WIN32
    int clientAddrLen = sizeof(clientAddr);
#else
    socklen_t clientAddrLen = sizeof(clientAddr);
#endif
    
#ifdef _WIN32
    SOCKET clientSocket = accept(serverSocket_, (sockaddr*)&clientAddr, &clientAddrLen);
#else
    int clientSocket = accept(serverSocket_, (sockaddr*)&clientAddr, &clientAddrLen);
#endif
    
    if (clientSocket == INVALID_SOCKET) {
#ifdef _WIN32
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK && error != WSAEINTR) {
            if (logger_) {
                logger_->error("Accept failed with error: " + std::to_string(error));
            }
        }
#endif
        return;
    }
    
    // Check client limit
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        if (clients_.size() >= MAX_CLIENTS) {
            if (logger_) {
                logger_->warning("Client limit reached, rejecting connection");
            }
#ifdef _WIN32
            closesocket(clientSocket);
#else
            close(clientSocket);
#endif
            return;
        }
    }
    
    // Set client socket to non-blocking
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);
#else
    int flags = fcntl(clientSocket, F_GETFL, 0);
    fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);
#endif
    
    // Add client
    std::string clientAddress = inet_ntoa(clientAddr.sin_addr);
    addClient(clientSocket, clientAddress);
    
    // Start client handler thread
    std::string clientId = std::to_string(clientSocket);
    clientThreads_.emplace_back(&IpcServer::handleClient, this, clientId, clientSocket);
}

void IpcServer::handleClient(const std::string& clientId, int socket) {
    if (logger_) {
        logger_->info("Client handler started for ID: " + clientId);
    }
    
    try {
        while (running_) {
            std::string message = receiveMessage(socket);
            if (message.empty()) {
                break; // Client disconnected
            }
            
            processClientMessage(clientId, message);
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in client handler " + clientId + ": " + std::string(e.what()));
        }
    }
    
    removeClient(clientId);
    
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
    
    if (logger_) {
        logger_->info("Client handler stopped for ID: " + clientId);
    }
}

void IpcServer::processClientMessage(const std::string& clientId, const std::string& message) {
    try {
        // Update client activity
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            auto it = clients_.find(clientId);
            if (it != clients_.end()) {
                it->second.lastActivity = std::chrono::system_clock::now();
            }
        }
        
        auto messageType = IpcProtocol::getMessageType(message);
        
        switch (messageType) {
            case IpcProtocol::MessageType::COMMAND: {
                Command command = IpcProtocol::deserializeCommand(message);
                if (commandHandler_) {
                    Response response = commandHandler_(command);
                    sendResponseToClient(clientId, response);
                } else {
                    // Send error response if no handler set
                    Response errorResponse = createResponse(command.id, CommandStatus::FAILED, 
                        "No command handler configured");
                    sendResponseToClient(clientId, errorResponse);
                }
                break;
            }
            case IpcProtocol::MessageType::RESPONSE:
                // Handle responses if needed
                break;
            case IpcProtocol::MessageType::EVENT:
                // Handle events if needed
                break;
            default: {
                // Send error for unknown message type
                Response errorResponse = createResponse("unknown", CommandStatus::FAILED, 
                    "Unknown message type");
                sendResponseToClient(clientId, errorResponse);
                break;
            }
        }
    } catch (const std::exception& e) {
        // Send error response for parsing errors
        Response errorResponse = createResponse("error", CommandStatus::FAILED, 
            "Message parsing error: " + std::string(e.what()));
        sendResponseToClient(clientId, errorResponse);
    }
}

void IpcServer::addClient(int socket, const std::string& address) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    
    ClientConnection client;
    client.id = std::to_string(socket);
    client.address = address;
    client.connectTime = std::chrono::system_clock::now();
    client.lastActivity = client.connectTime;
    
    clients_[client.id] = client;
    clientSockets_[client.id] = socket;
    
    // Log new connection
    if (logger_) {
        logger_->info("New client connected from " + address + " with ID: " + client.id);
    }
}

void IpcServer::removeClient(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    
    auto it = clients_.find(clientId);
    if (it != clients_.end() && logger_) {
        logger_->info("Client disconnected: " + clientId + " from " + it->second.address);
    }
    
    clients_.erase(clientId);
    clientSockets_.erase(clientId);
}

bool IpcServer::createServerSocket(int port) {
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        if (logger_) {
            logger_->error("Failed to create server socket");
        }
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0) {
        if (logger_) {
            logger_->warning("Failed to set SO_REUSEADDR");
        }
    }
    
    // Set non-blocking mode
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(serverSocket_, FIONBIO, &mode);
#else
    int flags = fcntl(serverSocket_, F_GETFL, 0);
    fcntl(serverSocket_, F_SETFL, flags | O_NONBLOCK);
#endif
    
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    
    if (bind(serverSocket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        if (logger_) {
            logger_->error("Failed to bind server socket to port " + std::to_string(port));
        }
        closeServerSocket();
        return false;
    }
    
    if (listen(serverSocket_, SOMAXCONN) < 0) {
        if (logger_) {
            logger_->error("Failed to listen on server socket");
        }
        closeServerSocket();
        return false;
    }
    
    if (logger_) {
        logger_->info("Server socket created and listening on port " + std::to_string(port));
    }
    
    return true;
}

void IpcServer::closeServerSocket() {
    if (serverSocket_ >= 0) {
#ifdef _WIN32
        closesocket(serverSocket_);
#else
        close(serverSocket_);
#endif
        serverSocket_ = -1;
        
        if (logger_) {
            logger_->info("Server socket closed");
        }
    }
}

bool IpcServer::sendMessage(int socket, const std::string& message) {
    if (socket < 0) {
        return false;
    }
    
    // Send message length first
    uint32_t msgLength = static_cast<uint32_t>(message.length());
    uint32_t sent = 0;
    
    while (sent < sizeof(msgLength)) {
        int result = send(socket, (const char*)&msgLength + sent, sizeof(msgLength) - sent, 0);
        if (result <= 0) {
            return false;
        }
        sent += result;
    }
    
    // Send message content
    sent = 0;
    while (sent < msgLength) {
        int result = send(socket, message.c_str() + sent, msgLength - sent, 0);
        if (result <= 0) {
            return false;
        }
        sent += result;
    }
    
    return true;
}
std::string IpcServer::receiveMessage(int socket) {
    if (socket < 0) {
        return "";
    }
    
    // Receive length first
    uint32_t msgLength;
    int received = recv(socket, (char*)&msgLength, sizeof(msgLength), 0);
    if (received != sizeof(msgLength)) {
        return "";
    }
    
    // Validate message length
    if (msgLength > 1024 * 1024) { // 1MB limit
        if (logger_) {
            logger_->warning("Received message too large: " + std::to_string(msgLength) + " bytes");
        }
        return "";
    }
    
    // Receive message
    std::string message(msgLength, '\0');
    uint32_t totalReceived = 0;
    
    while (totalReceived < msgLength) {
        int result = recv(socket, &message[totalReceived], msgLength - totalReceived, 0);
        if (result <= 0) {
            return "";
        }
        totalReceived += result;
    }
    
    return message;
}

void IpcServer::cleanupInactiveClients() {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    
    auto now = std::chrono::system_clock::now();
    std::vector<std::string> toRemove;
    
    for (const auto& client : clients_) {
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - client.second.lastActivity);
        if (duration > CLIENT_TIMEOUT) {
            toRemove.push_back(client.first);
        }
    }
    
    for (const auto& clientId : toRemove) {
        auto socketIt = clientSockets_.find(clientId);
        if (socketIt != clientSockets_.end()) {
#ifdef _WIN32
            closesocket(socketIt->second);
#else
            close(socketIt->second);
#endif
            clientSockets_.erase(socketIt);
        }
        clients_.erase(clientId);
        
        if (logger_) {
            logger_->info("Removed inactive client: " + clientId);
        }
    }
}

} // namespace SysMon
