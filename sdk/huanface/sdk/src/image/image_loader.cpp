/**
 * HuanFace Image Loader Implementation — Phase 4
 * Minimal PNG loader via zlib (real), BMP loader simple, PNG saver via zlib
 * No stb_image dependency (network blocked), clean-room
 * Uses zlib for DEFLATE (same as ZipReader)
 */

#include "../../include/huanface/huanface_image.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cstdint>

#if __has_include(<zlib.h>)
#include <zlib.h>
#define HF_HAS_ZLIB 1
#else
#define HF_HAS_ZLIB 0
#include "../third_party/miniz/miniz.h"
#endif

namespace huanface {

static uint32_t ReadU32BE(const uint8_t* p) {
    return (uint32_t)((p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3]);
}
static uint32_t ReadU32LE(const uint8_t* p) {
    return (uint32_t)(p[0]|(p[1]<<8)|(p[2]<<16)|(p[3]<<24));
}
static uint16_t ReadU16LE(const uint8_t* p) {
    return (uint16_t)(p[0]|(p[1]<<8));
}

bool ImageLoader::IsPNG(const uint8_t* data, size_t size) {
    if (size < 8) return false;
    static const uint8_t sig[8] = {137,80,78,71,13,10,26,10};
    return memcmp(data, sig, 8)==0;
}
bool ImageLoader::IsBMP(const uint8_t* data, size_t size) {
    if (size < 2) return false;
    return data[0]=='B' && data[1]=='M';
}
bool ImageLoader::IsJPG(const uint8_t* data, size_t size) {
    if (size < 3) return false;
    return data[0]==0xFF && data[1]==0xD8 && data[2]==0xFF;
}

// PNG chunk handling
struct PNGChunk {
    uint32_t length;
    uint32_t type; // 4 chars
    const uint8_t* data;
    uint32_t crc;
};

// Minimal PNG loader: supports 8-bit RGB, RGBA, grayscale, palette? We support RGBA8, RGB8, grayscale
// Returns RGBA8
static bool DecompressZlib(const uint8_t* compData, size_t compSize, std::vector<uint8_t>& outDecomp, size_t expectedSize, std::string& outError) {
#if HF_HAS_ZLIB
    // compData is zlib stream (has header) for PNG IDAT
    outDecomp.resize(expectedSize);
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    strm.next_in = (Bytef*)compData;
    strm.avail_in = (uInt)compSize;
    strm.next_out = outDecomp.data();
    strm.avail_out = (uInt)expectedSize;
    int ret = inflateInit(&strm); // zlib header
    if (ret != Z_OK) {
        outError = "inflateInit failed PNG";
        return false;
    }
    ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);
    if (ret != Z_STREAM_END) {
        // Try raw deflate as fallback
        outDecomp.resize(expectedSize);
        memset(&strm, 0, sizeof(strm));
        strm.next_in = (Bytef*)compData;
        strm.avail_in = (uInt)compSize;
        strm.next_out = outDecomp.data();
        strm.avail_out = (uInt)expectedSize;
        ret = inflateInit2(&strm, -MAX_WBITS);
        if (ret != Z_OK) {
            outError = "inflateInit2 failed PNG ret="+std::to_string(ret);
            return false;
        }
        ret = inflate(&strm, Z_FINISH);
        inflateEnd(&strm);
        if (ret != Z_STREAM_END) {
            outError = "PNG inflate failed ret="+std::to_string(ret);
            return false;
        }
    }
    // Resize to actual decompressed size if needed
    if (strm.total_out != expectedSize) {
        // Allow if smaller? For PNG we expect exact
        outDecomp.resize(strm.total_out);
    }
    return true;
#else
    // Fallback to simple stored deflate (no compression) for Windows without zlib
    // Parse zlib header 0x78 0x01, then stored blocks
    if(compSize < 6){ outError="PNG comp too small"; return false; }
    // Check zlib header (CMF 0x78, FLG 0x01 or 0x9C etc) — we accept any
    size_t pos=2; // skip zlib header
    outDecomp.clear();
    outDecomp.reserve(expectedSize);
    uint32_t adler=1;
    // Simple adler not checked for now
    while(pos+5 <= compSize){
        uint8_t bfinal = compData[pos] & 1;
        uint8_t btype = (compData[pos]>>1) & 3;
        if(btype!=0){
            // Only support stored blocks for our simple encoder
            // Try to fallback to miniz if available
            // For simplicity, if not stored, fail and try miniz uncompress if possible
            break;
        }
        // Stored block: next byte is padding, then LEN, NLEN
        // Actually stored block header is 1 byte with BFINAL+BTYPE+padding, then 2 bytes LEN, 2 bytes NLEN
        // We are at byte pos, we have already read bits, but for stored, the header is 1 byte, then 2 bytes LEN LE, 2 bytes NLEN LE
        pos++; // skip header byte (we already checked bfinal/btype, but need to handle bit alignment — stored blocks are byte aligned after header)
        if(pos+4 > compSize){ outError="PNG stored block truncated"; return false; }
        uint16_t len = compData[pos] | (compData[pos+1]<<8);
        uint16_t nlen = compData[pos+2] | (compData[pos+3]<<8);
        if((len ^ nlen)!=0xFFFF){ outError="PNG stored block LEN/NLEN mismatch"; return false; }
        pos+=4;
        if(pos+len > compSize){ outError="PNG stored block data out of bounds"; return false; }
        outDecomp.insert(outDecomp.end(), compData+pos, compData+pos+len);
        pos+=len;
        if(bfinal) break;
    }
    if(outDecomp.size()==0){
        // Try miniz uncompress as fallback
        mz_ulong decompLen = (mz_ulong)expectedSize;
        outDecomp.resize(expectedSize);
        int retMz = mz_uncompress(outDecomp.data(), &decompLen, compData, (mz_ulong)compSize);
        if(retMz==MZ_OK){
            outDecomp.resize(decompLen);
            return true;
        }
        // Try raw deflate via tinfl
        size_t outLen = expectedSize;
        size_t inLen = compSize;
        // Use tinfl_decompress_mem_to_mem with parse zlib header flag
        size_t res = tinfl_decompress_mem_to_mem(outDecomp.data(), outLen, compData, compSize, TINFL_FLAG_PARSE_ZLIB_HEADER);
        if(res!=TINFL_DECOMPRESS_MEM_TO_MEM_FAILED){
            outDecomp.resize(res);
            return true;
        }
        outError="PNG decompress failed: stored blocks 0 and miniz/tinfl failed";
        return false;
    }
    if(outDecomp.size()!=expectedSize && expectedSize>0){
        // Allow resize if expected was estimate
        if(outDecomp.size()>expectedSize){
            outError="PNG decompressed size larger than expected";
            return false;
        }
    }
    return true;
#endif
}

static uint32_t Adler32(const uint8_t* data, size_t len){
    uint32_t a=1,b=0;
    for(size_t i=0;i<len;++i){ a=(a+data[i])%65521; b=(b+a)%65521; }
    return (b<<16)|a;
}

static bool CompressZlib(const uint8_t* src, size_t srcSize, std::vector<uint8_t>& outComp, std::string& outError) {
#if HF_HAS_ZLIB
    uLong bound = compressBound((uLong)srcSize);
    outComp.resize(bound);
    uLong destLen = bound;
    int ret = compress2(outComp.data(), &destLen, src, (uLong)srcSize, Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK) {
        outError = "compress2 failed ret="+std::to_string(ret);
        return false;
    }
    outComp.resize(destLen);
    return true;
#else
    // Simple stored deflate (no compression) with zlib wrapper for Windows without zlib
    outComp.clear();
    outComp.reserve(srcSize + 6 + (srcSize/65535+1)*5 + 6);
    // zlib header 0x78 0x01 (no compression, fastest)
    outComp.push_back(0x78);
    outComp.push_back(0x01);
    size_t pos=0;
    while(pos < srcSize){
        size_t blockSize = std::min<size_t>(srcSize-pos, 65535);
        bool isLast = (pos+blockSize >= srcSize);
        uint8_t header = isLast ? 1 : 0; // BFINAL=1 if last, BTYPE=00
        outComp.push_back(header);
        uint16_t len = (uint16_t)blockSize;
        uint16_t nlen = (uint16_t)~len;
        outComp.push_back(len & 0xFF);
        outComp.push_back((len>>8)&0xFF);
        outComp.push_back(nlen & 0xFF);
        outComp.push_back((nlen>>8)&0xFF);
        outComp.insert(outComp.end(), src+pos, src+pos+blockSize);
        pos+=blockSize;
    }
    uint32_t adler = Adler32(src, srcSize);
    outComp.push_back((adler>>24)&0xFF);
    outComp.push_back((adler>>16)&0xFF);
    outComp.push_back((adler>>8)&0xFF);
    outComp.push_back(adler&0xFF);
    return true;
#endif
}

bool ImageLoader::LoadImageFromMemory(const uint8_t* fileData, size_t fileSize, HFImage& outImage, std::string& outError) {
    if (!fileData || fileSize < 8) {
        outError = "Invalid file data";
        return false;
    }
    if (IsPNG(fileData, fileSize)) {
        // Parse PNG
        const uint8_t* p = fileData + 8;
        size_t remaining = fileSize - 8;
        uint32_t width = 0, height = 0;
        uint8_t bitDepth = 0, colorType = 0, interlace = 0;
        std::vector<uint8_t> idatCompressed;
        bool foundIHDR = false;
        bool foundIEND = false;

        while (remaining >= 12) { // at least length+type+crc
            uint32_t chunkLen = ReadU32BE(p);
            if (chunkLen > remaining) {
                outError = "PNG chunk length out of bounds";
                return false;
            }
            uint32_t chunkType = ReadU32BE(p+4);
            const uint8_t* chunkData = p+8;
            // uint32_t chunkCrc = ReadU32BE(p+8+chunkLen);
            // Type as string
            char typeStr[5];
            typeStr[0]=(char)((chunkType>>24)&0xFF);
            typeStr[1]=(char)((chunkType>>16)&0xFF);
            typeStr[2]=(char)((chunkType>>8)&0xFF);
            typeStr[3]=(char)(chunkType&0xFF);
            typeStr[4]=0;

            if (strcmp(typeStr,"IHDR")==0) {
                if (chunkLen != 13) {
                    outError = "IHDR length invalid";
                    return false;
                }
                width = ReadU32BE(chunkData);
                height = ReadU32BE(chunkData+4);
                bitDepth = chunkData[8];
                colorType = chunkData[9];
                // compression 0, filter 0, interlace
                interlace = chunkData[12];
                if (width==0 || height==0 || width>16384 || height>16384) {
                    outError = "Invalid PNG dimensions";
                    return false;
                }
                if (bitDepth != 8) {
                    outError = "Only 8-bit PNG supported, got bitDepth="+std::to_string(bitDepth);
                    return false;
                }
                if (interlace != 0) {
                    outError = "Interlaced PNG not supported";
                    return false;
                }
                if (colorType!=2 && colorType!=6 && colorType!=0 && colorType!=3) {
                    outError = "Unsupported PNG colorType="+std::to_string(colorType)+" (only RGB=2, RGBA=6, Gray=0 supported)";
                    return false;
                }
                foundIHDR = true;
            } else if (strcmp(typeStr,"IDAT")==0) {
                // Append
                size_t oldSize = idatCompressed.size();
                idatCompressed.resize(oldSize + chunkLen);
                memcpy(idatCompressed.data()+oldSize, chunkData, chunkLen);
            } else if (strcmp(typeStr,"IEND")==0) {
                foundIEND = true;
                break;
            }
            // Move to next chunk: 4 len +4 type + chunkLen +4 crc = 12+chunkLen
            size_t totalChunk = 12 + chunkLen;
            if (totalChunk > remaining) {
                outError = "PNG chunk overflow";
                return false;
            }
            p += totalChunk;
            remaining -= totalChunk;
        }

        if (!foundIHDR) {
            outError = "PNG IHDR not found";
            return false;
        }
        if (idatCompressed.empty()) {
            outError = "PNG IDAT not found";
            return false;
        }

        // Decompress IDAT
        // Each scanline has filter byte + data
        // For RGB: 3*width, RGBA: 4*width, Gray: 1*width
        int channels = 0;
        if (colorType==6) channels=4;
        else if (colorType==2) channels=3;
        else if (colorType==0) channels=1;
        else if (colorType==3) {
            outError = "Palette PNG not supported in minimal loader (use RGBA)";
            return false;
        }
        size_t stride = (size_t)width * channels;
        size_t expectedDecomp = (stride + 1) * height; // +1 filter per line
        std::vector<uint8_t> decomp;
        if (!DecompressZlib(idatCompressed.data(), idatCompressed.size(), decomp, expectedDecomp, outError)) {
            return false;
        }
        if (decomp.size() < expectedDecomp) {
            // Some PNG encoders may have different size? Try to handle
            // But we expect exact
            if (decomp.size() < (stride+1)*height) {
                outError = "PNG decompressed size too small";
                return false;
            }
        }

        // Unfilter
        outImage.width = (int)width;
        outImage.height = (int)height;
        outImage.channels = 4;
        outImage.data.resize((size_t)width*height*4);

        const uint8_t* src = decomp.data();
        std::vector<uint8_t> prevLine(stride, 0);
        std::vector<uint8_t> curLine(stride);

        for (uint32_t y=0; y<height; ++y) {
            uint8_t filterType = src[0];
            const uint8_t* lineData = src+1;
            // Copy to curLine for unfiltering
            // Apply filter
            switch (filterType) {
                case 0: // None
                    memcpy(curLine.data(), lineData, stride);
                    break;
                case 1: // Sub
                    for (size_t i=0;i<stride;++i) {
                        uint8_t left = (i>= (size_t)channels) ? curLine[i-channels] : 0;
                        curLine[i] = lineData[i] + left;
                    }
                    break;
                case 2: // Up
                    for (size_t i=0;i<stride;++i) {
                        curLine[i] = lineData[i] + prevLine[i];
                    }
                    break;
                case 3: // Average
                    for (size_t i=0;i<stride;++i) {
                        uint8_t left = (i>= (size_t)channels) ? curLine[i-channels] : 0;
                        uint8_t up = prevLine[i];
                        curLine[i] = lineData[i] + (uint8_t)((left+up)/2);
                    }
                    break;
                case 4: // Paeth
                    for (size_t i=0;i<stride;++i) {
                        uint8_t left = (i>= (size_t)channels) ? curLine[i-channels] : 0;
                        uint8_t up = prevLine[i];
                        uint8_t upLeft = (i>= (size_t)channels) ? prevLine[i-channels] : 0;
                        // Paeth predictor
                        int p = (int)left + (int)up - (int)upLeft;
                        int pa = abs(p - (int)left);
                        int pb = abs(p - (int)up);
                        int pc = abs(p - (int)upLeft);
                        uint8_t pr;
                        if (pa <= pb && pa <= pc) pr = left;
                        else if (pb <= pc) pr = up;
                        else pr = upLeft;
                        curLine[i] = lineData[i] + pr;
                    }
                    break;
                default:
                    outError = "Unsupported PNG filter type "+std::to_string(filterType);
                    return false;
            }

            // Convert curLine to RGBA8
            for (uint32_t x=0;x<width;++x) {
                uint8_t r=0,g=0,b=0,a=255;
                if (channels==4) {
                    r = curLine[x*4+0];
                    g = curLine[x*4+1];
                    b = curLine[x*4+2];
                    a = curLine[x*4+3];
                } else if (channels==3) {
                    r = curLine[x*3+0];
                    g = curLine[x*3+1];
                    b = curLine[x*3+2];
                    a = 255;
                } else if (channels==1) {
                    r = g = b = curLine[x];
                    a = 255;
                }
                size_t dstIdx = ((size_t)y*width + x)*4;
                outImage.data[dstIdx+0]=r;
                outImage.data[dstIdx+1]=g;
                outImage.data[dstIdx+2]=b;
                outImage.data[dstIdx+3]=a;
            }

            prevLine = curLine;
            src += stride + 1;
        }

        return true;
    } else if (IsBMP(fileData, fileSize)) {
        // Minimal BMP loader: only 24-bit uncompressed
        if (fileSize < 54) {
            outError = "BMP too small";
            return false;
        }
        uint32_t dataOffset = ReadU32LE(fileData+10);
        uint32_t headerSize = ReadU32LE(fileData+14);
        int32_t w = (int32_t)ReadU32LE(fileData+18);
        int32_t h = (int32_t)ReadU32LE(fileData+22);
        uint16_t planes = ReadU16LE(fileData+26);
        uint16_t bpp = ReadU16LE(fileData+28);
        uint32_t compression = ReadU32LE(fileData+30);
        if (planes!=1 || bpp!=24 || compression!=0) {
            outError = "Only 24-bit uncompressed BMP supported";
            return false;
        }
        if (w<=0 || h==0) {
            outError = "Invalid BMP dimensions";
            return false;
        }
        bool topDown = false;
        if (h<0) {
            topDown = true;
            h = -h;
        }
        if (w>8192 || h>8192) {
            outError = "BMP too large";
            return false;
        }
        // Row padded to 4 bytes
        int rowSize = ((w*3 + 3)/4)*4;
        if (dataOffset + (size_t)rowSize*h > fileSize) {
            outError = "BMP data out of bounds";
            return false;
        }
        outImage.width = w;
        outImage.height = h;
        outImage.channels = 4;
        outImage.data.resize((size_t)w*h*4);
        const uint8_t* bmpData = fileData + dataOffset;
        for (int y=0;y<h;++y) {
            int srcY = topDown ? y : (h-1-y);
            const uint8_t* srcRow = bmpData + srcY*rowSize;
            for (int x=0;x<w;++x) {
                uint8_t b = srcRow[x*3+0];
                uint8_t g = srcRow[x*3+1];
                uint8_t r = srcRow[x*3+2];
                size_t dstIdx = ((size_t)y*w + x)*4;
                outImage.data[dstIdx+0]=r;
                outImage.data[dstIdx+1]=g;
                outImage.data[dstIdx+2]=b;
                outImage.data[dstIdx+3]=255;
            }
        }
        return true;
    } else if (IsJPG(fileData, fileSize)) {
        outError = "JPG not supported in minimal loader (use PNG). For Phase 4 minimal, only PNG/BMP.";
        return false;
    } else {
        outError = "Unknown image format (not PNG/BMP/JPG)";
        return false;
    }
}

bool ImageLoader::LoadImage(const std::string& path, HFImage& outImage, std::string& outError) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        outError = "Failed to open image file: "+path;
        return false;
    }
    file.seekg(0, std::ios::end);
    size_t size = (size_t)file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    file.read((char*)buffer.data(), size);
    if ((size_t)file.gcount()!=size) {
        outError = "Failed to read image file: "+path;
        return false;
    }
    return LoadImageFromMemory(buffer.data(), buffer.size(), outImage, outError);
}

// PNG encoding: create PNG file from RGBA8
static uint32_t CRC32(const uint8_t* data, size_t len) {
#if HF_HAS_ZLIB
    return (uint32_t)crc32(0, data, (uInt)len);
#else
    // Simple CRC32 fallback (not accurate but for minimal)
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i=0;i<len;++i) {
        crc ^= data[i];
        for (int j=0;j<8;++j) crc = (crc>>1) ^ (0xEDB88320 & -(crc&1));
    }
    return ~crc;
#endif
}

static void WriteU32BE(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((uint8_t)((v>>24)&0xFF));
    out.push_back((uint8_t)((v>>16)&0xFF));
    out.push_back((uint8_t)((v>>8)&0xFF));
    out.push_back((uint8_t)(v&0xFF));
}
static void WriteChunk(std::vector<uint8_t>& out, const char* type, const uint8_t* data, size_t len) {
    WriteU32BE(out, (uint32_t)len);
    size_t typePos = out.size();
    out.push_back(type[0]); out.push_back(type[1]); out.push_back(type[2]); out.push_back(type[3]);
    if (data && len>0) out.insert(out.end(), data, data+len);
    uint32_t crc = CRC32(out.data()+typePos, 4+len);
    WriteU32BE(out, crc);
}

bool ImageLoader::EncodePNGToMemory(int width, int height, const uint8_t* rgbaData, std::vector<uint8_t>& outPng, std::string& outError) {
    if (width<=0 || height<=0 || !rgbaData) {
        outError = "Invalid params for PNG encode";
        return false;
    }
    outPng.clear();
    // PNG signature
    static const uint8_t sig[8]={137,80,78,71,13,10,26,10};
    outPng.insert(outPng.end(), sig, sig+8);

    // IHDR
    std::vector<uint8_t> ihdr;
    WriteU32BE(ihdr, (uint32_t)width);
    WriteU32BE(ihdr, (uint32_t)height);
    ihdr.push_back(8); // bit depth
    ihdr.push_back(6); // color type RGBA
    ihdr.push_back(0); // compression
    ihdr.push_back(0); // filter
    ihdr.push_back(0); // interlace
    WriteChunk(outPng, "IHDR", ihdr.data(), ihdr.size());

    // IDAT: filter + data
    size_t stride = (size_t)width*4;
    std::vector<uint8_t> raw;
    raw.reserve((stride+1)*height);
    for (int y=0;y<height;++y) {
        raw.push_back(0); // filter none
        const uint8_t* row = rgbaData + (size_t)y*stride;
        raw.insert(raw.end(), row, row+stride);
    }
    std::vector<uint8_t> comp;
    if (!CompressZlib(raw.data(), raw.size(), comp, outError)) return false;
    WriteChunk(outPng, "IDAT", comp.data(), comp.size());

    // IEND
    WriteChunk(outPng, "IEND", nullptr, 0);
    return true;
}

bool ImageLoader::SaveImageAsPNG(const std::string& path, int width, int height, const uint8_t* rgbaData, std::string& outError) {
    std::vector<uint8_t> png;
    if (!EncodePNGToMemory(width, height, rgbaData, png, outError)) return false;
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        outError = "Failed to open file for writing: "+path;
        return false;
    }
    file.write((char*)png.data(), png.size());
    if (!file) {
        outError = "Failed to write PNG file: "+path;
        return false;
    }
    return true;
}

bool ImageLoader::SaveImage(const std::string& path, const HFImage& image, std::string& outError) {
    if (!image.IsValid()) {
        outError = "Invalid image for saving";
        return false;
    }
    // Determine extension, but we only support PNG for now
    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.size()>=4 && lower.substr(lower.size()-4)==".png") {
        return SaveImageAsPNG(path, image.width, image.height, image.data.data(), outError);
    } else if (lower.size()>=4 && (lower.substr(lower.size()-4)==".bmp")) {
        outError = "BMP saving not implemented, use PNG";
        return false;
    } else {
        // Default to PNG even if extension not png, for minimal
        return SaveImageAsPNG(path, image.width, image.height, image.data.data(), outError);
    }
}

} // namespace huanface
