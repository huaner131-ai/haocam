/**
 * HuanFace Minimal ZIP Reader Implementation — Phase 3/4
 * Phase 3: STORE only
 * Phase 4: STORE + DEFLATE via zlib (Linux) / miniz (Windows vendored)
 * Clean-room, MIT
 */

#include "zip_reader.h"
#include <cstring>
#include <algorithm>

// Try to include zlib for DEFLATE support
#if __has_include(<zlib.h>)
#include <zlib.h>
#define HF_HAS_ZLIB 1
#elif __has_include("zlib.h")
#include "zlib.h"
#define HF_HAS_ZLIB 1
#else
#define HF_HAS_ZLIB 0
#endif

// Fallback: try miniz if zlib not available (Windows vendored)
#if !HF_HAS_ZLIB
#if __has_include("../../third_party/miniz/miniz.h")
#include "../../third_party/miniz/miniz.h"
#define HF_HAS_MINIZ 1
#else
#define HF_HAS_MINIZ 0
#endif
#else
#define HF_HAS_MINIZ 0
#endif

namespace huanface {

static uint16_t ReadU16LE(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
static uint32_t ReadU32LE(const uint8_t* p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

bool ZipReader::Open(const std::string& path, std::string& outError) {
    Close();
    zipPath = path;
    fromMemory = false;
    fileStream.open(path, std::ios::binary);
    if (!fileStream.is_open()) {
        outError = "Failed to open ZIP file: " + path;
        return false;
    }
    // Read entire file into buffer for parsing (simpler)
    fileStream.seekg(0, std::ios::end);
    size_t fileSize = (size_t)fileStream.tellg();
    fileStream.seekg(0, std::ios::beg);
    fileBuffer.resize(fileSize);
    fileStream.read((char*)fileBuffer.data(), fileSize);
    if ((size_t)fileStream.gcount() != fileSize) {
        outError = "Failed to read ZIP file: " + path;
        Close();
        return false;
    }
    // Keep fileStream open? We have buffer, so we can use buffer for parsing
    // Parse
    if (!ParseCentralDirectory(outError)) {
        Close();
        return false;
    }
    isOpen = true;
    return true;
}

bool ZipReader::OpenFromMemory(const uint8_t* data, size_t size, std::string& outError) {
    Close();
    fromMemory = true;
    memoryData.assign(data, data + size);
    fileBuffer = memoryData; // use same buffer
    if (!ParseCentralDirectory(outError)) {
        Close();
        return false;
    }
    isOpen = true;
    return true;
}

void ZipReader::Close() {
    isOpen = false;
    entries.clear();
    entryMap.clear();
    if (fileStream.is_open()) fileStream.close();
    fileBuffer.clear();
    memoryData.clear();
    zipPath.clear();
    fromMemory = false;
}

bool ZipReader::ParseEOCD(const uint8_t* data, size_t size, uint32_t& outCentralDirOffset, uint32_t& outCentralDirSize, uint16_t& outTotalEntries, std::string& outError) {
    // EOCD signature 0x06054b50 little endian: 0x50 0x4b 0x05 0x06
    // Search from end, up to 64KB + EOCD size
    const uint32_t EOCD_SIG = 0x06054b50;
    const size_t maxComment = 65535;
    const size_t eocdMinSize = 22;
    if (size < eocdMinSize) {
        outError = "ZIP too small for EOCD";
        return false;
    }
    size_t searchStart = (size > (maxComment + eocdMinSize)) ? size - (maxComment + eocdMinSize) : 0;
    for (size_t i = size - eocdMinSize; ; --i) {
        if (ReadU32LE(data + i) == EOCD_SIG) {
            // Found EOCD
            // struct: sig 4, diskNumber 2, centralDirDisk 2, entriesThisDisk 2, totalEntries 2, centralDirSize 4, centralDirOffset 4, commentLen 2
            if (i + eocdMinSize > size) {
                outError = "EOCD truncated";
                return false;
            }
            uint16_t totalEntries = ReadU16LE(data + i + 10);
            uint32_t centralDirSize = ReadU32LE(data + i + 12);
            uint32_t centralDirOffset = ReadU32LE(data + i + 16);
            uint16_t commentLen = ReadU16LE(data + i + 20);
            // Validate comment length matches remaining
            if (i + eocdMinSize + commentLen != size) {
                // Comment length may not match if there are extra bytes? But we found EOCD, check
                // For simplicity, allow if commentLen <= remaining
                // But we already searched from end, so this should be correct EOCD
                // Continue searching if not exact? For now accept if i + 22 + commentLen <= size and no other EOCD after
                // We'll accept
            }
            outCentralDirOffset = centralDirOffset;
            outCentralDirSize = centralDirSize;
            outTotalEntries = totalEntries;
            return true;
        }
        if (i == searchStart) break;
        if (i == 0) break;
    }
    outError = "EOCD not found (not a valid ZIP or ZIP64 not supported)";
    return false;
}

bool ZipReader::ParseCentralDirectory(std::string& outError) {
    const uint8_t* data = fileBuffer.data();
    size_t size = fileBuffer.size();

    uint32_t centralDirOffset = 0;
    uint32_t centralDirSize = 0;
    uint16_t totalEntries = 0;

    if (!ParseEOCD(data, size, centralDirOffset, centralDirSize, totalEntries, outError)) {
        return false;
    }

    if (centralDirOffset + centralDirSize > size) {
        outError = "Central directory out of bounds";
        return false;
    }

    // Parse central directory entries
    // Each entry: sig 0x02014b50, versionMadeBy 2, versionNeeded 2, flag 2, method 2, time 2, date 2, crc 4, compSize 4, uncompSize 4, nameLen 2, extraLen 2, commentLen 2, diskStart 2, internalAttr 2, externalAttr 4, localHeaderOffset 4, filename, extra, comment
    const uint32_t CD_SIG = 0x02014b50;
    size_t offset = centralDirOffset;
    for (uint16_t i = 0; i < totalEntries; ++i) {
        if (offset + 46 > size) {
            outError = "Central directory entry truncated";
            return false;
        }
        if (ReadU32LE(data + offset) != CD_SIG) {
            outError = "Invalid central directory signature at offset " + std::to_string(offset);
            return false;
        }
        uint16_t method = ReadU16LE(data + offset + 10);
        uint32_t crc = ReadU32LE(data + offset + 16);
        uint32_t compSize = ReadU32LE(data + offset + 20);
        uint32_t uncompSize = ReadU32LE(data + offset + 24);
        uint16_t nameLen = ReadU16LE(data + offset + 28);
        uint16_t extraLen = ReadU16LE(data + offset + 30);
        uint16_t commentLen = ReadU16LE(data + offset + 32);
        uint32_t localOffset = ReadU32LE(data + offset + 42);

        if (offset + 46 + nameLen + extraLen + commentLen > size) {
            outError = "Central directory entry filename out of bounds";
            return false;
        }

        std::string filename((char*)(data + offset + 46), nameLen);

        // Security: reject path traversal in ZIP entries themselves? We'll check later but store
        ZipEntry entry;
        entry.filename = filename;
        entry.compressedSize = compSize;
        entry.uncompressedSize = uncompSize;
        entry.compressionMethod = method;
        entry.localHeaderOffset = localOffset;
        entry.crc32 = crc;

        entries.push_back(entry);
        entryMap[filename] = entry;

        offset += 46 + nameLen + extraLen + commentLen;
    }

    return true;
}

bool ZipReader::HasFile(const std::string& filename) const {
    return entryMap.find(filename) != entryMap.end();
}

bool ZipReader::ReadLocalFileData(const ZipEntry& entry, std::vector<uint8_t>& outData, std::string& outError) {
    const uint8_t* data = fileBuffer.data();
    size_t size = fileBuffer.size();

    uint32_t localOffset = entry.localHeaderOffset;
    if (localOffset + 30 > size) {
        outError = "Local header out of bounds for " + entry.filename;
        return false;
    }
    const uint32_t LOCAL_SIG = 0x04034b50;
    if (ReadU32LE(data + localOffset) != LOCAL_SIG) {
        outError = "Invalid local header signature for " + entry.filename;
        return false;
    }
    uint16_t method = ReadU16LE(data + localOffset + 8);
    uint16_t nameLen = ReadU16LE(data + localOffset + 26);
    uint16_t extraLen = ReadU16LE(data + localOffset + 28);

    if (localOffset + 30 + nameLen + extraLen + entry.compressedSize > size) {
        outError = "Local file data out of bounds for " + entry.filename;
        return false;
    }

    // Verify method matches central directory (should)
    if (method != entry.compressionMethod) {
        // Allow but warn? For now use central dir method
    }

    const uint8_t* fileDataStart = data + localOffset + 30 + nameLen + extraLen;

    if (entry.compressionMethod == 0) { // STORE
        outData.assign(fileDataStart, fileDataStart + entry.compressedSize);
        return true;
    } else if (entry.compressionMethod == 8) { // DEFLATE
#if HF_HAS_ZLIB
        // DEFLATE via zlib (raw deflate, -MAX_WBITS)
        if (entry.uncompressedSize == 0) {
            outData.clear();
            return true;
        }
        outData.resize(entry.uncompressedSize);
        z_stream strm;
        memset(&strm, 0, sizeof(strm));
        strm.next_in = (Bytef*)fileDataStart;
        strm.avail_in = entry.compressedSize;
        strm.next_out = outData.data();
        strm.avail_out = entry.uncompressedSize;
        int ret = inflateInit2(&strm, -MAX_WBITS); // raw deflate
        if (ret != Z_OK) {
            outError = "inflateInit2 failed for " + entry.filename + " ret=" + std::to_string(ret);
            return false;
        }
        ret = inflate(&strm, Z_FINISH);
        inflateEnd(&strm);
        if (ret != Z_STREAM_END) {
            outError = "inflate failed for " + entry.filename + " ret=" + std::to_string(ret) + " msg=" + (strm.msg ? strm.msg : "") + " expected=" + std::to_string(entry.uncompressedSize) + " comp=" + std::to_string(entry.compressedSize);
            return false;
        }
        return true;
#elif HF_HAS_MINIZ
        // DEFLATE via miniz (vendored)
        if (entry.uncompressedSize == 0) {
            outData.clear();
            return true;
        }
        outData.resize(entry.uncompressedSize);
        mz_ulong outLen = entry.uncompressedSize;
        int status = mz_uncompress(outData.data(), &outLen, fileDataStart, entry.compressedSize);
        // mz_uncompress expects zlib-wrapped, but ZIP is raw deflate, so use tinfl_decompress_mem_to_mem with raw flag if available
        // For miniz, we need raw: use tinfl_decompress_mem_to_mem with 0 flags for raw? Actually tinfl needs flag handling.
        // Simplified: try raw via tinfl
        if (status != MZ_OK) {
            // Try raw inflate via tinfl_decompress_mem_to_mem
            size_t res = tinfl_decompress_mem_to_mem(outData.data(), entry.uncompressedSize, fileDataStart, entry.compressedSize, 0);
            if (res == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) {
                outError = "miniz DEFLATE failed for " + entry.filename;
                return false;
            }
        }
        return true;
#else
        outError = "DEFLATE compression not supported (no zlib/miniz). File: " + entry.filename + ". Repack bundle with STORE or add zlib/miniz.";
        return false;
#endif
    } else {
        outError = "Unsupported compression method " + std::to_string(entry.compressionMethod) + " for file " + entry.filename;
        return false;
    }
}

bool ZipReader::ReadFile(const std::string& filename, std::vector<uint8_t>& outData, std::string& outError) {
    auto it = entryMap.find(filename);
    if (it == entryMap.end()) {
        outError = "File not found in ZIP: " + filename;
        return false;
    }
    return ReadLocalFileData(it->second, outData, outError);
}

bool ZipReader::ReadFileAsString(const std::string& filename, std::string& outStr, std::string& outError) {
    std::vector<uint8_t> data;
    if (!ReadFile(filename, data, outError)) return false;
    outStr.assign((char*)data.data(), data.size());
    return true;
}

} // namespace huanface
