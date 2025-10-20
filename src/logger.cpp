#include "logger.h"
#include <iostream>
#include <chrono>
#include <ctime>

std::mutex logMutex;

void logMessage(const std::string &message) {
    std::lock_guard<std::mutex> lock(logMutex);

    std::ofstream logFile("log.txt", std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file!" << std::endl;
        return;
    }

    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    logFile << std::ctime(&now) << ": " << message << std::endl;
    logFile.close();
}
