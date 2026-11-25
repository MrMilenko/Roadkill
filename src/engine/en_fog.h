/*
 * en_fog.h: light terms of one polygon for the renderer's 0xa0 path, ported from TM2.EXE
 * 0x0049024c (polygon_renderer 0x0048e9b8 calls it when 0x004e8e10 != 0, render flag 1 is set
 * and the page's TPC entry +0x14 is nonzero). Floating point as in en_quad.h (x87 at 53 bit
 * precision: doubles, float stores mirrored).
 *
 * Light table 0x00c85c00: 16 entries x 0x78 bytes:
 *   +0x00 u8 kind (0 point: nearest one picked for gouraud; 1 shimmer; 2 dead code, see below)
 *   +0x04, +0x08 must both be nonzero (entry in use); +0x0c must be nonzero for kind 0
 *   +0x50 / +0x54 / +0x58 f32 position (kind 0), +0x5c / +0x60 / +0x64 f32 direction (kind 2),
 *   +0x6c f32 intensity (kinds 1, 2)
 *
 * Writes: q[0..2] (0x009f7d6c, 0x009f7d74, 0x009f7d68: the fourth interpolant at the polygon's
 * first three vertices, zeroed first), the nearest kind 0 light's index (0x009f7ccc, only when
 * one is nearer than 1e6 squared units, starting from 1e6), render flags (0x00c846d8) |= 0x4000
 * when a kind 0 light was picked, |= 0x80 when any q is nonzero.
 *
 * ORIGINAL BUG: kind 2 lights never contribute. The test after `cmp kind, 1; jne` is a second
 * `jne` on the same flags (0x0049053d), so the kind 2 block (0x00490543..0x0049073c, a
 * directional term from the polygon normal or the vertex normals, by z against 0x009f7d20) is
 * unreachable. Not ported.
 * Kind 0 distance quirk: each axis difference (a double) is multiplied by its own float rounding.
 */
#ifndef TM2_EN_FOG_H
#define TM2_EN_FOG_H

#include <stdint.h>

#include "en_vertex.h"

typedef struct {
    const uint8_t* lights;     /* 0x00c85c00, 16 x 0x78 */
    const EnClipBuf* clip;     /* 0x004e8290 clip buffer B (the clipped polygon) */
    const uint8_t* poly;       /* 0x009f7cf4 polygon record: u16 vertex indices at +4, +6, +8 */
    const uint8_t* verts;      /* 0x009f7cc4 mesh vertices (i16 x, y, z, pad) */
    int32_t wave_a, wave_b;    /* 0x009f7d04, 0x009f7d08 */
    uint32_t render_flags;     /* 0x00c846d8 */
    int32_t light_index;       /* 0x009f7ccc */
    float q[3];                /* 0x009f7d6c, 0x009f7d74, 0x009f7d68 */
} EnFogEnv;

void en_light_terms(EnFogEnv* e);

#endif /* TM2_EN_FOG_H */
