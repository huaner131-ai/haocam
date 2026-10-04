/**
 * HuanFace Bundle Reader — Phase 3
 * Reads .hfbundle ZIP open format or unpacked directory
 * Clean-room, no FaceUnity encrypted handling
 */

#pragma once
#include "manifest.h"
#include "zip_reader.h"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace huanface {

class BundleReader {
public:
    BundleReader() = default;
    ~BundleReader() = default;

    // Open bundle from path (file .hfbundle ZIP or directory)
    bool Open(const std::string& path, std::string& outError);
    // Open from memory (ZIP data)
    bool OpenFromMemory(const uint8_t* data, size_t size, std::string& outError);
    void Close();

    bool IsOpen() const { return isOpen; }
    bool IsDirectory() const { return isDirectory; }
    const HFManifest& GetManifest() const { return manifest; }
    std::string GetBundlePath() const { return bundlePath; }

    // List files in bundle
    std::vector<std::string> ListFiles() const;

    // Check if file exists
    bool HasFile(const std::string& filename) const;

    // Read file content
    bool ReadFile(const std::string& filename, std::vector<uint8_t>& outData, std::string& outError);
    bool ReadFileAsString(const std::string& filename, std::string& outStr, std::string& outError);

    // Read manifest (already parsed)
    const HFManifest& GetParsedManifest() const { return manifest; }

private:
    bool isOpen = false;
    bool isDirectory = false;
    std::string bundlePath;
    HFManifest manifest;
    std::string manifestJsonStr;

    // For ZIP mode
    std::unique_ptr<ZipReader> zipReader;

    // For directory mode
    std::map<std::string, std::string> fileMap; // logical path -> absolute path

    bool LoadManifestFromJson(const std::string& jsonStr, std::string& outError);
    bool ScanDirectory(const std::string& dirPath, std::string& outError);
};

} // namespace huanface
