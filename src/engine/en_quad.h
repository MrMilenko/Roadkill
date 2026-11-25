/*
 * en_quad.h: float leaves of the polygon renderer (TM2.EXE 0x0048e9b8) that turn one DPC mesh
 * polygon into the clip input, ported from the machine code:
 *
 *   0x00490780 billboard_quad     sprite polygon (1 vertex record): a screen aligned quad
 *   0x004908b0 line_segment_quad  line polygon (2 vertices): a quad of constant width
 *   0x00490e74 water_uv_animate   mesh type 699: texture coordinate wobble from sin()
 *   0x00492914 color_interpolate  fog colour: lerp of two 0x00RRGGBB colours
 *
 * Floating point: the game runs the x87 with control word 0x127F (Watcom CRT, word 0x004e94dc
 * loaded at 0x004c15e6): 53 bit precision, round to nearest. So every x87 intermediate is a C
 * double, every `fstp dword` a (float) store, `fistp` round to nearest even. The port mirrors the
 * original's operation order and float stores exactly (verified bit for bit, emulator FPCW set
 * to 0x127F). Not modelled: the x87's wider exponent range (a double overflow / denormal in an
 * intermediate; unreachable with game coordinates).
 *
 * Vertex record (g_vertex_struct_base 0x009f78b0, 0x34 bytes each; polygon_renderer fills
 * records 0..n-1, line_segment_quad uses records 4..7 as scratch):
 *   +0x00 f32 x, +0x04 y, +0x08 z (view space, later scaled by 1 / (1 << texture shift))
 *   +0x0c f32 u, +0x10 v (texel * 0x1000 + 0x800)
 *   +0x14, +0x18 (not touched here), +0x1c / +0x20 / +0x24 f32 r, g, b (gouraud)
 *   +0x28 f32 0.1, +0x2c f32 depth scale (g_depth_scale), +0x30 (not touched here)
 * Clip buffer A (0x009f7ddc): i32 vertex count, then vertex pointers 0x009f7de0...
 */
#ifndef TM2_EN_QUAD_H
#define TM2_EN_QUAD_H

#include <stdint.h>

#include "en_vertex.h"   /* EnVertex (0x34 bytes), EN_CLIP_MAX */


typedef struct {
    EnVertex* vtx;                 /* 0x009f78b0 vertex records (>= 8 for line_segment_quad) */
    int32_t vert_count;            /* 0x009f7ddc clip buffer A count */
    EnVertex* ptr[EN_CLIP_MAX];    /* 0x009f7de0 clip buffer A vertex pointers */
    float tex_inv;                 /* 0x009f7d14 1 / (1 << texture shift) */
    float fov_x;                   /* 0x009f7ce0 g_fov_x_scaled */
    float size_x;                  /* 0x009f7d00 sprite half width / line width */
    float size_y;                  /* 0x009f7d30 sprite half height */
    float zmax;                    /* 0x009f7d1c */
    float zmin;                    /* 0x009f7d2c */
    uint8_t render_flags;          /* 0x00c846d8 low byte (renderer flags) */
    /* water_uv_animate */
    const uint8_t* poly;           /* 0x009f7cf4 current polygon record (u16 vertex indices at +4) */
    const uint8_t* verts;          /* 0x009f7cc4 mesh vertices, 8 bytes: i16 x, y, z, pad */
    int32_t wave_a, wave_b;        /* 0x009f7d04, 0x009f7d08 wave phases (0..0xffe) */
} EnQuadEnv;

/* 0x00490780: ptr[0] holds the sprite center (x, y, z), size_x / size_y its scaled half sizes.
 * z *= tex_inv -> zmax; returns 0 unless zmax (as int bits) > 1.0f (in front of the near plane).
 * Else x, y *= tex_inv and builds the quad ptr[0..3] = (x - w, y - h), (x + w, y - h),
 * (x + w, y + h), (x - w, y + h), all at z (corner 1 x is rounded once from the unrounded
 * x - w); size_x / size_y become 2w / 2h; zmin = zmax; count 4; flags |= 0x20, &= ~0x10. */
int en_billboard_quad(EnQuadEnv* e);

/* 0x004908b0: vertex records 4 and 5 hold the end points; size_x the width. Returns 0 when the
 * far end is behind the near plane (tex_inv * max z as int bits <= 1.0f) or the two points are
 * on one line through the eye (cross product length 0); else writes records 0..3 (end points
 * -/+ the perpendicular offset), count 4, flags |= 0x20. Records 6 / 7 hold the scratch vectors
 * (6: end - start, 7: start x end, scaled). */
int en_line_segment_quad(EnQuadEnv* e);

/* 0x00490e74: for each of the vert_count vertices i (mesh index u16 at poly + 4 + 2 i):
 * u += sin((x * 17 + a) & 0xfff) * 2.6 * 0.5 + 0.5 * (sin((y * 9 + b) & 0xfff) * 4.3),
 * v += sin((x * 7 + b) & 0xfff) * 3.0 * 0.5 + 0.5 * (sin((y * 23 + a) & 0xfff) * 3.8)
 * (each term added and stored as float separately). */
void en_water_uv_animate(EnQuadEnv* e);

/* 0x00492914 (five stack args, ret 0x14): t = (z - z0) / (z1 - z0) as float; per channel
 * round(c0 + (c1 - c0) * t) & 0xff. Red is computed from the ints in double, green and blue
 * from float copies of the channels (the same values: channels are exact in float). */
uint32_t en_color_interpolate(float z, float z0, float z1, uint32_t c0, uint32_t c1);

#endif /* TM2_EN_QUAD_H */
