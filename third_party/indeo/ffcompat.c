/*
 * ffcompat.c: the framework pieces behind ffcompat.h: memory, logging, frames, the pixel copies
 * of hpeldsp and the VLC table builder (build_table / ff_vlc_init_sparse, from FFmpeg 7.1's
 * libavcodec/vlc.c, trimmed to what the Indeo decoders use).
 *
 * vlc.c: Copyright (c) 2000, 2001 Fabrice Bellard
 *        Copyright (c) 2002-2004 Michael Niedermayer <michaelni@gmx.at>
 *        Copyright (c) 2010 Loren Merritt
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
 */
#include <stdarg.h>
#include <stdio.h>

#include "ffcompat.h"

/* ---- mathtables.c ---- */

const uint8_t ff_zigzag_direct[64] = {
    0,   1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

/* ---- logging: INDEO_LOG=1 prints the decoders' messages ---- */

void av_log(void* avcl, int level, const char* fmt, ...)
{
    (void)avcl;
    static int on = -1;
    if (on < 0) { const char* e = getenv("INDEO_LOG"); on = e && *e && *e != '0'; }
    if (!on || level > AV_LOG_DEBUG) return;
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "indeo: ");
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

/* ---- memory ---- */

void* av_malloc(size_t size) { return malloc(size ? size : 1); }
void* av_mallocz(size_t size) { return calloc(1, size ? size : 1); }
void* av_calloc(size_t nmemb, size_t size) { return calloc(nmemb ? nmemb : 1, size ? size : 1); }
void* av_malloc_array(size_t nmemb, size_t size)
{
    if (size && nmemb > SIZE_MAX / size) return NULL;
    return av_malloc(nmemb * size);
}
void* av_realloc_f(void* ptr, size_t nelem, size_t elsize)
{
    if (elsize && nelem > SIZE_MAX / elsize) { free(ptr); return NULL; }
    void* r = realloc(ptr, nelem * elsize ? nelem * elsize : 1);
    if (!r) free(ptr);
    return r;
}
void av_free(void* ptr) { free(ptr); }
void av_freep(void* arg)
{
    void* p;
    memcpy(&p, arg, sizeof p);
    free(p);
    p = NULL;
    memcpy(arg, &p, sizeof p);
}

/* ---- frames ---- */

AVFrame* av_frame_alloc(void) { return calloc(1, sizeof(AVFrame)); }
void av_frame_unref(AVFrame* f)
{
    if (!f) return;
    free(f->buf);
    memset(f, 0, sizeof *f);
}
void av_frame_free(AVFrame** f)
{
    if (!f || !*f) return;
    av_frame_unref(*f);
    free(*f);
    *f = NULL;
}
void av_frame_move_ref(AVFrame* dst, AVFrame* src)
{
    av_frame_unref(dst);
    *dst = *src;
    memset(src, 0, sizeof *src);
}

/* a YUV 4:1:0 picture of the context's size; the frame's own buffer is kept when it fits */
int ff_get_buffer(AVCodecContext* avctx, AVFrame* frame, int flags)
{
    (void)flags;
    int w = avctx->width, h = avctx->height;
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) return AVERROR_INVALIDDATA;
    int cw = (w + 3) >> 2, ch = (h + 3) >> 2;
    int yp = FFALIGN(w, 16), cp = FFALIGN(cw, 16);
    size_t need = (size_t)yp * (size_t)h + 2 * (size_t)cp * (size_t)ch + 64;
    if (!frame->buf || frame->width != w || frame->height != h) {
        free(frame->buf);
        memset(frame, 0, sizeof *frame);
        frame->buf = calloc(1, need);
        if (!frame->buf) return AVERROR(ENOMEM);
    }
    frame->width = w;
    frame->height = h;
    frame->data[0] = frame->buf;
    frame->data[1] = frame->buf + (size_t)yp * (size_t)h;
    frame->data[2] = frame->data[1] + (size_t)cp * (size_t)ch;
    frame->linesize[0] = yp;
    frame->linesize[1] = frame->linesize[2] = cp;
    return 0;
}

int av_image_check_size2(unsigned w, unsigned h, int64_t max_pixels, enum AVPixelFormat pix_fmt, int log_offset,
                         void* log_ctx)
{
    (void)pix_fmt; (void)log_offset;
    if (!w || !h || w > 16384 || h > 16384 || (max_pixels > 0 && (int64_t)w * h > max_pixels)) {
        av_log(log_ctx, AV_LOG_ERROR, "Picture size %ux%u is invalid\n", w, h);
        return AVERROR(EINVAL);
    }
    return 0;
}
int av_image_check_size(unsigned w, unsigned h, int log_offset, void* log_ctx)
{
    return av_image_check_size2(w, h, INT64_MAX, AV_PIX_FMT_NONE, log_offset, log_ctx);
}
int ff_set_dimensions(AVCodecContext* avctx, int width, int height)
{
    int r = av_image_check_size((unsigned)width, (unsigned)height, 0, avctx);
    if (r < 0) width = height = 0;
    avctx->width = width;
    avctx->height = height;
    return r;
}

/* ---- hpeldsp: the full pel copies (put_pixels16 / 8 / 4) ---- */

static void put_pixels16_c(uint8_t* block, const uint8_t* pixels, ptrdiff_t line_size, int h)
{
    for (int i = 0; i < h; i++) { memcpy(block, pixels, 16); block += line_size; pixels += line_size; }
}
static void put_pixels8_c(uint8_t* block, const uint8_t* pixels, ptrdiff_t line_size, int h)
{
    for (int i = 0; i < h; i++) { memcpy(block, pixels, 8); block += line_size; pixels += line_size; }
}
static void put_pixels4_c(uint8_t* block, const uint8_t* pixels, ptrdiff_t line_size, int h)
{
    for (int i = 0; i < h; i++) { memcpy(block, pixels, 4); block += line_size; pixels += line_size; }
}
void ff_hpeldsp_init(HpelDSPContext* c, int flags)
{
    (void)flags;
    memset(c, 0, sizeof *c);
    c->put_pixels_tab[0][0] = put_pixels16_c;
    c->put_pixels_tab[1][0] = put_pixels8_c;
    c->put_pixels_tab[2][0] = put_pixels4_c;
}

/* ---- VLC tables (vlc.c) ---- */

#define GET_DATA(v, table, i, wrap, size)                   \
{                                                           \
    const uint8_t *ptr = (const uint8_t *)table + i * wrap; \
    switch(size) {                                          \
    case 1:                                                 \
        v = *(const uint8_t *)ptr;                          \
        break;                                              \
    case 2:                                                 \
        v = *(const uint16_t *)ptr;                         \
        break;                                              \
    case 4:                                                 \
    default:                                                \
        v = *(const uint32_t *)ptr;                         \
        break;                                              \
    }                                                       \
}

static int alloc_table(VLC *vlc, int size, int use_static)
{
    int index = vlc->table_size;

    vlc->table_size += size;
    if (vlc->table_size > vlc->table_allocated) {
        if (use_static)
            abort(); // cannot do anything, vlc_init() is used with too little memory
        vlc->table_allocated += (1 << vlc->bits);
        vlc->table = av_realloc_f(vlc->table, vlc->table_allocated, sizeof(*vlc->table));
        if (!vlc->table) {
            vlc->table_allocated = 0;
            vlc->table_size = 0;
            return AVERROR(ENOMEM);
        }
        memset(vlc->table + vlc->table_allocated - (1 << vlc->bits), 0, sizeof(*vlc->table) << vlc->bits);
    }
    return index;
}

#define LOCALBUF_ELEMS 1500

static uint32_t bitswap_32(uint32_t x)
{
    uint32_t r = 0;
    for (int i = 0; i < 32; i++) { r = r << 1 | (x & 1); x >>= 1; }
    return r;
}

typedef struct VLCcode {
    uint8_t bits;
    VLCBaseType symbol;
    /** codeword, with the first bit-to-be-read in the msb
     * (even if intended for a little-endian bitstream reader) */
    uint32_t code;
} VLCcode;

static int vlc_common_init(VLC *vlc, int nb_bits, int nb_codes,
                           VLCcode **buf, int flags)
{
    vlc->bits = nb_bits;
    vlc->table_size = 0;
    if (flags & VLC_INIT_USE_STATIC) {
        av_assert0(nb_codes <= LOCALBUF_ELEMS);
    } else {
        vlc->table           = NULL;
        vlc->table_allocated = 0;
    }
    if (nb_codes > LOCALBUF_ELEMS) {
        *buf = av_malloc_array(nb_codes, sizeof(VLCcode));
        if (!*buf)
            return AVERROR(ENOMEM);
    }

    return 0;
}

static int compare_vlcspec(const void *a, const void *b)
{
    const VLCcode *sa = a, *sb = b;
    return (int)(sa->code >> 1) - (int)(sb->code >> 1);
}

static int build_table(VLC *vlc, int table_nb_bits, int nb_codes,
                       VLCcode *codes, int flags)
{
    int table_size, table_index;
    VLCElem *table;

    if (table_nb_bits > 30)
       return AVERROR(EINVAL);
    table_size = 1 << table_nb_bits;
    table_index = alloc_table(vlc, table_size, flags & VLC_INIT_USE_STATIC);
    if (table_index < 0)
        return table_index;
    table = &vlc->table[table_index];

    /* first pass: map codes and compute auxiliary table sizes */
    for (int i = 0; i < nb_codes; i++) {
        int         n = codes[i].bits;
        uint32_t code = codes[i].code;
        int    symbol = codes[i].symbol;
        if (n <= table_nb_bits) {
            /* no need to add another table */
            int   j = code >> (32 - table_nb_bits);
            int  nb = 1 << (table_nb_bits - n);
            int inc = 1;

            if (flags & VLC_INIT_OUTPUT_LE) {
                j = bitswap_32(code);
                inc = 1 << n;
            }
            for (int k = 0; k < nb; k++) {
                int   bits = table[j].len;
                int oldsym = table[j].sym;
                if ((bits || oldsym) && (bits != n || oldsym != symbol)) {
                    av_log(NULL, AV_LOG_ERROR, "incorrect codes\n");
                    return AVERROR_INVALIDDATA;
                }
                table[j].len = n;
                table[j].sym = symbol;
                j += inc;
            }
        } else {
            /* fill auxiliary table recursively */
            uint32_t code_prefix;
            int index, subtable_bits, j, k;

            n -= table_nb_bits;
            code_prefix = code >> (32 - table_nb_bits);
            subtable_bits = n;
            codes[i].bits = n;
            codes[i].code = code << table_nb_bits;
            for (k = i + 1; k < nb_codes; k++) {
                n = codes[k].bits - table_nb_bits;
                if (n <= 0)
                    break;
                code = codes[k].code;
                if (code >> (32 - table_nb_bits) != code_prefix)
                    break;
                codes[k].bits = n;
                codes[k].code = code << table_nb_bits;
                subtable_bits = FFMAX(subtable_bits, n);
            }
            subtable_bits = FFMIN(subtable_bits, table_nb_bits);
            j = (flags & VLC_INIT_OUTPUT_LE) ? bitswap_32(code_prefix) >> (32 - table_nb_bits) : code_prefix;
            table[j].len = -subtable_bits;
            index = build_table(vlc, subtable_bits, k-i, codes+i, flags);
            if (index < 0)
                return index;
            /* note: realloc has been done, so reload tables */
            table = &vlc->table[table_index];
            table[j].sym = index;
            if (table[j].sym != index) {
                avpriv_request_sample(NULL, "strange codes");
                return AVERROR_PATCHWELCOME;
            }
            i = k-1;
        }
    }

    for (int i = 0; i < table_size; i++) {
        if (table[i].len == 0)
            table[i].sym = -1;
    }

    return table_index;
}

static int vlc_common_end(VLC *vlc, int nb_bits, int nb_codes, VLCcode *codes,
                          int flags, VLCcode localbuf[LOCALBUF_ELEMS])
{
    int ret = build_table(vlc, nb_bits, nb_codes, codes, flags);

    if (flags & VLC_INIT_USE_STATIC) {
        av_assert0(ret >= 0);
    } else {
        if (codes != localbuf)
            av_free(codes);
        if (ret < 0) {
            av_freep(&vlc->table);
            return ret;
        }
    }
    return 0;
}

int ff_vlc_init_sparse(VLC *vlc, int nb_bits, int nb_codes,
                       const void *bits, int bits_wrap, int bits_size,
                       const void *codes, int codes_wrap, int codes_size,
                       const void *symbols, int symbols_wrap, int symbols_size,
                       int flags)
{
    VLCcode localbuf[LOCALBUF_ELEMS], *buf = localbuf;
    int j, ret;

    ret = vlc_common_init(vlc, nb_bits, nb_codes, &buf, flags);
    if (ret < 0)
        return ret;

    av_assert0(symbols_size <= 2 || !symbols);
    j = 0;
#define COPY(condition)\
    for (int i = 0; i < nb_codes; i++) {                                    \
        unsigned len;                                                       \
        GET_DATA(len, bits, i, bits_wrap, bits_size);                       \
        if (!(condition))                                                   \
            continue;                                                       \
        if (len > 3u*nb_bits || len > 32) {                                 \
            av_log(NULL, AV_LOG_ERROR, "Too long VLC (%u) in vlc_init\n", len);\
            if (buf != localbuf)                                            \
                av_free(buf);                                               \
            return AVERROR(EINVAL);                                         \
        }                                                                   \
        buf[j].bits = len;                                                  \
        GET_DATA(buf[j].code, codes, i, codes_wrap, codes_size);            \
        if (buf[j].code >= (1LL<<buf[j].bits)) {                            \
            av_log(NULL, AV_LOG_ERROR, "Invalid code %"PRIx32" for %d in "  \
                   "vlc_init\n", buf[j].code, i);                           \
            if (buf != localbuf)                                            \
                av_free(buf);                                               \
            return AVERROR(EINVAL);                                         \
        }                                                                   \
        if (flags & VLC_INIT_INPUT_LE)                                      \
            buf[j].code = bitswap_32(buf[j].code);                          \
        else                                                                \
            buf[j].code <<= 32 - buf[j].bits;                               \
        if (symbols)                                                        \
            GET_DATA(buf[j].symbol, symbols, i, symbols_wrap, symbols_size) \
        else                                                                \
            buf[j].symbol = i;                                              \
        j++;                                                                \
    }
    COPY(len > (unsigned)nb_bits);
    // qsort is the slowest part of vlc_init, and could probably be improved or avoided
    qsort(buf, j, sizeof(VLCcode), compare_vlcspec);
    COPY(len && len <= (unsigned)nb_bits);
    nb_codes = j;

    return vlc_common_end(vlc, nb_bits, nb_codes, buf,
                          flags, localbuf);
}

void ff_vlc_free(VLC *vlc)
{
    av_freep(&vlc->table);
}
