#include "backup.h"
#include "logger.h"
#include "encrypt.h"
#include "compress.h"

#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <set>
#include <chrono>
#include <zlib.h>
#include <openssl/evp.h>

namespace fs = std::filesystem;

static std::atomic<bool> g_shouldStop{false};
static const char *kStopFileName = ".abt_stop";

// External encryption key/IV from encrypt.cpp
extern std::vector<unsigned char> g_key;
extern std::vector<unsigned char> g_iv;

void requestStopBackup() {
    g_shouldStop.store(true);
}

// Copy, compress, and encrypt a single file in one operation
void copyFile(const fs::path &src, const fs::path &dest) {
    try {
        fs::create_directories(dest.parent_path());

        // Read source file
        std::ifstream in(src, std::ios::binary);
        if (!in.is_open()) {
            logMessage("Failed to open source: " + src.string());
            return;
        }

        // Compress and encrypt in one step
        std::string compressedPath = dest.string() + ".gz";
        std::string encryptedPath = compressedPath + ".enc";
        
        // First compress
        gzFile gz = gzopen(compressedPath.c_str(), "wb9");
        if (!gz) {
            logMessage("Failed to create compressed file: " + compressedPath);
            return;
        }
        
        std::vector<char> buf(8192);
        while (in) {
            in.read(buf.data(), buf.size());
            std::streamsize got = in.gcount();
            if (got <= 0) break;
            if (gzwrite(gz, buf.data(), static_cast<unsigned int>(got)) == 0) {
                gzclose(gz);
                logMessage("Compression failed: " + src.string());
                return;
            }
        }
        gzclose(gz);
        
        // Then encrypt the compressed file
        ensureEncryptionKeyLogged();
        
        std::ifstream compIn(compressedPath, std::ios::binary);
        std::ofstream encOut(encryptedPath, std::ios::binary);
        
        if (!compIn.is_open() || !encOut.is_open()) {
            logMessage("Failed to encrypt: " + compressedPath);
            return;
        }
        
        // AES-256-CTR encryption
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            logMessage("Failed to create encryption context");
            return;
        }
        
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), nullptr, g_key.data(), g_iv.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            logMessage("Failed to initialize encryption");
            return;
        }
        
        std::vector<unsigned char> inBuf(8192);
        std::vector<unsigned char> outBuf(inBuf.size() + EVP_CIPHER_block_size(EVP_aes_256_ctr()));
        int outLen = 0;
        
        while (compIn) {
            compIn.read(reinterpret_cast<char*>(inBuf.data()), inBuf.size());
            std::streamsize got = compIn.gcount();
            if (got <= 0) break;
            
            if (EVP_EncryptUpdate(ctx, outBuf.data(), &outLen, inBuf.data(), static_cast<int>(got)) != 1) {
                EVP_CIPHER_CTX_free(ctx);
                logMessage("Encryption failed: " + compressedPath);
                return;
            }
            encOut.write(reinterpret_cast<char*>(outBuf.data()), outLen);
        }
        
        if (EVP_EncryptFinal_ex(ctx, outBuf.data(), &outLen) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            logMessage("Encryption final failed: " + compressedPath);
            return;
        }
        if (outLen > 0) encOut.write(reinterpret_cast<char*>(outBuf.data()), outLen);
        
        EVP_CIPHER_CTX_free(ctx);
        
        // Remove intermediate compressed file
        std::remove(compressedPath.c_str());
        
        // Remove original backup file if it exists
        if (fs::exists(dest)) {
            fs::remove(dest);
        }

        logMessage("Backed up (compressed+encrypted): " + src.string() + " -> " + encryptedPath);
    } catch (...) {
        logMessage("Failed to copy: " + src.string());
    }
}

// Perform multithreaded backup
void performBackup(const std::string &srcDir, const std::string &destDir, int threadCount) {
    if (threadCount <= 0) threadCount = 1;
    
    logMessage("Starting backup from " + srcDir + " to " + destDir);
    logMessage("Press Ctrl+C to stop backup process");
    
    std::set<fs::path> processedFiles;
    
    while (!g_shouldStop.load()) {
        // Stop if stop-file exists in destination root
        if (fs::exists(fs::path(destDir) / kStopFileName)) {
            g_shouldStop.store(true);
            break;
        }
        std::vector<std::thread> threads;
        bool foundNewFiles = false;
        
        for (auto &entry : fs::recursive_directory_iterator(srcDir)) {
            if (g_shouldStop.load()) break;
            if (entry.is_regular_file()) {
                fs::path relative = fs::relative(entry.path(), srcDir);
                fs::path destPath = fs::path(destDir) / relative;
                fs::path encryptedPath = destPath.string() + ".gz.enc";
                
                // Check if file was already processed and is newer
                if (processedFiles.find(entry.path()) != processedFiles.end()) {
                    // Check if source file is newer than encrypted backup
                    if (fs::exists(encryptedPath)) {
                        auto srcTime = fs::last_write_time(entry.path());
                        auto destTime = fs::last_write_time(encryptedPath);
                        if (srcTime <= destTime) {
                            continue; // File hasn't changed
                        }
                    }
                }
                
                processedFiles.insert(entry.path());
                foundNewFiles = true;
                
                threads.emplace_back(copyFile, entry.path(), destPath);

                if (static_cast<int>(threads.size()) >= threadCount) {
                    for (auto &t : threads) t.join();
                    threads.clear();
                }
            }
        }

        for (auto &t : threads) t.join();
        
        if (!foundNewFiles && !g_shouldStop.load()) {
            logMessage("No new files to backup. Monitoring for changes...");
            std::this_thread::sleep_for(std::chrono::seconds(5)); // Wait 5 seconds before checking again
        }
    }
    
    logMessage("Backup process stopped by user");
}
