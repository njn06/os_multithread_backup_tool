//main.cpp
#include "backup.h"
#include "logger.h"
#include "encrypt.h"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <csignal>

static std::string normalizePathForWSL(const std::string &inputPath) {
    if (inputPath.size() >= 2 && std::isalpha(static_cast<unsigned char>(inputPath[0])) && inputPath[1] == ':') {
        char driveLetter = static_cast<char>(std::tolower(static_cast<unsigned char>(inputPath[0])));
        std::string rest = inputPath.substr(2);
        std::string converted;
        converted.reserve(rest.size() + 10);
        for (char ch : rest) converted.push_back(ch == '\\' ? '/' : ch);
        if (!converted.empty() && converted.front() == '/') {
            // ok
        } else if (!converted.empty()) {
            converted = std::string("/") + converted;
        }
        return std::string("/mnt/") + driveLetter + converted;
    }
    std::string replaced = inputPath;
    std::replace(replaced.begin(), replaced.end(), '\\', '/');
    return replaced;
}

static bool parseLogFile(const std::string &logPath, std::string &key, std::string &iv) {
    std::ifstream log(logPath);
    if (!log.is_open()) return false;
    
    std::string line;
    while (std::getline(log, line)) {
        auto kpos = line.find("ENCRYPTION_KEY=");
        if (kpos != std::string::npos) {
            key = line.substr(kpos + std::string("ENCRYPTION_KEY=").size());
        }
        auto ipos = line.find("ENCRYPTION_IV=");
        if (ipos != std::string::npos) {
            iv = line.substr(ipos + std::string("ENCRYPTION_IV=").size());
        }
    }
    return !key.empty() && !iv.empty();
}

static void showUsage() {
    std::cout << "Advanced Backup Tool\n";
    std::cout << "Usage:\n";
    std::cout << "  Backup: " << "AdvancedBackupTool <source> <dest> [threads]\n";
    std::cout << "  Decrypt: " << "AdvancedBackupTool --decrypt <encrypted_file> <output_file> [log_file]\n";
    std::cout << "  Interactive: " << "AdvancedBackupTool\n";
}

int main(int argc, char *argv[]) {
    // Handle Ctrl+C to stop continuous backup gracefully
    std::signal(SIGINT, [](int){ requestStopBackup(); });
    // Check for decrypt mode
    if (argc >= 4 && std::string(argv[1]) == "--decrypt") {
        std::string encryptedFile = argv[2];
        std::string outputFile = argv[3];
        std::string logFile = (argc >= 5) ? argv[4] : "log.txt";
        
        std::string key, iv;
        if (!parseLogFile(logFile, key, iv)) {
            std::cerr << "Error: Could not find encryption keys in " << logFile << std::endl;
            std::cerr << "Make sure the log file contains ENCRYPTION_KEY=... and ENCRYPTION_IV=... lines" << std::endl;
            return 1;
        }
        
        std::cout << "Decrypting " << encryptedFile << " to " << outputFile << std::endl;
        if (decryptFileWithKey(encryptedFile, outputFile, key, iv)) {
            std::cout << "Decryption successful!" << std::endl;
            return 0;
        } else {
            std::cerr << "Decryption failed!" << std::endl;
            return 1;
        }
    }
    
    // Check for help
    if (argc >= 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        showUsage();
        return 0;
    }
    
    std::string sourceDir, destDir;
    int threadCount = 4;

    if (argc >= 3) {
        sourceDir = argv[1];
        destDir = argv[2];
        if (argc >= 4) {
            try {
                threadCount = std::stoi(argv[3]);
            } catch (...) {
                threadCount = 4;
            }
        }
    } else {
        std::cout << "Enter source directory: ";
        std::getline(std::cin, sourceDir);
        std::cout << "Enter destination directory: ";
        std::getline(std::cin, destDir);
        std::cout << "Enter number of threads (default 4): ";
        std::string tc;
        std::getline(std::cin, tc);
        if (!tc.empty()) {
            try { threadCount = std::stoi(tc); } catch (...) { threadCount = 4; }
        }
    }

    sourceDir = normalizePathForWSL(sourceDir);
    destDir = normalizePathForWSL(destDir);

    if (!std::filesystem::exists(sourceDir) || !std::filesystem::is_directory(sourceDir)) {
        std::cerr << "Error: source directory does not exist or is not a directory: " << sourceDir << std::endl;
        return 1;
    }

    logMessage("Backup started.");
    try {
        performBackup(sourceDir, destDir, threadCount);
    } catch (const std::exception &ex) {
        std::cerr << "Backup failed: " << ex.what() << std::endl;
        logMessage(std::string("Backup failed: ") + ex.what());
        return 1;
    }
    logMessage("Backup finished.");

    std::cout << "Backup completed. Check log.txt for details." << std::endl;
    return 0;
}
