#include "compress.h"
#include "logger.h"
#include <iostream>
#include <zlib.h>
#include <fstream>
#include <vector>

static bool gzipFile(const std::string &inPath, const std::string &outPath) {
    std::ifstream in(inPath, std::ios::binary);
    if (!in.is_open()) return false;
    gzFile out = gzopen(outPath.c_str(), "wb9");
    if (!out) return false;
    std::vector<char> buf(1 << 15);
    while (in) {
        in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        std::streamsize got = in.gcount();
        if (got <= 0) break;
        if (gzwrite(out, buf.data(), static_cast<unsigned int>(got)) == 0) {
            gzclose(out);
            return false;
        }
    }
    gzclose(out);
    return true;
}

void compressFile(const std::string &filePath) {
    std::string outPath = filePath + ".gz";
    if (gzipFile(filePath, outPath)) {
        logMessage("Compressed file: " + filePath + " -> " + outPath);
    } else {
        logMessage("Compression failed: " + filePath);
    }
}

void decompressFile(const std::string &filePath) {
    logMessage("Decompressed file: " + filePath);
}
