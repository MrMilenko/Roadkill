/*
 * indeo.h: Intel Indeo 3 and Indeo 4 (Video Interactive 4.1) video decoding, from FFmpeg's
 * decoders (see README.md). LGPL-2.1-or-later.
 *
 * Usage: open a decoder for a stream's FOURCC and size, feed it each frame's chunk in order, and
 * read the picture it gives back (planar YUV 4:1:0: Y w x h, U and V (w+3)/4 x (h+3)/4).
 */
#ifndef INDEO_H
#define INDEO_H

#include <stdint.h>

typedef struct IndeoDecoder IndeoDecoder;

typedef struct IndeoPicture {
    const uint8_t* plane[3];   /* Y, U, V */
    int pitch[3];
    int width, height;
} IndeoPicture;

/* 1 when the FOURCC (as read from the file, any case: IV31 / IV32 / IV41) is one of these */
int indeo_supported(uint32_t fourcc);
/* NULL when the FOURCC is not Indeo 3 / 4 or out of memory */
IndeoDecoder* indeo_open(uint32_t fourcc, int width, int height);
/* decodes one chunk: 1 with a new picture in *pic, 0 when the chunk gives no picture (a sync or
 * null frame: the previous picture stays), < 0 on an error */
int indeo_decode(IndeoDecoder* d, const uint8_t* data, int size, IndeoPicture* pic);
void indeo_close(IndeoDecoder* d);

/* YUV 4:1:0 -> 32 bit XRGB (0xffRRGGBB per pixel, BT.601 limited range as Video for Windows
 * shows it), w x h into dst (pitch in bytes) */
void indeo_to_xrgb(const IndeoPicture* pic, uint32_t* dst, int pitch);

#endif /* INDEO_H */
