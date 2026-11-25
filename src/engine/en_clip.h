/*
 * en_clip.h: float vertex leaves of the polygon path of TM2.EXE's scene renderer
 * (polygon_renderer 0x0048e9b8 calls them per polygon).
 *
 *   0x0048c62c en_xform_short   (old name math_short_to_float_vec)  i16 x, y, z -> view space
 *   0x0048c6b8 en_xform_int     (old name math_int_to_float_vec)    i32 x, y, z -> view space
 *   0x0048c890 en_project       (old name math_normalize_float)     perspective divide to screen
 *   0x0048f59c en_clip_plane    (old name frustum_clip_plane)       one Sutherland-Hodgman pass
 *   0x00491ee4 en_winding_check (old name winding_check)            back face / tiny polygon test
 *
 * Floating point: the game runs the x87 with control word 0x127F (Watcom CRT, 0x004e94dc
 * loaded at 0x004c15e6): 53 bit precision, round to nearest. So every x87 operation rounds
 * like a C double operation; the port does each operation in double in the machine code's
 * order and rounds to float exactly where the original stores a dword (fstp dword).
 * Not covered (port and original may differ): NaN operands (x87 and the host pick different
 * NaN payloads) and double-denormal intermediates (x87 keeps the 15 bit exponent).
 *
 * Vertex record (0x34 bytes, the pool at g_vertex_struct_base, next free 0x009f7dd8),
 * as float index (byte offset):
 *   0 x, 1 y, 2 z (+0x00..+0x08)  view space (en_xform_*); z is depth
 *   3 u, 4 v      (+0x0c, +0x10)  texture coordinates (polygon_renderer: u8 * 0x1000 + 0x800)
 *   5 sx, 6 sy    (+0x14, +0x18)  screen position (en_project)
 *   7 r, 8 g, 9 b (+0x1c..+0x24)  gouraud colour (0..255)
 *   10            (+0x28)         always 0.1f (0x3dcccccd)
 *   11            (+0x2c)         copy of 0x004e82b4 (g_depth_scale), raw 32 bits
 *   12 1/z        (+0x30)         written by en_project
 * en_clip_plane interpolates the fields whose bit is set in the interpolation mask
 * (0x009f7d44: 7 = x y z, 0x1f = + u v, 0x39f = + r g b).
 *
 * Vertex list (EnClipBuf, the clip buffers, e.g. 0x009f7d84 g_poly_vert_count): i32 count, then
 * that many vertex pointers (the port drops pointers past EN_CLIP_MAX, the count still grows).
 */
#ifndef TM2_EN_CLIP_H
#define TM2_EN_CLIP_H

#include <stdint.h>

#include "en_vertex.h"   /* EnVertex, EnClipBuf */

/* view transform 0x009f77d0: m[9] row major (out[i] = sum_j m[3i + j] * in[j]), then t[3]
 * (0x009f77f4) added to the input before the rotation */
typedef struct {
    float m[9];
    float t[3];
} EnXform;

/* projection constants 0x009f7810: centre x, centre y, scale x, scale y */
typedef struct {
    float cx, cy, sx, sy;
} EnProj;

typedef struct {
    EnVertex* next;          /* 0x009f7dd8 next free vertex (advanced by 1 per new vertex) */
    uint32_t interp_mask;    /* 0x009f7d44 */
    int32_t clipped;         /* 0x009f7d80, set to 1 when an edge crosses the plane */
    uint32_t depth_scale;    /* 0x004e82b4, raw bits copied into new vertices' +0x2c */
} EnClipEnv;

/* out = M * (in + t), stored as 3 floats */
void en_xform_short(const EnXform* x, const int16_t in[3], float out[3]);
void en_xform_int(const EnXform* x, const int32_t in[3], float out[3]);

/* z = 1.0 when z is +-0 (written back); +0x30 = (float)(1 / z);
 * sx = x * scale_x * (1/z) + cx, sy = y * scale_y * (1/z) + cy */
void en_project(const EnProj* p, EnVertex* v);

/* Clips `in` against the plane v[a] - v[b] >= 0 (sign = 1) or v[a] + v[b] >= 0 (otherwise)
 * into `out`. New vertices come from env->next. With in->n == 2 (a line) only the first
 * edge is looked at, then the second vertex is appended unless the first one is outside. */
void en_clip_plane(EnClipEnv* env, const EnClipBuf* in, EnClipBuf* out, int a, int b, int sign);

/* Returns 1 to draw. Counts the corner triangles (i, i+1, i+2) whose signed screen area is
 * <= 500.0 (by bit pattern, full screen) or <= 0 (split screen): full screen draws when none
 * is, split screen when at most n - 2 are. */
int en_winding_check(const EnClipBuf* list, int split);

#endif /* TM2_EN_CLIP_H */
