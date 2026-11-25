/*
 * en_poly.h: the scene's polygon renderer, ported from TM2.EXE 0x0048e9b8 (old name
 * polygon_renderer; 0x0048e9a8 is a plain wrapper). For every render list item of the DPC tree
 * walk (src/scene/dpc_walk.h DpcDrawItem: a type 0 mesh node, its position, rotation slot and
 * scale shift) it loads the view transform, then for every polygon record of the mesh:
 * back face test, vertices into view space (or a billboard / line quad), near / side / far
 * clipping, depth, texture page and flags, projection, fog, and one GPU primitive into the
 * ordering table (software GPU: src/engine/en_submit.c; with the hardware 3D device active,
 * [0x00c85008] behind the old name screen_is_split 0x004958ac: src/engine/en_split.c).
 * Verified under emulation: tools/emu/check_engine_poly.sh.
 *
 * Mesh node (type 0): +0x02 u16 kind (699 water: animated uv; 700 special, drawn only in the
 * special render mode, with render flag 2), +0x04 vertices (8 bytes: i16 x, y, z, pad),
 * +0x08 (kept in 0x009f7d3c), +0x0c polygon records, +0x10 u32 polygon count, +0x18 u32 bits
 * 19..30 fixed depth, used when u16 +0x1a & 0x7ff8.
 * Polygon record (src/formats/dpc_mesh.h): +0 u8 uv count (& 0x7f), +1 u8 shading count, +2 u8
 * size in words, +4 i16 vertex indices, +0x0c / +0x0e / +0x10 i16 face normal (nz 0x2000 = no
 * back face test, winding test instead), +0x10 bits 16..18 vertex count (0 billboard, 1 line),
 * 19..21, 24..27 normal words, 28..30 color words; +0x14 bits 1..6 fog distance (0x3f = none),
 * 7..12 signed depth bias; +0x18 colors, then uvs (u16 u, v per vertex), then the page word
 * (& 0x1ff page, & 0x3000 blend).
 * Floating point as in the rest of src/engine (x87 control word 0x127f: 53 bit, nearest).
 */
#ifndef TM2_EN_POLY_H
#define TM2_EN_POLY_H

#include <stdint.h>

#include "en_clip.h"
#include "en_vertex.h"

#define EN_POLY_POOL 20   /* 0x009f78b0 .. 0x009f7cc0 */

typedef struct EnPoly EnPoly;

struct EnPoly {
    /* memory of the caller: a 32 bit address (DPC nodes, polygons, vertices, entities) to bytes */
    uint8_t* (*mem)(void* user, uint32_t addr);
    void* user;

    /* inputs read by the original through small getters */
    int32_t view_w, view_h;  /* level_get_viewport_data 0x00493880: +0 / +8 of the current view */
    uint32_t elapsed;        /* timer_get_elapsed 0x00493038 (platform) */
    uint32_t fog_rgb;        /* 0x00494dc0: bytes 0x00a03b64 / 68 / 6c as 0x00RRGGBB */
    uint32_t camera_obj;     /* 0x00494c48(0) = [0x00a03acc] */
    int split;               /* 0x004958ac (old name screen_is_split): the hardware 3D device renderer
                              * is active ([0x00c85008]); not split screen */
    uint8_t format;          /* 0x00a2dba5 */
    /* settings */
    double fov_base;         /* 0x004e2d6c */
    int32_t special_mode;    /* 0x004e82cc g_special_render_mode */
    int32_t fog_mode;        /* 0x004e82b0 */
    int32_t fog_on;          /* 0x004e85f8 */
    uint32_t depth_scale;    /* 0x004e82b4 (raw float bits) */
    int32_t alpha_test;      /* 0x004e82b8 */
    float alpha_z;           /* 0x004e82bc */
    float fog_k;             /* 0x004e85fc */
    double fog_k2;           /* 0x004e2d74 */
    uint32_t fog_color;      /* 0x004e8608 */
    int32_t q_enable;        /* 0x004e8e10 */
    const uint8_t* tex_table;/* 0x00a30c48, 0x30 per page */
    int32_t tex_count;
    const uint8_t* lights;   /* 0x00c85c00 */
    int32_t light_index;     /* 0x009f7ccc */
    EnProj proj;             /* 0x009f7810 */

    /* state (globals of the original, kept across calls) */
    EnXform xf;              /* 0x009f77d0 */
    float fov_x, fov_y, inv_fov_x, inv_fov_y;   /* 0x009f7ce0, 0x009f7ce8, 0x009f7cf8, 0x009f7cec */
    int32_t wave_a, wave_b;  /* 0x009f7d04, 0x009f7d08 */
    uint32_t fog_rgb_copy;   /* 0x009f7d10 */
    uint32_t page_ref;       /* 0x009f7e30: low 16 the fog color in screen format, high the page */
    uint32_t render_flags;   /* 0x00c846d8 (written as a dword) */
    uint32_t interp_mask;    /* 0x009f7d44 */
    uint32_t shade;          /* 0x009f7d34 (only set in split screen, stale otherwise) */
    uint32_t item;           /* 0x009f7d4c current render item (address) */
    uint32_t verts, node8, poly;                 /* 0x009f7cc4, 0x009f7d3c, 0x009f7cf4 */
    uint32_t kind;           /* 0x009f7d18 */
    int32_t tex_shift;       /* 0x009f7cc0 */
    float tex_inv;           /* 0x009f7d14 */
    int32_t fog;             /* 0x009f7d28 g_fog_enabled */
    int32_t far_clip;        /* 0x009f7cfc g_fog_active */
    uint32_t color;          /* 0x009f7d54 */
    int32_t clipped;         /* 0x009f7d80 */
    int32_t depth_fixed;     /* 0x009f7d48 */
    uint32_t normals;        /* 0x009f7d64 */
    int32_t semi;            /* 0x009f7d38 */
    int32_t depth_bias;      /* 0x009f7d50 */
    uint32_t bits19;         /* 0x009f7d70 */
    float size_x, size_y;    /* 0x009f7d00, 0x009f7d30 */
    float zmax, zmin;        /* 0x009f7d1c, 0x009f7d2c */
    int32_t depth;           /* 0x009f7d58 */
    uint32_t blend;          /* 0x009f7d7c */
    float fog_dist;          /* 0x009f7d20 */
    float q[3];              /* 0x009f7d6c, 0x009f7d74, 0x009f7d68 */
    EnVertex vtx[EN_POLY_POOL];   /* 0x009f78b0 */
    EnClipBuf a, b;          /* 0x009f7ddc, 0x009f7d84 */
    int b_is_a;              /* 0x004e8290 points at a (after the far clip) */

    /* primitives: the display context param_1: +0 ordering table base, +0x8008 cursor */
    uint8_t* prim;
    uint32_t prim_addr;
    uint32_t ot_base;
    uint32_t cursor;         /* in: the context's +0x8008 (the original copies it into 0x009f7d78); out: advanced */
    uint32_t rec, end, end_split;   /* the record pointers of the last primitive */
    /* 0x00496680 (full screen, bucket) and 0x004966cc (split screen, slot address) */
    void (*ot_insert)(void* user, int32_t bucket, uint32_t rec);
    void (*ot_link)(void* user, uint32_t slot, uint32_t rec);
    /* 0x0049024c (fills q[]); NULL: not called */
    void (*fog_q)(void* user, EnPoly* p);
};

/* items: count render list entries (0x20 bytes each, DpcDrawItem layout) at items_addr; mats:
 * the slot matrices (0x30 bytes per slot, chain 0 of the walk) at mats_addr. */
void en_poly_render(EnPoly* p, uint32_t items_addr, int32_t count, uint32_t mats_addr);

/* 0x00489d20 fxDebrisDraw: the debris bursts (4 x 0x238 at bursts_addr = 0x009f6eb4, see
 * src/engine/en_debris.h) drawn through the same leaves: the view is the scene root's rotation
 * (root +0x20) times the transposed burst matrix (+0xc8), no translation; every captured mesh
 * (+0xf8 + 0x14 m: node, i32 position, shift) is split into four pieces by the polygon bits
 * 13..15 of +0x14 (offsets +0x44 + 12 k accumulate: piece k has the offsets 0..k). Full screen
 * primitives only, render flags 0x10. ORIGINAL BUG: piece index 4..7 reads the stack past the
 * four pieces (4..6 the transposed matrix, 7 words never written: the port reads zeros). */
void en_poly_debris(EnPoly* p, uint32_t bursts_addr, uint32_t root_addr);

#endif /* TM2_EN_POLY_H */
