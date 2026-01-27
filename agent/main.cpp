#include "agentcore.h"
#include <iostream>
#include <signal.h>
#include <memory>

using namespace SysMon;

// Global agent instance for signal handling
std::unique_ptr<AgentCore> g_agent;

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    if (g_agent) {
        g_agent->stop();
        g_agent->shutdown();
    }
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
        g_agent = std::make_unique<AgentCore>();
        
        // Initialize agent
        if (!g_agent->initialize()) {
            std::cerr << "Failed to initialize agent" << std::endl;
            return 1;
        }
        
        // Start agent
        if (!g_agent->start()) {
            std::cerr << "Failed to start agent" << std::endl;
            g_agent->shutdown();
            return 1;
        }
        
        std::cout << "SysMon3 Agent started successfully" << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        
        // Keep agent running
        while (g_agent->isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Agent failed with exception: " << e.what() << std::endl;
        if (g_agent) {
            g_agent->shutdown();
        }
        return 1;
    }
    
    std::cout << "SysMon3 Agent stopped" << std::endl;
    return 0;
}
