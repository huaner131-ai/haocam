/**
 * HuanFace Minimal ZIP Reader — Phase 3
 * Supports STORE (method 0) only for now, DEFLATE returns error (needs zlib)
 * Clean-room, no external dependency, MIT
 * For reading .hfbundle ZIP open format (PK magic)
 * NOT for FaceUnity encrypted bundles (magic F3 5B 06 12 PROTECTED)
 */

#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <fstream>
#include <stdexcept>

namespace huanface {

struct ZipEntry {
    std::string filename;
    uint32_t compressedSize = 0;
    uint32_t uncompressedSize = 0;
    uint16_t compressionMethod = 0; // 0=STORE, 8=DEFLATE
    uint32_t localHeaderOffset = 0;
    uint32_t crc32 = 0;
};

class ZipReader {
public:
    ZipReader() = default;
    ~ZipReader() { Close(); }

    // Open ZIP file from disk
    bool Open(const std::string& zipPath, std::string& outError);
    // Open from memory buffer
    bool OpenFromMemory(const uint8_t* data, size_t size, std::string& outError);
    void Close();

    // List files
    std::vector<ZipEntry> GetEntries() const { return entries; }
    bool HasFile(const std::string& filename) const;

    // Read file content (only STORE supported for now, DEFLATE returns error)
    bool ReadFile(const std::string& filename, std::vector<uint8_t>& outData, std::string& outError);
    // Read file as string
    bool ReadFileAsString(const std::string& filename, std::string& outStr, std::string& outError);

    bool IsOpen() const { return isOpen; }

private:
    bool isOpen = false;
    std::string zipPath;
    std::vector<uint8_t> memoryData; // if opened from memory
    bool fromMemory = false;
    std::vector<ZipEntry> entries;
    std::map<std::string, ZipEntry> entryMap;

    // For file reading
    std::ifstream fileStream;
    std::vector<uint8_t> fileBuffer; // for memory mode

    bool ParseCentralDirectory(std::string& outError);
    bool ParseEOCD(const uint8_t* data, size_t size, uint32_t& outCentralDirOffset, uint32_t& outCentralDirSize, uint16_t& outTotalEntries, std::string& outError);
    bool ReadLocalFileData(const ZipEntry& entry, std::vector<uint8_t>& outData, std::string& outError);
};

} // namespace huanface
