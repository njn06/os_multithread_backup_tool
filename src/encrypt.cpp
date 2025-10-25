//encrypt.cpp
#include "encrypt.h"
#include "logger.h"
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <fstream>
#include <vector>
#include <iostream>
#include <mutex>
#include <zlib.h>

// Make key/IV accessible to backup.cpp
std::vector<unsigned char> g_key;
std::vector<unsigned char> g_iv;
std::once_flag g_keyOnce;

    std::string toHex(const std::vector<unsigned char> &buf) {
        static const char *digits = "0123456789ABCDEF";
        std::string out;
        out.reserve(buf.size() * 2);
        for (unsigned char b : buf) {
            out.push_back(digits[b >> 4]);
            out.push_back(digits[b & 0xF]);
        }
        return out;
    }

void ensureEncryptionKeyLogged() {
    std::call_once(g_keyOnce, [](){
        g_key.assign(32, 0);
        g_iv.assign(16, 0);
        RAND_bytes(g_key.data(), static_cast<int>(g_key.size()));
        RAND_bytes(g_iv.data(), static_cast<int>(g_iv.size()));
        std::string keyHex = toHex(g_key);
        std::string ivHex = toHex(g_iv);
        logMessage(std::string("ENCRYPTION_KEY=") + keyHex);
        logMessage(std::string("ENCRYPTION_IV=") + ivHex);
        std::cout << "Generated encryption key: " << keyHex << std::endl;
        std::cout << "Generated encryption IV: " << ivHex << std::endl;
    });
}

static bool aes256CtrFile(const std::string &inPath, const std::string &outPath, bool encrypt) {
    ensureEncryptionKeyLogged();
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    const EVP_CIPHER *cipher = EVP_aes_256_ctr();
    if (EVP_EncryptInit_ex(ctx, cipher, nullptr, g_key.data(), g_iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    std::ifstream in(inPath, std::ios::binary);
    std::ofstream out(outPath, std::ios::binary);
    if (!in.is_open() || !out.is_open()) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    std::vector<unsigned char> inBuf(4096);
    std::vector<unsigned char> outBuf(inBuf.size() + EVP_CIPHER_block_size(cipher));
    int outLen = 0;
    while (in) {
        in.read(reinterpret_cast<char*>(inBuf.data()), static_cast<std::streamsize>(inBuf.size()));
        std::streamsize got = in.gcount();
        if (got <= 0) break;
        if (EVP_EncryptUpdate(ctx, outBuf.data(), &outLen, inBuf.data(), static_cast<int>(got)) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return false;
        }
        out.write(reinterpret_cast<char*>(outBuf.data()), outLen);
    }
    if (EVP_EncryptFinal_ex(ctx, outBuf.data(), &outLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    if (outLen > 0) out.write(reinterpret_cast<char*>(outBuf.data()), outLen);
    EVP_CIPHER_CTX_free(ctx);
    return true;
}

void encryptFile(const std::string &filePath) {
    std::string outPath = filePath + ".enc";
    if (aes256CtrFile(filePath, outPath, true)) {
        logMessage("Encrypted file: " + filePath + " -> " + outPath);
    } else {
        logMessage("Encryption failed: " + filePath);
    }
}

void decryptFile(const std::string &filePath) {
    std::string outPath = filePath + ".dec";
    if (aes256CtrFile(filePath, outPath, false)) {
        logMessage("Decrypted file: " + filePath + " -> " + outPath);
    } else {
        logMessage("Decryption failed: " + filePath);
    }
}

static std::vector<unsigned char> hexToBytes(const std::string &hex) {
    std::vector<unsigned char> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteStr = hex.substr(i, 2);
        unsigned char byte = static_cast<unsigned char>(std::strtol(byteStr.c_str(), nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

bool decryptFileWithKey(const std::string &encryptedPath, const std::string &outputPath, 
                        const std::string &keyHex, const std::string &ivHex) {
    std::vector<unsigned char> key = hexToBytes(keyHex);
    std::vector<unsigned char> iv = hexToBytes(ivHex);
    
    if (key.size() != 32 || iv.size() != 16) {
        logMessage("Invalid key/IV size for decryption");
        return false;
    }
    
    // Step 1: Decrypt the encrypted file to get compressed file
    std::string tempCompressed = outputPath + ".temp.gz";
    
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_ctr(), nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    std::ifstream in(encryptedPath, std::ios::binary);
    std::ofstream tempOut(tempCompressed, std::ios::binary);
    if (!in.is_open() || !tempOut.is_open()) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    std::vector<unsigned char> inBuf(4096);
    std::vector<unsigned char> outBuf(inBuf.size() + EVP_CIPHER_block_size(EVP_aes_256_ctr()));
    int outLen = 0;
    
    while (in) {
        in.read(reinterpret_cast<char*>(inBuf.data()), static_cast<std::streamsize>(inBuf.size()));
        std::streamsize got = in.gcount();
        if (got <= 0) break;
        
        if (EVP_DecryptUpdate(ctx, outBuf.data(), &outLen, inBuf.data(), static_cast<int>(got)) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return false;
        }
        tempOut.write(reinterpret_cast<char*>(outBuf.data()), outLen);
    }
    
    if (EVP_DecryptFinal_ex(ctx, outBuf.data(), &outLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    if (outLen > 0) tempOut.write(reinterpret_cast<char*>(outBuf.data()), outLen);
    
    EVP_CIPHER_CTX_free(ctx);
    tempOut.close();
    
    // Step 2: Decompress the compressed file to get original
    gzFile gz = gzopen(tempCompressed.c_str(), "rb");
    if (!gz) {
        std::remove(tempCompressed.c_str());
        return false;
    }
    
    std::ofstream finalOut(outputPath, std::ios::binary);
    if (!finalOut.is_open()) {
        gzclose(gz);
        std::remove(tempCompressed.c_str());
        return false;
    }
    
    std::vector<char> buf(8192);
    int bytesRead;
    while ((bytesRead = gzread(gz, buf.data(), buf.size())) > 0) {
        finalOut.write(buf.data(), bytesRead);
    }
    
    gzclose(gz);
    finalOut.close();
    std::remove(tempCompressed.c_str());
    
    return true;
}
