/*
 * en_submit.h: turn a clipped, projected polygon into a GPU primitive and put it in the ordering
 * table, ported from TM2.EXE (outside split screen; split screen uses 0x00491018).
 *
 *   0x0048fd54 en_submit_prim    render_submit_nonplit: 0x40 line (2 vertices), 0x90 textured
 *                                n-gon with integer gradients, 0xa0 3D n-gon (float), 0xb0 flat n-gon
 *   0x0048f798 en_depth_bucket   depth -> ordering table bucket (1 .. 0xffe, coarser with distance)
 * Verified under emulation: tools/emu/check_engine_submit.sh. The record layouts are the ones the
 * renderer reads (docs/RENDERER.md, src/render/gpu_dispatch.c).
 *
 * Path by the render flags (0x00c846d8, the renderer's own flag word, which the polygon renderer
 * writes before submitting): 2 vertices: line; flag 1 (textured) without 0x10: 0x90; flag 1 with
 * 0x10: 0xa0 (0x0048f838 fills its gradients before); else 0xb0.
 * Screen x / y are rounded to nearest (fistp, the game's FPU control word 0x127f) and get bit 0 set.
 */
#ifndef TM2_EN_SUBMIT_H
#define TM2_EN_SUBMIT_H

#include <stdint.h>

#include "en_vertex.h"

typedef struct {
    const EnClipBuf* clip;  /* 0x004e8290 the clipped polygon */
    EnVertex* tri;          /* 0x009f78b0 the vertex array: its first three give the 0x90 gradients */
    uint32_t color;         /* 0x009f7d54 0x00RRGGBB */
    int32_t depth;          /* 0x009f7d58 g_depth_value */
    int32_t depth_fixed;    /* 0x009f7d48 depth given (no bias, no compression) */
    int32_t depth_bias;     /* 0x009f7d50 from the polygon record */
    uint16_t render_flags;  /* 0x00c846d8 */
    uint32_t page_ref;      /* 0x009f7e30 (high 16 bits: texture page) */
    int32_t fog;            /* 0x009f7d28 g_fog_enabled */
    float fog_dist;         /* 0x009f7d20 */
    float zmin, zmax;       /* 0x009f7d2c / 0x009f7d1c */
    float fog_sub;          /* 0x009f7d6c subtracted from the palette row with flag 0x80 */
    int32_t normalize;      /* 0x009f7d80 the polygon was clipped: reproject the three gradient vertices */
    uint8_t format;         /* 0x00a2dba5 screen pixel format (for 0x0049653c) */
    /* 0x0048f838 gradients */
    float q[3];             /* 0x009f7d6c / 0x009f7d74 / 0x009f7d68 fourth interpolant at the three
                             * gradient vertices (render flag 0x80) */
    int32_t light_index;    /* 0x009f7ccc light entry (render flag 0x4000) */
    const uint8_t* lights;  /* 0x00c85c00, 0x78 bytes per entry: +0x50/+0x54/+0x58 float position */

    /* primitive memory: records are written at prim + (cursor - prim_addr) */
    uint8_t* prim;
    uint32_t prim_addr;
    uint32_t cursor;        /* 0x009f7d78 next free record (advanced) */
    uint32_t end;           /* 0x009f7cc8 end of the vertex list written (0x90 / 0xa0 / 0xb0) */
    uint32_t rec;           /* the record: 0x009f7cd0 (line), 0x009f7d0c (0x90), 0x009f7ce4 (0xa0), 0x009f7d24 (0xb0) */

    /* 0x00496680 ordering table insert (bucket from en_depth_bucket, record address) */
    void (*ot_insert)(void* user, int32_t bucket, uint32_t rec);
    /* 0x0048c890 (vertex_project, the perspective divide; src/engine/en_clip.c en_project) on the
     * first three vertices: 0x90 when the polygon was clipped (0x009f7d80) */
    void (*normalize_fn)(void* user, EnVertex* v);
    void* user;
} EnSubmitEnv;

int32_t en_depth_bucket(const EnSubmitEnv* e, int32_t depth);
/* 0x0048f838 render_submit_effect (before en_submit_prim on the 0xa0 path): the 0xa0 record's
 * perspective correct gradients from the first three vertices (w = 1 / z at +0x30): screen
 * determinant, w gradients (+0x2c, +0x48, w0 at +0x64) and the screen origin minus 16 (+0x70 /
 * +0x74) when any of flags 0x4081; u * w, v * w (+0x14 +0x18 +0x30 +0x34, at v0 +0x4c +0x50) with
 * flag 1; q * w (+0x28 +0x44, +0x60) with 0x80; (position - light) * w (+0x1c..+0x24,
 * +0x38..+0x40, +0x54..+0x5c) and the light index (+0x0e) with 0x4000. Writes at the cursor
 * without advancing it; rec = the cursor when anything was written (0x009f7ce4). */
void en_submit_gradients(EnSubmitEnv* e);
void en_submit_prim(EnSubmitEnv* e);

#endif /* TM2_EN_SUBMIT_H */
