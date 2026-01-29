#include "ipcserver.h"
#include "logger.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <future>
#include <atomic>
#include <cerrno>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#endif

namespace SysMon {

IpcServer::IpcServer() 
    : serverSocket_(-1)
    , port_(Constants::DEFAULT_IPC_PORT)
    , running_(false)
    , initialized_(false)
    , shuttingDown_(false)
    , securityManager_(&Security::SecurityManager::getInstance()) {
    
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
    
    // Configure security manager
    securityManager_->setMaxMessageSize(Constants::MAX_MESSAGE_SIZE);
    securityManager_->setRateLimit(Constants::MAX_REQUESTS_PER_MINUTE, Constants::RATE_LIMIT_WINDOW);
    
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
        logger_->info("Starting graceful shutdown of IPC server...");
    }
    
    // Signal shutdown to all threads
    shuttingDown_ = true;
    running_ = false;
    
    // Close server socket first to stop accepting new connections
    closeServerSocket();
    
    // Close all client sockets to unblock recv() calls
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        if (logger_) {
            logger_->info("Closing " + std::to_string(clientSockets_.size()) + " client connections...");
        }
        
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
    
    // Wait for client threads to finish with timeout
    if (logger_) {
        logger_->info("Waiting for client threads to finish...");
    }
    
    auto startTime = std::chrono::steady_clock::now();
    for (auto& thread : clientThreads_) {
        if (thread.joinable()) {
            // Check remaining timeout
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            auto remaining = SHUTDOWN_TIMEOUT - elapsed;
            
            if (remaining.count() > 0) {
                // Wait with timeout using future
                auto future = std::async(std::launch::async, [&thread]() {
                    thread.join();
                });
                
                if (future.wait_for(remaining) == std::future_status::timeout) {
                    if (logger_) {
                        logger_->warning("Client thread did not finish within timeout, forcing continuation");
                    }
                    // Thread is stuck, but we continue with shutdown
                    break;
                }
            } else {
                if (logger_) {
                    logger_->warning("Shutdown timeout exceeded, forcing continuation");
                }
                break;
            }
        }
    }
    
    // Clear client threads vector
    clientThreads_.clear();
    
    // Wait for server thread
    if (serverThread_.joinable()) {
        if (logger_) {
            logger_->info("Waiting for server thread to finish...");
        }
        
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        auto remaining = SHUTDOWN_TIMEOUT - elapsed;
        
        if (remaining.count() > 0) {
            auto future = std::async(std::launch::async, [this]() {
                serverThread_.join();
            });
            
            if (future.wait_for(remaining) == std::future_status::timeout) {
                if (logger_) {
                    logger_->warning("Server thread did not finish within timeout");
                }
            }
        }
    }
    
    if (logger_) {
        logger_->info("IPC server graceful shutdown completed");
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
    
    while (running_ && !shuttingDown_) {
        try {
            acceptConnections();
            cleanupInactiveClients();
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Уменьшили с 100мс до 10мс
        } catch (const std::exception& e) {
            if (logger_) {
                logger_->error("Exception in server loop: " + std::string(e.what()));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    if (logger_) {
        logger_->info("IPC server thread stopped");
    }
}

void IpcServer::acceptConnections() {
    if (serverSocket_ == -1 || shuttingDown_) {
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
    
    // Log new connection
    if (logger_) {
        logger_->info("New client connected from: " + std::string(inet_ntoa(clientAddr.sin_addr)) + 
                     " on socket " + std::to_string(clientSocket));
    }
    
    std::cout << "New client connected from " << inet_ntoa(clientAddr.sin_addr) 
              << " on socket " << clientSocket << std::endl;
    
    // Check client limit
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        if (clients_.size() >= Constants::MAX_CLIENTS) {
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
        while (running_ && !shuttingDown_) {
            std::string message = receiveMessage(socket);
            if (message.empty()) {
                if (shuttingDown_) {
                    if (logger_) {
                        logger_->info("Client handler " + clientId + " exiting due to shutdown");
                    }
                } else {
                    if (logger_) {
                        logger_->info("Client " + clientId + " disconnected");
                    }
                }
                break; // Client disconnected or shutting down
            }
            
            processClientMessage(clientId, message);
        }
    } catch (const std::exception& e) {
        if (logger_) {
            logger_->error("Exception in client handler " + clientId + ": " + std::string(e.what()));
        }
    }
    
    // Remove client from tracking
    removeClient(clientId);
    
    // Close socket if not already closed
    if (socket >= 0) {
#ifdef _WIN32
        closesocket(socket);
#else
        close(socket);
#endif
    }
    
    if (logger_) {
        logger_->info("Client handler for ID: " + clientId + " finished");
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
        
        // Validate message size and format
        if (!securityManager_->validateCommand(message)) {
            if (logger_) {
                logger_->warning("Invalid message format or size from client " + clientId);
            }
            Response errorResponse = createResponse("invalid", CommandStatus::FAILED, 
                "Invalid message format or size");
            sendResponseToClient(clientId, errorResponse);
            return;
        }
        
        // Check rate limiting
        if (isRateLimited(clientId)) {
            if (logger_) {
                logger_->warning("Rate limit exceeded for client " + clientId);
            }
            Response errorResponse = createResponse("rate_limited", CommandStatus::FAILED, 
                "Rate limit exceeded");
            sendResponseToClient(clientId, errorResponse);
            return;
        }
        
        // Check authentication
        if (!isClientAuthenticated(clientId)) {
            handleAuthentication(clientId, message);
            return;
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

bool IpcServer::authenticateClient(const std::string& clientId, const std::string& token) {
    return securityManager_->authenticateClient(clientId, token);
}

bool IpcServer::isClientAuthenticated(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    auto it = clients_.find(clientId);
    return it != clients_.end() && it->second.isAuthenticated;
}

bool IpcServer::isRateLimited(const std::string& clientId) {
    return securityManager_->isRateLimited(clientId);
}

void IpcServer::handleAuthentication(const std::string& clientId, const std::string& message) {
    std::cout << "Handling authentication for client " << clientId << std::endl;
    
    try {
        // Try to parse as authentication request
        auto messageType = IpcProtocol::getMessageType(message);
        
        if (messageType == IpcProtocol::MessageType::COMMAND) {
            Command command = IpcProtocol::deserializeCommand(message);
            
            // Check if this is an authentication command
            if (command.type == CommandType::PING && command.parameters.find("auth_token") != command.parameters.end()) {
                std::string token = command.parameters.at("auth_token");
                std::cout << "Received authentication token: " << token << " from client " << clientId << std::endl;
                
                std::lock_guard<std::mutex> lock(clientsMutex_);
                auto it = clients_.find(clientId);
                if (it != clients_.end()) {
                    auto& client = it->second;
                    
                    // Check if client is locked out
                    auto now = std::chrono::system_clock::now();
                    if (client.lockoutUntil > now) {
                        Response errorResponse = createResponse(command.id, CommandStatus::FAILED, 
                            "Account locked out. Try again later.");
                        sendResponseToClient(clientId, errorResponse);
                        return;
                    }
                    
                    if (authenticateClient(clientId, token)) {
                        std::cout << "Authentication successful for client " << clientId << std::endl;
                        client.isAuthenticated = true;
                        client.failedAuthAttempts = 0;
                        
                        if (logger_) {
                            logger_->info("Client " + clientId + " authenticated successfully");
                        }
                        
                        Response successResponse = createResponse(command.id, CommandStatus::SUCCESS, 
                            "Authentication successful");
                        sendResponseToClient(clientId, successResponse);
                    } else {
                        std::cout << "Authentication failed for client " << clientId << std::endl;
                        client.failedAuthAttempts++;
                        
                        if (client.failedAuthAttempts >= Constants::MAX_LOGIN_ATTEMPTS) {
                            client.lockoutUntil = now + Constants::LOCKOUT_DURATION;
                            
                            if (logger_) {
                                logger_->warning("Client " + clientId + " locked out due to too many failed attempts");
                            }
                            
                            Response errorResponse = createResponse(command.id, CommandStatus::FAILED, 
                                "Account locked out due to too many failed attempts");
                            sendResponseToClient(clientId, errorResponse);
                        } else {
                            Response errorResponse = createResponse(command.id, CommandStatus::FAILED, 
                                "Invalid authentication token");
                            sendResponseToClient(clientId, errorResponse);
                        }
                    }
                }
            } else {
                std::cout << "Non-authentication command received from unauthenticated client " << clientId << std::endl;
                // Client is not authenticated and this is not an auth request
                Response errorResponse = createResponse("auth_required", CommandStatus::FAILED, 
                    "Authentication required");
                sendResponseToClient(clientId, errorResponse);
            }
        } else {
            // Invalid message type for unauthenticated client
            Response errorResponse = createResponse("auth_required", CommandStatus::FAILED, 
                "Authentication required");
            sendResponseToClient(clientId, errorResponse);
        }
    } catch (const std::exception& e) {
        Response errorResponse = createResponse("auth_error", CommandStatus::FAILED, 
            "Authentication error: " + std::string(e.what()));
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
    client.authToken = securityManager_->generateClientToken();
    
    clients_[client.id] = client;
    clientSockets_[client.id] = socket;
    
    // Log new connection
    if (logger_) {
        logger_->info("New client connected from " + address + " with ID: " + client.id + 
                     " (token: " + client.authToken.substr(0, 8) + "...)");
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
    
    // Remove from security manager
    securityManager_->removeClient(clientId);
}

bool IpcServer::createServerSocket(int port) {
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        if (logger_) {
            logger_->error("Failed to create server socket");
        }
        return false;
    }
    
    if (logger_) {
        logger_->info("Creating server socket on port " + std::to_string(port));
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
        int errorCode = 
#ifdef _WIN32
            WSAGetLastError();
#else
            errno;
#endif
        
        if (logger_) {
            logger_->error("Failed to bind server socket to port " + std::to_string(port) + 
                          " (error code: " + std::to_string(errorCode) + ")");
            
            if (errorCode == EADDRINUSE) {
                logger_->error("Port " + std::to_string(port) + " is already in use. "
                              "Please check if another agent is running or choose a different port.");
            }
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
        std::cout << "IPC Server listening on port " << port << std::endl;
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
    
    // Validate message size
    if (message.size() > Constants::MAX_MESSAGE_SIZE) {
        if (logger_) {
            logger_->error("Message too large to send: " + std::to_string(message.size()) + " bytes (max: " + std::to_string(Constants::MAX_MESSAGE_SIZE) + ")");
        }
        return false;
    }
    
    // Send message length first
    uint32_t msgLength = static_cast<uint32_t>(message.length());
    uint32_t sent = 0;
    
    while (sent < sizeof(msgLength)) {
        int result = send(socket, (const char*)&msgLength + sent, sizeof(msgLength) - sent, 0);
        if (result <= 0) {
            if (logger_) {
                logger_->error("Failed to send message length to socket " + std::to_string(socket));
            }
            return false;
        }
        sent += result;
    }
    
    // Send message content
    sent = 0;
    while (sent < msgLength) {
        int result = send(socket, message.c_str() + sent, msgLength - sent, 0);
        if (result <= 0) {
            if (logger_) {
                logger_->error("Failed to send message content to socket " + std::to_string(socket));
            }
            return false;
        }
        sent += result;
    }
    
    return true;
}
std::string IpcServer::receiveMessage(int socket) {
    if (socket < 0) {
        if (logger_) {
            logger_->error("Invalid socket for receiveMessage");
        }
        return "";
    }
    
    // Check if we're shutting down
    if (shuttingDown_) {
        return "";
    }
    
    // Use select for timeout-based receive
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(socket, &read_fds);
    
    struct timeval timeout;
    timeout.tv_sec = 1; // 1 second timeout
    timeout.tv_usec = 0;
    
    int selectResult = select(socket + 1, &read_fds, nullptr, nullptr, &timeout);
    if (selectResult <= 0) {
        if (selectResult == 0) {
            // Timeout - check if we should continue
            return shuttingDown_ ? "" : "";
        } else {
            if (logger_) {
                logger_->error("Select failed on socket " + std::to_string(socket));
            }
            return "";
        }
    }
    
    if (!FD_ISSET(socket, &read_fds)) {
        return "";
    }
    
    // Check shutdown flag again after select
    if (shuttingDown_) {
        return "";
    }
    
    // Receive length first
    uint32_t msgLength;
    int received = recv(socket, (char*)&msgLength, sizeof(msgLength), 0);
    if (received != sizeof(msgLength)) {
        if (received == 0) {
            if (logger_) {
                logger_->info("Client disconnected gracefully (socket " + std::to_string(socket) + ")");
            }
        } else {
            if (logger_) {
                logger_->error("Failed to receive message length from socket " + std::to_string(socket) + ": " + std::to_string(received) + " bytes");
            }
        }
        return "";
    }
    
    // Validate message length against our limit
    if (msgLength > Constants::MAX_MESSAGE_SIZE) {
        if (logger_) {
            logger_->warning("Received message too large: " + std::to_string(msgLength) + " bytes (max: " + std::to_string(Constants::MAX_MESSAGE_SIZE) + ")");
        }
        return "";
    }
    
    // Validate minimum message length
    if (msgLength < 2) { // At least "{}"
        if (logger_) {
            logger_->warning("Received message too small: " + std::to_string(msgLength) + " bytes");
        }
        return "";
    }
    
    // Receive message with timeout protection
    std::string message(msgLength, '\0');
    uint32_t totalReceived = 0;
    
    while (totalReceived < msgLength && !shuttingDown_) {
        // Use select for timeout on each receive
        FD_ZERO(&read_fds);
        FD_SET(socket, &read_fds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        selectResult = select(socket + 1, &read_fds, nullptr, nullptr, &timeout);
        if (selectResult <= 0) {
            if (selectResult == 0) {
                // Timeout - continue if not shutting down
                continue;
            } else {
                if (logger_) {
                    logger_->error("Select failed during message receive from socket " + std::to_string(socket));
                }
                return "";
            }
        }
        
        if (!FD_ISSET(socket, &read_fds)) {
            continue;
        }
        
        int result = recv(socket, &message[totalReceived], msgLength - totalReceived, 0);
        if (result <= 0) {
            if (result == 0) {
                if (logger_) {
                    logger_->warning("Client disconnected during message receive (socket " + std::to_string(socket) + ")");
                }
            } else {
                if (logger_) {
                    logger_->error("Failed to receive message content from socket " + std::to_string(socket) + ": " + std::to_string(result));
                }
            }
            return "";
        }
        totalReceived += result;
    }
    
    if (shuttingDown_) {
        return "";
    }
    
    if (logger_) {
        logger_->info("Received message of " + std::to_string(msgLength) + " bytes from socket " + std::to_string(socket));
    }
    
    return message;
}

void IpcServer::cleanupInactiveClients() {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    
    auto now = std::chrono::system_clock::now();
    std::vector<std::string> toRemove;
    
    for (const auto& client : clients_) {
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - client.second.lastActivity);
        if (duration > Constants::CLIENT_TIMEOUT) {
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
        
        // Remove from security manager
        securityManager_->removeClient(clientId);
        
        if (logger_) {
            logger_->info("Removed inactive client: " + clientId);
        }
    }
    
    // Cleanup security manager
    securityManager_->cleanupInactiveClients();
}

} // namespace SysMon
