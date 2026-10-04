/* miniz.c minimal implementation for HuanFace Phase 4
 * Uses zlib for DEFLATE on Linux, and provides tinfl fallback
 */

#include "miniz.h"
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

void *miniz_def_alloc_func(void *opaque, size_t items, size_t size) { (void)opaque; return malloc(items*size); }
void miniz_def_free_func(void *opaque, void *address) { (void)opaque; free(address); }
void *miniz_def_realloc_func(void *opaque, void *address, size_t items, size_t size) { (void)opaque; return realloc(address, items*size); }

const char *mz_version(void) { return "miniz minimal 1.0 HuanFace"; }

unsigned long mz_crc32(unsigned long crc, const unsigned char *ptr, size_t buf_len) {
#if HF_USE_ZLIB_FOR_MINIZ
    return crc32(crc, ptr, (uInt)buf_len);
#else
    // Simple CRC32
    static uint32_t table[256];
    static int have_table=0;
    if (!have_table) {
        for (int i=0;i<256;++i) {
            uint32_t c=i;
            for (int j=0;j<8;++j) c = (c>>1) ^ (0xEDB88320 & -(c&1));
            table[i]=c;
        }
        have_table=1;
    }
    crc = ~crc;
    for (size_t i=0;i<buf_len;++i) crc = table[(crc ^ ptr[i]) & 0xFF] ^ (crc>>8);
    return ~crc;
#endif
}

#if HF_USE_ZLIB_FOR_MINIZ
// Use zlib for inflate
int mz_inflateInit(mz_streamp pStream) { return mz_inflateInit2(pStream, MZ_DEFAULT_WINDOW_BITS); }
int mz_inflateInit2(mz_streamp pStream, int window_bits) {
    if (!pStream) return MZ_DATA_ERROR;
    z_stream *zs = (z_stream*)malloc(sizeof(z_stream));
    if (!zs) return MZ_MEM_ERROR;
    memset(zs,0,sizeof(z_stream));
    zs->zalloc = Z_NULL;
    zs->zfree = Z_NULL;
    zs->opaque = Z_NULL;
    int ret = inflateInit2(zs, window_bits);
    if (ret!=Z_OK) { free(zs); return ret; }
    pStream->state = (void*)zs;
    pStream->total_in=0; pStream->total_out=0;
    return MZ_OK;
}
int mz_inflate(mz_streamp pStream, int flush) {
    if (!pStream || !pStream->state) return MZ_DATA_ERROR;
    z_stream *zs = (z_stream*)pStream->state;
    zs->next_in = (Bytef*)pStream->next_in;
    zs->avail_in = pStream->avail_in;
    zs->next_out = pStream->next_out;
    zs->avail_out = pStream->avail_out;
    int ret = inflate(zs, flush);
    pStream->next_in = zs->next_in;
    pStream->avail_in = zs->avail_in;
    pStream->total_in = zs->total_in;
    pStream->next_out = zs->next_out;
    pStream->avail_out = zs->avail_out;
    pStream->total_out = zs->total_out;
    if (ret==Z_STREAM_END) return MZ_STREAM_END;
    if (ret==Z_OK) return MZ_OK;
    if (ret==Z_BUF_ERROR) return MZ_BUF_ERROR;
    return MZ_DATA_ERROR;
}
int mz_inflateEnd(mz_streamp pStream) {
    if (!pStream || !pStream->state) return MZ_DATA_ERROR;
    z_stream *zs = (z_stream*)pStream->state;
    inflateEnd(zs);
    free(zs);
    pStream->state=NULL;
    return MZ_OK;
}
int mz_uncompress(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len) {
    uLongf destLen = *pDest_len;
    int ret = uncompress(pDest, &destLen, pSource, source_len);
    *pDest_len = destLen;
    if (ret==Z_OK) return MZ_OK;
    if (ret==Z_BUF_ERROR) return MZ_BUF_ERROR;
    return MZ_DATA_ERROR;
}
size_t tinfl_decompress_mem_to_mem(void *pOut_buf, size_t out_buf_len, const void *pSrc_buf, size_t src_buf_len, int flags) {
    // Use zlib for raw deflate if flags==0 (raw), else zlib header
    int window_bits = (flags & TINFL_FLAG_PARSE_ZLIB_HEADER) ? MZ_DEFAULT_WINDOW_BITS : -MZ_DEFAULT_WINDOW_BITS;
    z_stream zs;
    memset(&zs,0,sizeof(zs));
    zs.next_in = (Bytef*)pSrc_buf;
    zs.avail_in = (uInt)src_buf_len;
    zs.next_out = (Bytef*)pOut_buf;
    zs.avail_out = (uInt)out_buf_len;
    int ret = inflateInit2(&zs, window_bits);
    if (ret!=Z_OK) return TINFL_DECOMPRESS_MEM_TO_MEM_FAILED;
    ret = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    if (ret!=Z_STREAM_END) return TINFL_DECOMPRESS_MEM_TO_MEM_FAILED;
    return zs.total_out;
}
void *tinfl_decompress_mem_to_heap(const void *pSrc_buf, size_t src_buf_len, size_t *pOut_len, int flags) {
    size_t out_len = src_buf_len*3;
    void *out = malloc(out_len);
    if (!out) return NULL;
    size_t res = tinfl_decompress_mem_to_mem(out, out_len, pSrc_buf, src_buf_len, flags);
    if (res==TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) { free(out); return NULL; }
    *pOut_len = res;
    return out;
}
tinfl_status tinfl_decompress(tinfl_decompressor *r, const mz_uint8 *pIn_buf_next, size_t *pIn_buf_size, mz_uint8 *pOut_buf_start, mz_uint8 *pOut_buf_next, size_t *pOut_buf_size, const uint32_t decomp_flags) {
    (void)r; (void)pOut_buf_start;
    size_t res = tinfl_decompress_mem_to_mem(pOut_buf_next, *pOut_buf_size, pIn_buf_next, *pIn_buf_size, decomp_flags);
    if (res==TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) return TINFL_STATUS_FAILED;
    *pOut_buf_size = res;
    *pIn_buf_size = *pIn_buf_size; // consume all
    return TINFL_STATUS_DONE;
}
#else
// Windows tinfl fallback - minimal stub that fails (would need real tinfl implementation)
// For Phase 4, we provide simple implementation that returns failed, but on Windows we would have real tinfl.c
// To keep minimal, we return failed and rely on zlib if available via vcpkg
int mz_inflateInit(mz_streamp p) { (void)p; return MZ_DATA_ERROR; }
int mz_inflateInit2(mz_streamp p, int w) { (void)p;(void)w; return MZ_DATA_ERROR; }
int mz_inflate(mz_streamp p, int f) { (void)p;(void)f; return MZ_DATA_ERROR; }
int mz_inflateEnd(mz_streamp p) { (void)p; return MZ_DATA_ERROR; }
int mz_uncompress(unsigned char *d, mz_ulong *dl, const unsigned char *s, mz_ulong sl) { (void)d;(void)dl;(void)s;(void)sl; return MZ_DATA_ERROR; }
size_t tinfl_decompress_mem_to_mem(void *o, size_t ol, const void *s, size_t sl, int f) { (void)o;(void)ol;(void)s;(void)sl;(void)f; return TINFL_DECOMPRESS_MEM_TO_MEM_FAILED; }
void *tinfl_decompress_mem_to_heap(const void *s, size_t sl, size_t *ol, int f) { (void)s;(void)sl;(void)ol;(void)f; return NULL; }
tinfl_status tinfl_decompress(tinfl_decompressor *r, const mz_uint8 *in, size_t *ins, mz_uint8 *out_start, mz_uint8 *out_next, size_t *outs, const uint32_t flags) { (void)r;(void)in;(void)ins;(void)out_start;(void)out_next;(void)outs;(void)flags; return TINFL_STATUS_FAILED; }
#endif

#ifdef __cplusplus
}
#endif
