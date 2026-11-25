/*
 * indeo.c: the small front end over the Indeo 3 / Indeo 4 decoders (indeo.h): codec contexts,
 * one output frame, and the YUV 4:1:0 -> RGB conversion.
 *
 * This file is part of Roadkill's copy of FFmpeg's Indeo decoders.
 *
 * It is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser
 * General Public License as published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details.
 */
#include "indeo.h"

#include "ffcompat.h"

int ff_indeo3_priv_size(void);
int ff_indeo3_init(AVCodecContext* avctx);
int ff_indeo3_decode(AVCodecContext* avctx, AVFrame* frame, int* got_frame, AVPacket* avpkt);
int ff_indeo3_close(AVCodecContext* avctx);
int ff_indeo4_priv_size(void);
int ff_indeo4_init(AVCodecContext* avctx);
int ff_ivi_decode_frame(AVCodecContext* avctx, AVFrame* data, int* got_frame, AVPacket* avpkt);
int ff_ivi_decode_close(AVCodecContext* avctx);

struct IndeoDecoder {
    AVCodecContext ctx;
    AVFrame* frame;
    int ver;   /* 3 or 4 */
};

static uint32_t upper(uint32_t f)
{
    uint32_t r = 0;
    for (int i = 0; i < 4; i++) {
        uint32_t c = (f >> (8 * i)) & 0xff;
        if (c >= 'a' && c <= 'z') c -= 32;
        r |= c << (8 * i);
    }
    return r;
}

static int version_of(uint32_t fourcc)
{
    uint32_t f = upper(fourcc);
    if (f == MKTAG('I', 'V', '3', '1') || f == MKTAG('I', 'V', '3', '2')) return 3;
    if (f == MKTAG('I', 'V', '4', '1')) return 4;
    return 0;
}

int indeo_supported(uint32_t fourcc) { return version_of(fourcc) != 0; }

IndeoDecoder* indeo_open(uint32_t fourcc, int width, int height)
{
    int ver = version_of(fourcc);
    if (!ver || width <= 0 || height <= 0) return NULL;
    IndeoDecoder* d = calloc(1, sizeof *d);
    if (!d) return NULL;
    d->ver = ver;
    d->ctx.width = width;
    d->ctx.height = height;
    d->ctx.codec_id = ver == 3 ? AV_CODEC_ID_INDEO3 : AV_CODEC_ID_INDEO4;
    d->ctx.skip_frame = AVDISCARD_DEFAULT;
    d->ctx.priv_data = calloc(1, (size_t)(ver == 3 ? ff_indeo3_priv_size() : ff_indeo4_priv_size()));
    d->frame = av_frame_alloc();
    if (!d->ctx.priv_data || !d->frame || (ver == 3 ? ff_indeo3_init(&d->ctx) : ff_indeo4_init(&d->ctx)) < 0) {
        indeo_close(d);
        return NULL;
    }
    return d;
}

void indeo_close(IndeoDecoder* d)
{
    if (!d) return;
    if (d->ctx.priv_data) {
        if (d->ver == 3) ff_indeo3_close(&d->ctx);
        else ff_ivi_decode_close(&d->ctx);
    }
    free(d->ctx.priv_data);
    av_frame_free(&d->frame);
    free(d);
}

int indeo_decode(IndeoDecoder* d, const uint8_t* data, int size, IndeoPicture* pic)
{
    if (!d || !data || size <= 0) return 0;
    AVPacket pkt = {data, size};
    int got = 0;
    int r = d->ver == 3 ? ff_indeo3_decode(&d->ctx, d->frame, &got, &pkt)
                        : ff_ivi_decode_frame(&d->ctx, d->frame, &got, &pkt);
    if (r < 0) return r;
    if (!got || !d->frame->data[0]) return 0;
    if (pic) {
        for (int i = 0; i < 3; i++) {
            pic->plane[i] = d->frame->data[i];
            pic->pitch[i] = d->frame->linesize[i];
        }
        pic->width = d->frame->width;
        pic->height = d->frame->height;
    }
    return 1;
}

static uint8_t clamp8(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }

void indeo_to_xrgb(const IndeoPicture* pic, uint32_t* dst, int pitch)
{
    for (int y = 0; y < pic->height; y++) {
        const uint8_t* yl = pic->plane[0] + (ptrdiff_t)y * pic->pitch[0];
        const uint8_t* ul = pic->plane[1] + (ptrdiff_t)(y >> 2) * pic->pitch[1];
        const uint8_t* vl = pic->plane[2] + (ptrdiff_t)(y >> 2) * pic->pitch[2];
        uint32_t* o = (uint32_t*)((uint8_t*)dst + (ptrdiff_t)y * pitch);
        for (int x = 0; x < pic->width; x++) {
            int c = (yl[x] - 16) * 298, u = ul[x >> 2] - 128, v = vl[x >> 2] - 128;
            int r = (c + 409 * v + 128) >> 8;
            int g = (c - 100 * u - 208 * v + 128) >> 8;
            int b = (c + 516 * u + 128) >> 8;
            o[x] = 0xff000000u | (uint32_t)clamp8(r) << 16 | (uint32_t)clamp8(g) << 8 | clamp8(b);
        }
    }
}
