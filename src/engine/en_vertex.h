/*
 * en_vertex.h: the polygon renderer's working vertex (TM2.EXE, 0x34 bytes, an array at
 * 0x009f78b0 = g_vertex_struct_base; the clip buffers hold pointers to them). Shared by the
 * src/engine ports. Field meaning from 0x0048e9b8 / 0x0048fd54 (by reading).
 */
#ifndef TM2_EN_VERTEX_H
#define TM2_EN_VERTEX_H

#include <stdint.h>

typedef struct {
    float x, y, z;      /* +0x00 view space position (z = depth) */
    float u, v;         /* +0x0c / +0x10 texture coordinates, .12 fixed as float (u8 * 0x1000 + 0x800) */
    float sx, sy;       /* +0x14 / +0x18 screen position in 1/32 pixel units (edge units) */
    float r, g, b;      /* +0x1c / +0x20 / +0x24 vertex color 0..255 (gouraud) */
    float near_z;       /* +0x28 set to 0.1f per polygon */
    float depth_scale;  /* +0x2c g_depth_scale */
    float w;            /* +0x30 */
} EnVertex;

/* a clip buffer (0x004e8290 / ... point at one): count, then vertex pointers */
#define EN_CLIP_MAX 16
typedef struct {
    int32_t count;
    EnVertex* v[EN_CLIP_MAX];
} EnClipBuf;

#endif /* TM2_EN_VERTEX_H */
