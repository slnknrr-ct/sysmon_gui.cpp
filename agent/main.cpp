#include "agentcore.h"
#include <iostream>
#include <signal.h>
#include <memory>
#include <mutex>

using namespace SysMon;

// Thread-safe agent instance management
namespace {
    std::unique_ptr<AgentCore> g_agent;
    std::mutex g_agentMutex;
    
    void setAgent(std::unique_ptr<AgentCore> agent) {
        std::lock_guard<std::mutex> lock(g_agentMutex);
        g_agent = std::move(agent);
    }
    
    std::unique_ptr<AgentCore> getAgent() {
        std::lock_guard<std::mutex> lock(g_agentMutex);
        return std::move(g_agent);
    }
    
    void stopAgent() {
        std::lock_guard<std::mutex> lock(g_agentMutex);
        if (g_agent) {
            g_agent->stop();
            g_agent->shutdown();
        }
    }
}

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    stopAgent();
    exit(0);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    std::cout << "SysMon3 Agent starting..." << std::endl;
    
    // Set up signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
    
    try {
        // Create agent instance
        auto agent = std::make_unique<AgentCore>();
        setAgent(std::move(agent));
        
        // Get agent for initialization
        auto currentAgent = getAgent();
        if (!currentAgent) {
            std::cerr << "Failed to create agent instance" << std::endl;
            return 1;
        }
        
        // Initialize agent
        if (!currentAgent->initialize()) {
            std::cerr << "Failed to initialize agent" << std::endl;
            return 1;
        }
        
        // Start agent
        if (!currentAgent->start()) {
            std::cerr << "Failed to start agent" << std::endl;
            currentAgent->shutdown();
            return 1;
        }
        
        // Put agent back for signal handling
        setAgent(std::move(currentAgent));
        
        std::cout << "SysMon3 Agent started successfully" << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        
        // Keep agent running
        auto runningAgent = getAgent();
        while (runningAgent && runningAgent->isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Agent failed with exception: " << e.what() << std::endl;
        stopAgent();
        return 1;
    }
    
    std::cout << "SysMon3 Agent stopped" << std::endl;
    return 0;
}
