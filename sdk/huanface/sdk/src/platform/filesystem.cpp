/**
 * HuanFace Platform Filesystem — Phase 3
 * Cross-platform implementation using std::filesystem
 * Windows: UTF-8 -> UTF-16 conversion, but for Phase 3 minimal use std::filesystem
 */

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace huanface {

namespace fs = std::filesystem;

class IFileSystem {
public:
    virtual ~IFileSystem() = default;
    virtual bool ReadFile(const std::string& path, std::vector<uint8_t>& outData) = 0;
    virtual bool WriteFile(const std::string& path, const void* data, size_t size) = 0;
    virtual bool Exists(const std::string& path) = 0;
    virtual bool IsDirectory(const std::string& path) = 0;
    virtual bool CreateDirectory(const std::string& path) = 0;
    virtual std::vector<std::string> ListFiles(const std::string& path) = 0;
};

class StdFileSystem : public IFileSystem {
public:
    bool ReadFile(const std::string& path, std::vector<uint8_t>& outData) override {
        try {
            if (!fs::exists(path)) return false;
            std::ifstream f(path, std::ios::binary);
            if (!f.is_open()) return false;
            f.seekg(0, std::ios::end);
            size_t size = (size_t)f.tellg();
            f.seekg(0, std::ios::beg);
            outData.resize(size);
            f.read((char*)outData.data(), size);
            return (size_t)f.gcount() == size;
        } catch (...) {
            return false;
        }
    }

    bool WriteFile(const std::string& path, const void* data, size_t size) override {
        try {
            std::ofstream f(path, std::ios::binary);
            if (!f.is_open()) return false;
            f.write((char*)data, size);
            return f.good();
        } catch (...) {
            return false;
        }
    }

    bool Exists(const std::string& path) override {
        try {
            return fs::exists(path);
        } catch (...) {
            return false;
        }
    }

    bool IsDirectory(const std::string& path) override {
        try {
            return fs::is_directory(path);
        } catch (...) {
            return false;
        }
    }

    bool CreateDirectory(const std::string& path) override {
        try {
            return fs::create_directories(path);
        } catch (...) {
            return false;
        }
    }

    std::vector<std::string> ListFiles(const std::string& path) override {
        std::vector<std::string> files;
        try {
            if (!fs::is_directory(path)) return files;
            for (auto& p : fs::recursive_directory_iterator(path)) {
                if (p.is_regular_file()) {
                    files.push_back(p.path().string());
                }
            }
        } catch (...) {}
        return files;
    }
};

} // namespace huanface
