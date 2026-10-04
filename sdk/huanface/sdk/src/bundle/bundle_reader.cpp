/**
 * HuanFace Bundle Reader Implementation — Phase 3
 */

#include "bundle_reader.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace huanface {

namespace fs = std::filesystem;

bool BundleReader::Open(const std::string& path, std::string& outError) {
    Close();
    bundlePath = path;

    // Check if path is directory
    try {
        if (fs::is_directory(path)) {
            isDirectory = true;
            // Scan directory
            if (!ScanDirectory(path, outError)) {
                Close();
                return false;
            }
            // Read manifest.json
            std::string manifestPath = (fs::path(path) / "manifest.json").string();
            if (!fs::exists(manifestPath)) {
                outError = "manifest.json not found in bundle directory: " + path;
                Close();
                return false;
            }
            std::ifstream f(manifestPath);
            if (!f.is_open()) {
                outError = "Failed to open manifest.json: " + manifestPath;
                Close();
                return false;
            }
            std::stringstream ss;
            ss << f.rdbuf();
            manifestJsonStr = ss.str();
            if (!LoadManifestFromJson(manifestJsonStr, outError)) {
                Close();
                return false;
            }
            isOpen = true;
            return true;
        }
    } catch (const std::exception& e) {
        outError = std::string("Filesystem error checking path: ") + e.what();
        Close();
        return false;
    }

    // Otherwise treat as ZIP file
    // First check for FaceUnity encrypted magic BEFORE trying ZIP parse
    {
        std::ifstream f(path, std::ios::binary);
        if (f.is_open()) {
            uint8_t magic[4];
            f.read((char*)magic, 4);
            if (f.gcount() == 4) {
                if (magic[0] == 0xF3 && magic[1] == 0x5B && magic[2] == 0x06 && magic[3] == 0x12) {
                    outError = "Detected FaceUnity encrypted bundle (magic F3 5B 06 12 PROTECTED). This tool only handles HuanFace .hfbundle ZIP open format. No DRM bypass.";
                    Close();
                    return false;
                }
            }
        }
    }

    isDirectory = false;
    zipReader = std::make_unique<ZipReader>();
    if (!zipReader->Open(path, outError)) {
        Close();
        return false;
    }

    // Read manifest.json from ZIP
    std::string manifestStr;
    if (!zipReader->ReadFileAsString("manifest.json", manifestStr, outError)) {
        // Try case insensitive? No, must be exact
        outError = "manifest.json not found in ZIP bundle: " + outError;
        Close();
        return false;
    }
    manifestJsonStr = manifestStr;
    if (!LoadManifestFromJson(manifestJsonStr, outError)) {
        Close();
        return false;
    }

    isOpen = true;
    return true;
}

bool BundleReader::OpenFromMemory(const uint8_t* data, size_t size, std::string& outError) {
    Close();
    isDirectory = false;
    bundlePath = "<memory>";

    // Check magic for FaceUnity
    if (size >= 4 && data[0] == 0xF3 && data[1] == 0x5B && data[2] == 0x06 && data[3] == 0x12) {
        outError = "Detected FaceUnity encrypted bundle (magic F3 5B 06 12 PROTECTED) in memory. Only HuanFace ZIP supported.";
        return false;
    }

    // Check PK magic for ZIP
    if (size < 4 || !(data[0] == 'P' && data[1] == 'K')) {
        outError = "Memory data not ZIP (PK magic missing), size=" + std::to_string(size);
        return false;
    }

    zipReader = std::make_unique<ZipReader>();
    if (!zipReader->OpenFromMemory(data, size, outError)) {
        Close();
        return false;
    }

    std::string manifestStr;
    if (!zipReader->ReadFileAsString("manifest.json", manifestStr, outError)) {
        outError = "manifest.json not found in memory ZIP: " + outError;
        Close();
        return false;
    }
    manifestJsonStr = manifestStr;
    if (!LoadManifestFromJson(manifestJsonStr, outError)) {
        Close();
        return false;
    }

    isOpen = true;
    return true;
}

void BundleReader::Close() {
    isOpen = false;
    isDirectory = false;
    bundlePath.clear();
    manifest = HFManifest();
    manifestJsonStr.clear();
    if (zipReader) {
        zipReader->Close();
        zipReader.reset();
    }
    fileMap.clear();
}

bool BundleReader::LoadManifestFromJson(const std::string& jsonStr, std::string& outError) {
    try {
        manifest = ManifestParser::Parse(jsonStr);
    } catch (const std::exception& e) {
        outError = std::string("Failed to parse manifest.json: ") + e.what();
        return false;
    }

    std::string validationError = ManifestParser::Validate(manifest);
    if (!validationError.empty()) {
        outError = "Manifest validation failed: " + validationError;
        return false;
    }

    return true;
}

bool BundleReader::ScanDirectory(const std::string& dirPath, std::string& outError) {
    try {
        fs::path base(dirPath);
        for (auto& p : fs::recursive_directory_iterator(base)) {
            if (p.is_regular_file()) {
                fs::path rel = fs::relative(p.path(), base);
                std::string relStr = rel.string();
                // Convert backslashes to forward slashes for consistency
                std::replace(relStr.begin(), relStr.end(), '\\', '/');
                fileMap[relStr] = p.path().string();
            }
        }
        return true;
    } catch (const std::exception& e) {
        outError = std::string("Failed to scan directory: ") + e.what();
        return false;
    }
}

std::vector<std::string> BundleReader::ListFiles() const {
    if (!isOpen) return {};
    if (isDirectory) {
        std::vector<std::string> files;
        for (const auto& kv : fileMap) files.push_back(kv.first);
        return files;
    } else {
        if (!zipReader) return {};
        std::vector<std::string> files;
        for (const auto& e : zipReader->GetEntries()) files.push_back(e.filename);
        return files;
    }
}

bool BundleReader::HasFile(const std::string& filename) const {
    if (!isOpen) return false;
    if (ManifestParser::IsPathTraversal(filename)) return false;
    if (isDirectory) {
        return fileMap.find(filename) != fileMap.end();
    } else {
        if (!zipReader) return false;
        return zipReader->HasFile(filename);
    }
}

bool BundleReader::ReadFile(const std::string& filename, std::vector<uint8_t>& outData, std::string& outError) {
    if (!isOpen) {
        outError = "Bundle not open";
        return false;
    }
    if (ManifestParser::IsPathTraversal(filename)) {
        outError = "Path traversal detected: " + filename;
        return false;
    }
    if (isDirectory) {
        auto it = fileMap.find(filename);
        if (it == fileMap.end()) {
            outError = "File not found in bundle directory: " + filename;
            return false;
        }
        std::ifstream f(it->second, std::ios::binary);
        if (!f.is_open()) {
            outError = "Failed to open file: " + it->second;
            return false;
        }
        f.seekg(0, std::ios::end);
        size_t size = (size_t)f.tellg();
        f.seekg(0, std::ios::beg);
        outData.resize(size);
        f.read((char*)outData.data(), size);
        if ((size_t)f.gcount() != size) {
            outError = "Failed to read file: " + it->second;
            return false;
        }
        return true;
    } else {
        if (!zipReader) {
            outError = "ZIP reader not initialized";
            return false;
        }
        return zipReader->ReadFile(filename, outData, outError);
    }
}

bool BundleReader::ReadFileAsString(const std::string& filename, std::string& outStr, std::string& outError) {
    std::vector<uint8_t> data;
    if (!ReadFile(filename, data, outError)) return false;
    outStr.assign((char*)data.data(), data.size());
    return true;
}

} // namespace huanface
