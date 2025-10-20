#ifndef ENCRYPT_H
#define ENCRYPT_H

#include <string>

void encryptFile(const std::string &filePath);
void decryptFile(const std::string &filePath);

// Ensure a session key/iv are generated once per run and logged via logger.
void ensureEncryptionKeyLogged();

// Decrypt with specific key/IV
bool decryptFileWithKey(const std::string &encryptedPath, const std::string &outputPath, 
                        const std::string &keyHex, const std::string &ivHex);

#endif
