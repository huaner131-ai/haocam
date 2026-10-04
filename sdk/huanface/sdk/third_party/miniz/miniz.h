/* miniz.h vendored minimal for HuanFace Phase 4
 * Provides DEFLATE via zlib (Linux) and tinfl (Windows) — public domain
 * Original miniz by Rich Geldreich, this is minimal wrapper
 */

#pragma once
#include "miniz_export.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Minimal types */
typedef unsigned char mz_uint8;
typedef uint16_t mz_uint16;
typedef uint32_t mz_uint32;
typedef uint32_t mz_uint;
typedef uint64_t mz_uint64;
typedef int mz_bool;
#define MZ_FALSE 0
#define MZ_TRUE 1

/* Error codes compatible with zlib */
#define MZ_OK 0
#define MZ_STREAM_END 1
#define MZ_DATA_ERROR -3
#define MZ_MEM_ERROR -4
#define MZ_BUF_ERROR -5

#define TINFL_LZ_DICT_SIZE 32768
#define TINFL_DECOMPRESS_MEM_TO_MEM_FAILED ((size_t)(-1))

/* Flags */
enum {
    TINFL_FLAG_PARSE_ZLIB_HEADER = 1,
    TINFL_FLAG_HAS_MORE_INPUT = 2,
    TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF = 4,
    TINFL_FLAG_COMPUTE_ADLER32 = 8
};

/* Status */
typedef enum {
    TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS = -4,
    TINFL_STATUS_BAD_PARAM = -3,
    TINFL_STATUS_ADLER32_MISMATCH = -2,
    TINFL_STATUS_FAILED = -1,
    TINFL_STATUS_DONE = 0,
    TINFL_STATUS_NEEDS_MORE_INPUT = 1,
    TINFL_STATUS_HAS_MORE_OUTPUT = 2
} tinfl_status;

/* Decompressor */
typedef struct tinfl_decompressor_tag {
    uint32_t m_state, m_num_bits, m_zhdr0, m_zhdr1, m_z_adler32, m_final, m_type, m_check_adler32, m_dist, m_counter, m_num_extra, m_table_sizes[3];
    uint32_t m_bit_buf;
    uint32_t m_dist_from_out_buf_start;
    int16_t m_look_up[3][1024];
    int16_t m_tree_0[288*2];
    int16_t m_tree_1[32*2];
    int16_t m_tree_2[19*2];
    uint8_t m_code_size_0[288];
    uint8_t m_code_size_1[32];
    uint8_t m_code_size_2[19];
    uint8_t m_raw_header[4], m_len_codes[288+32+137];
} tinfl_decompressor;

#define tinfl_init(r) do { (r)->m_state = 0; } while(0)
#define tinfl_get_adler32(r) (r)->m_check_adler32

MINIZ_EXPORT tinfl_status tinfl_decompress(tinfl_decompressor *r, const mz_uint8 *pIn_buf_next, size_t *pIn_buf_size, mz_uint8 *pOut_buf_start, mz_uint8 *pOut_buf_next, size_t *pOut_buf_size, const uint32_t decomp_flags);
MINIZ_EXPORT void *tinfl_decompress_mem_to_heap(const void *pSrc_buf, size_t src_buf_len, size_t *pOut_len, int flags);
MINIZ_EXPORT size_t tinfl_decompress_mem_to_mem(void *pOut_buf, size_t out_buf_len, const void *pSrc_buf, size_t src_buf_len, int flags);

/* zlib-style API using miniz */
typedef struct mz_stream_s {
    const unsigned char *next_in;
    unsigned int avail_in;
    unsigned long total_in;
    unsigned char *next_out;
    unsigned int avail_out;
    unsigned long total_out;
    char *msg;
    void *state;
    void *(*zalloc)(void *opaque, size_t items, size_t size);
    void (*zfree)(void *opaque, void *address);
    void *opaque;
    int data_type;
    unsigned long adler;
    unsigned long reserved;
} mz_stream;
typedef mz_stream *mz_streamp;
typedef unsigned long mz_ulong;

MINIZ_EXPORT int mz_inflateInit(mz_streamp pStream);
MINIZ_EXPORT int mz_inflateInit2(mz_streamp pStream, int window_bits);
MINIZ_EXPORT int mz_inflate(mz_streamp pStream, int flush);
MINIZ_EXPORT int mz_inflateEnd(mz_streamp pStream);
MINIZ_EXPORT int mz_uncompress(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len);
MINIZ_EXPORT unsigned long mz_crc32(unsigned long crc, const unsigned char *ptr, size_t buf_len);
MINIZ_EXPORT const char *mz_version(void);

#define MZ_DEFAULT_WINDOW_BITS 15
#define MZ_FINISH 4
#define Z_OK MZ_OK
#define Z_STREAM_END MZ_STREAM_END
#define Z_DATA_ERROR MZ_DATA_ERROR
#define Z_BUF_ERROR MZ_BUF_ERROR
#define MAX_WBITS 15

#ifdef __cplusplus
}
#endif

/* Include tinfl implementation if not using zlib */
#ifndef HF_USE_ZLIB_FOR_MINIZ
/* For Linux we use zlib, for Windows we use tinfl implementation */
#ifdef __linux__
#define HF_USE_ZLIB_FOR_MINIZ 1
#else
#define HF_USE_ZLIB_FOR_MINIZ 0
#endif
#endif

#if HF_USE_ZLIB_FOR_MINIZ
/* Use zlib for actual decompression */
#include <zlib.h>
#endif
