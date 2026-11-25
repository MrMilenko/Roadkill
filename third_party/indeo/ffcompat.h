/*
 * ffcompat.h: the small pieces of FFmpeg's libavutil / libavcodec framework the Indeo 3 and
 * Indeo 4 decoders use (bit reader, VLC tables, byte readers, AVCodecContext / AVFrame /
 * AVPacket stand ins, memory and helper macros), so the decoders build on their own.
 *
 * Written for Roadkill after FFmpeg's libavutil/intreadwrite.h, macros.h, common.h, mem.h and
 * libavcodec/get_bits.h, bytestream.h, vlc.h, copy_block.h, hpeldsp.h (FFmpeg 7.1), whose license
 * this file keeps:
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Differences from FFmpeg: the bit reader is a plain (checked, padding free) reader that reads
 * zeros past the end of its buffer; get_vlc2 is written out as a function; av_log prints only
 * when INDEO_LOG is set in the environment; one AVFrame owns one YUV 4:1:0 picture.
 */
#ifndef INDEO_FFCOMPAT_H
#define INDEO_FFCOMPAT_H

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ---- configuration (FFmpeg's config.h) ---- */
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define HAVE_BIGENDIAN 1
#else
#define HAVE_BIGENDIAN 0
#endif

/* ---- tables (libavcodec/mathtables.c) ---- */
extern const uint8_t ff_zigzag_direct[64];

/* ---- attributes and macros (libavutil/attributes.h, macros.h, common.h) ---- */
#define av_cold
#define av_unused __attribute__((unused))
#define av_always_inline inline __attribute__((always_inline))
#define av_noinline __attribute__((noinline))
#define DECLARE_ALIGNED(n, t, v) t __attribute__((aligned(n))) v

#define FFMIN(a, b) ((a) > (b) ? (b) : (a))
#define FFMAX(a, b) ((a) > (b) ? (a) : (b))
#define FFMIN3(a, b, c) FFMIN(FFMIN(a, b), c)
#define FFMAX3(a, b, c) FFMAX(FFMAX(a, b), c)
#define FFABS(a) ((a) >= 0 ? (a) : (-(a)))
#define FFSIGN(a) ((a) > 0 ? 1 : -1)
#define FFSWAP(type, a, b) do { type SWAP_tmp = b; b = a; a = SWAP_tmp; } while (0)
#define FFALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))
#define FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))
#define MKBETAG(a, b, c, d) ((unsigned)(d) | ((unsigned)(c) << 8) | ((unsigned)(b) << 16) | ((unsigned)(a) << 24))
#define MKTAG(a, b, c, d) ((unsigned)(a) | ((unsigned)(b) << 8) | ((unsigned)(c) << 16) | ((unsigned)(d) << 24))

static inline uint8_t av_clip_uint8(int a) { return (a & (~0xFF)) ? (uint8_t)((~a) >> 31) : (uint8_t)a; }
static inline int av_clip(int a, int amin, int amax) { return a < amin ? amin : a > amax ? amax : a; }
static inline unsigned av_clip_uintp2(int a, int p)
{
    if (a & ~((1 << p) - 1)) return (unsigned)((~a) >> 31 & ((1 << p) - 1));
    return (unsigned)a;
}

/* ---- errors and logging ---- */
#define AVERROR(e) (-(e))
#define AVERROR_INVALIDDATA (-0x41444e49)
#define AVERROR_PATCHWELCOME (-0x45574650)
#define AV_LOG_ERROR 16
#define AV_LOG_WARNING 24
#define AV_LOG_DEBUG 48
void av_log(void* avcl, int level, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
#define ff_dlog(ctx, ...) do { if (0) av_log(ctx, AV_LOG_DEBUG, __VA_ARGS__); } while (0)
#define avpriv_request_sample(ctx, ...) av_log(ctx, AV_LOG_WARNING, "not implemented: " __VA_ARGS__)
#define avpriv_report_missing_feature(ctx, ...) av_log(ctx, AV_LOG_WARNING, "not implemented: " __VA_ARGS__)
#define av_assert0(cond) do { if (!(cond)) { av_log(NULL, AV_LOG_ERROR, "assertion %s failed\n", #cond); abort(); } } while (0)
#define av_assert1(cond) ((void)0)
#define av_assert2(cond) ((void)0)

/* ---- memory ---- */
void* av_malloc(size_t size);
void* av_mallocz(size_t size);
void* av_calloc(size_t nmemb, size_t size);
void* av_malloc_array(size_t nmemb, size_t size);
void* av_realloc_f(void* ptr, size_t nelem, size_t elsize);
void av_free(void* ptr);
void av_freep(void* ptr);   /* ptr: the address of the pointer */

/* ---- once ---- */
typedef int AVOnce;
#define AV_ONCE_INIT 0
static inline int ff_thread_once(AVOnce* once, void (*fn)(void))
{
    if (!*once) { *once = 1; fn(); }
    return 0;
}

/* ---- unaligned / aligned reads and writes (libavutil/intreadwrite.h), little endian host
 * independent ---- */
static inline uint16_t AV_RL16(const void* p) { const uint8_t* b = p; return (uint16_t)(b[0] | b[1] << 8); }
static inline uint32_t AV_RL32(const void* p)
{
    const uint8_t* b = p;
    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}
static inline uint32_t AV_RB32(const void* p)
{
    const uint8_t* b = p;
    return (uint32_t)b[3] | (uint32_t)b[2] << 8 | (uint32_t)b[1] << 16 | (uint32_t)b[0] << 24;
}
/* native order (AV_RN / AV_WN: memory order kept, as FFmpeg's) */
static inline uint16_t AV_RN16(const void* p) { uint16_t v; memcpy(&v, p, 2); return v; }
static inline uint32_t AV_RN32(const void* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint64_t AV_RN64(const void* p) { uint64_t v; memcpy(&v, p, 8); return v; }
static inline void AV_WN16(void* p, unsigned v) { uint16_t x = (uint16_t)v; memcpy(p, &x, 2); }
static inline void AV_WN32(void* p, uint32_t v) { memcpy(p, &v, 4); }
static inline void AV_WN64(void* p, uint64_t v) { memcpy(p, &v, 8); }
#define AV_RN16A AV_RN16
#define AV_RN32A AV_RN32
#define AV_RN64A AV_RN64
#define AV_WN16A AV_WN16
#define AV_WN32A AV_WN32
#define AV_WN64A AV_WN64
#define AV_COPY32U(d, s) memcpy((d), (s), 4)
#define AV_COPY64U(d, s) memcpy((d), (s), 8)

/* ---- byte readers (libavcodec/bytestream.h) ---- */
static inline unsigned bytestream_get_byte(const uint8_t** b) { return *(*b)++; }
static inline unsigned bytestream_get_le16(const uint8_t** b) { unsigned v = AV_RL16(*b); *b += 2; return v; }
static inline unsigned bytestream_get_le32(const uint8_t** b) { unsigned v = AV_RL32(*b); *b += 4; return v; }

typedef struct GetByteContext {
    const uint8_t *buffer, *buffer_end, *buffer_start;
} GetByteContext;
static inline void bytestream2_init(GetByteContext* g, const uint8_t* buf, int size)
{
    g->buffer = g->buffer_start = buf;
    g->buffer_end = buf + (size > 0 ? size : 0);
}
static inline int bytestream2_get_bytes_left(const GetByteContext* g) { return (int)(g->buffer_end - g->buffer); }
static inline unsigned bytestream2_get_byte(GetByteContext* g)
{
    if (g->buffer_end - g->buffer < 1) { g->buffer = g->buffer_end; return 0; }
    return *g->buffer++;
}
static inline unsigned bytestream2_get_le16(GetByteContext* g)
{
    if (g->buffer_end - g->buffer < 2) { g->buffer = g->buffer_end; return 0; }
    unsigned v = AV_RL16(g->buffer); g->buffer += 2; return v;
}
static inline unsigned bytestream2_get_le32(GetByteContext* g)
{
    if (g->buffer_end - g->buffer < 4) { g->buffer = g->buffer_end; return 0; }
    unsigned v = AV_RL32(g->buffer); g->buffer += 4; return v;
}
static inline void bytestream2_skip(GetByteContext* g, unsigned size)
{
    g->buffer += FFMIN((size_t)(g->buffer_end - g->buffer), (size_t)size);
}

/* ---- the bit reader (libavcodec/get_bits.h): big endian, or little endian when the file
 * defines BITSTREAM_READER_LE before including this header ---- */
typedef struct GetBitContext {
    const uint8_t* buffer;
    const uint8_t* buffer_end;
    int index;
    int size_in_bits;
    int size_in_bits_plus8;
} GetBitContext;

static inline int init_get_bits(GetBitContext* s, const uint8_t* buffer, int bit_size)
{
    int ret = 0;
    if (bit_size >= INT_MAX - 64 * 8 || bit_size < 0 || !buffer) {
        bit_size = 0;
        buffer = NULL;
        ret = AVERROR_INVALIDDATA;
    }
    s->buffer = buffer;
    s->size_in_bits = bit_size;
    s->size_in_bits_plus8 = bit_size + 8;
    s->buffer_end = buffer ? buffer + ((bit_size + 7) >> 3) : NULL;
    s->index = 0;
    return ret;
}
static inline int init_get_bits8(GetBitContext* s, const uint8_t* buffer, int byte_size)
{
    if (byte_size > INT_MAX / 8 || byte_size < 0) byte_size = -1;
    return init_get_bits(s, buffer, byte_size * 8);
}

/* 8 bytes from byte offset `at`, zeros past the end */
static inline uint64_t gb_load_(const GetBitContext* s, int at, int le)
{
    uint8_t b[8] = {0};
    ptrdiff_t have = s->buffer_end - s->buffer - at;
    if (at >= 0 && have > 0) memcpy(b, s->buffer + at, have >= 8 ? 8 : (size_t)have);
    uint64_t v = 0;
    if (le) for (int i = 7; i >= 0; i--) v = v << 8 | b[i];
    else for (int i = 0; i < 8; i++) v = v << 8 | b[i];
    return v;
}

#ifdef BITSTREAM_READER_LE
static inline unsigned show_bits_long(const GetBitContext* s, int n)
{
    if (n <= 0) return 0;
    uint64_t v = gb_load_(s, s->index >> 3, 1) >> (s->index & 7);
    return (unsigned)(v & ((n >= 32 ? 0xffffffffull : ((1ull << n) - 1))));
}
#else
static inline unsigned show_bits_long(const GetBitContext* s, int n)
{
    if (n <= 0) return 0;
    uint64_t v = gb_load_(s, s->index >> 3, 0) << (s->index & 7);
    return (unsigned)(v >> (64 - n));
}
#endif

static inline unsigned show_bits(const GetBitContext* s, int n) { return show_bits_long(s, n); }
static inline void skip_bits_long(GetBitContext* s, int n)
{
    int64_t i = (int64_t)s->index + n;
    if (i > s->size_in_bits_plus8) i = s->size_in_bits_plus8;
    if (i < 0) i = 0;
    s->index = (int)i;
}
static inline void skip_bits(GetBitContext* s, int n) { skip_bits_long(s, n); }
static inline void skip_bits1(GetBitContext* s) { skip_bits_long(s, 1); }
static inline unsigned get_bits(GetBitContext* s, int n)
{
    unsigned v = show_bits_long(s, n);
    skip_bits_long(s, n);
    return v;
}
static inline unsigned get_bits_long(GetBitContext* s, int n) { return get_bits(s, n); }
static inline unsigned get_bits1(GetBitContext* s) { return get_bits(s, 1); }
static inline int get_bits_count(const GetBitContext* s) { return s->index; }
static inline int get_bits_left(const GetBitContext* s) { return s->size_in_bits - s->index; }
static inline const uint8_t* align_get_bits(GetBitContext* s)
{
    int n = -get_bits_count(s) & 7;
    if (n) skip_bits(s, n);
    return s->buffer + (s->index >> 3);
}

/* ---- VLC tables (libavcodec/vlc.h, vlc.c) ---- */
typedef int16_t VLCBaseType;
typedef struct VLCElem { VLCBaseType sym, len; } VLCElem;
typedef struct VLC {
    int bits;
    VLCElem* table;
    int table_size, table_allocated;
} VLC;

#define VLC_INIT_USE_STATIC 1
#define VLC_INIT_STATIC_OVERLONG (2 | VLC_INIT_USE_STATIC)
#define VLC_INIT_INPUT_LE 4
#define VLC_INIT_OUTPUT_LE 8
#define VLC_INIT_LE (VLC_INIT_INPUT_LE | VLC_INIT_OUTPUT_LE)

int ff_vlc_init_sparse(VLC* vlc, int nb_bits, int nb_codes, const void* bits, int bits_wrap, int bits_size,
                       const void* codes, int codes_wrap, int codes_size, const void* symbols, int symbols_wrap,
                       int symbols_size, int flags);
void ff_vlc_free(VLC* vlc);
#define vlc_init(vlc, nb_bits, nb_codes, bits, bits_wrap, bits_size, codes, codes_wrap, codes_size, flags) \
    ff_vlc_init_sparse(vlc, nb_bits, nb_codes, bits, bits_wrap, bits_size, codes, codes_wrap, codes_size,  \
                       NULL, 0, 0, flags)

/* a code from table (bits index bits; up to max_depth table levels) */
static inline int get_vlc2(GetBitContext* s, const VLCElem* table, int bits, int max_depth)
{
    unsigned index = show_bits(s, bits);
    int code = table[index].sym, n = table[index].len;
    if (max_depth > 1 && n < 0) {
        skip_bits(s, bits);
        int nb_bits = -n;
        index = show_bits(s, nb_bits) + (unsigned)code;
        code = table[index].sym;
        n = table[index].len;
        if (max_depth > 2 && n < 0) {
            skip_bits(s, nb_bits);
            nb_bits = -n;
            index = show_bits(s, nb_bits) + (unsigned)code;
            code = table[index].sym;
            n = table[index].len;
        }
    }
    skip_bits(s, n);
    return code;
}

/* ---- block copies (copy_block.h, hpeldsp's put_pixels) ---- */
static inline void copy_block4(uint8_t* dst, const uint8_t* src, ptrdiff_t dstStride, ptrdiff_t srcStride, int h)
{
    for (int i = 0; i < h; i++) { memcpy(dst, src, 4); dst += dstStride; src += srcStride; }
}
typedef void (*op_pixels_func)(uint8_t* block, const uint8_t* pixels, ptrdiff_t line_size, int h);
typedef struct HpelDSPContext { op_pixels_func put_pixels_tab[4][4]; } HpelDSPContext;
void ff_hpeldsp_init(HpelDSPContext* c, int flags);

/* ---- codec context, frames, packets (libavcodec/avcodec.h, libavutil/frame.h) ---- */
enum AVPixelFormat { AV_PIX_FMT_NONE = -1, AV_PIX_FMT_YUV410P = 0 };
enum AVCodecID { AV_CODEC_ID_NONE, AV_CODEC_ID_INDEO3, AV_CODEC_ID_INDEO4 };
enum AVDiscard { AVDISCARD_NONE = -16, AVDISCARD_DEFAULT = 0, AVDISCARD_NONREF = 8, AVDISCARD_BIDIR = 16,
                 AVDISCARD_NONINTRA = 24, AVDISCARD_NONKEY = 32, AVDISCARD_ALL = 48 };

typedef struct AVCodecContext {
    int width, height;
    int pix_fmt;
    int flags;
    int64_t max_pixels;
    enum AVCodecID codec_id;
    enum AVDiscard skip_frame;
    void* priv_data;
} AVCodecContext;

/* one YUV 4:1:0 picture: data[0..2] Y, U, V */
typedef struct AVFrame {
    uint8_t* data[4];
    int linesize[4];
    int width, height;
    uint8_t* buf;   /* owns data[] */
} AVFrame;

typedef struct AVPacket {
    const uint8_t* data;
    int size;
} AVPacket;

AVFrame* av_frame_alloc(void);
void av_frame_free(AVFrame** f);
void av_frame_unref(AVFrame* f);
void av_frame_move_ref(AVFrame* dst, AVFrame* src);
int ff_get_buffer(AVCodecContext* avctx, AVFrame* frame, int flags);
int ff_set_dimensions(AVCodecContext* avctx, int width, int height);
int av_image_check_size(unsigned w, unsigned h, int log_offset, void* log_ctx);
int av_image_check_size2(unsigned w, unsigned h, int64_t max_pixels, enum AVPixelFormat pix_fmt, int log_offset,
                         void* log_ctx);

#endif /* INDEO_FFCOMPAT_H */
