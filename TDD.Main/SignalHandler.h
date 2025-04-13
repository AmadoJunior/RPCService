#pragma once

#include <atomic>
#include <csignal>

class SignalHandler {
private:
    static std::atomic<bool> running_;

public:
    // Initialize signal handlers
    static void initialize();

    // Signal handler callback
    static void handleSignal(int signum);

    // Check if the application should continue running
    static bool isRunning();

    // Request application to stop
    static void stop();
};