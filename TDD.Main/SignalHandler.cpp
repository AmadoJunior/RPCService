#include "SignalHandler.h"
#include <iostream>

// Initialize Static Member
std::atomic<bool> SignalHandler::running_(true);

void SignalHandler::initialize() {
    signal(SIGINT, SignalHandler::handleSignal);   // Handle Ctrl+C
    signal(SIGTERM, SignalHandler::handleSignal);  // Handle Termination Request

    std::cout << "Signal handlers initialized." << std::endl;
}

void SignalHandler::handleSignal(int signum) {
    std::cout << "Received signal " << signum << ". Shutting down..." << std::endl;
    running_ = false;
}

bool SignalHandler::isRunning() {
    return running_.load();
}

void SignalHandler::stop() {
    running_ = false;
}