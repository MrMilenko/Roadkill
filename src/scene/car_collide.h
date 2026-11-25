/*
 * car_collide.h: an entity's collision points against the level and other entities, ported from
 * TM2.EXE 0x0049cec0 (entry), 0x0049a1b4 (tree walk), 0x0049a080 (walk push), 0x004bca88 (grid
 * reject) and 0x0048b5a0 (vector normalize).
 *
 * This is how cars hit walls. Each car entity carries a list of collision points (entity +0xF4
 * count, +0xF8 packed i16 x, y, z, 8 bytes apart). The entry moves them into world space with
 * the entity matrix (+0xB0) and position (+0xE8), then walks the level collision tree (scene root
 * entity +0x15C) and every other collidable entity's tree with the entity's position, like
 * dpc_collide (dpc_collide.h) but carrying the rotations so each point can be tested in a
 * volume's frame. A point inside a type 8 volume gets a hit record with the push normal (back in
 * world axes), the depth and the volume's ids. The face is picked from where the point was last
 * frame (prev matrix and position), so a car sliding along a wall is pushed out sideways.
 * Volumes with +0x09 set are spheres (1 = keep out, 2 = keep in). "-2" volumes (+0x05 0xFE) mark
 * points as fully outside the level.
 *
 * Hit record (0x20 bytes per point, at car +0x234 player / +0x648 AI):
 *   +0x04 i32 state (0 none, 1 hit, 2 ignored this frame)   +0x08..0x10 i32 normal
 *   +0x14 i32 depth                                          +0x18 u32 entity that was hit
 *   +0x1C u16 volume +0x08 byte  +0x1E u16 cleared  +0x20 u16 id b  +0x22 i16 id a (-1 none)
 * The first record's +0x00 is cleared once per call (original quirk).
 *
 * The tree walk keeps the original's shared rotation list: a rotate node writes its matrix at
 * the current depth, so siblings still waiting on the stack may see a deeper node's matrix.
 * Type 15 (height field) volumes are not ported: there are none on the disc.
 * Verified against the original under emulation (tools/emu/check_car_collide.sh).
 */
#ifndef TM2_CAR_COLLIDE_H
#define TM2_CAR_COLLIDE_H

#include <stddef.h>
#include <stdint.h>

#include "dpc_collide.h"

#define CAR_COLLIDE_MAX_POINTS 12
#define CAR_COLLIDE_MAX_DEPTH 32

/* 0x004bca88: occupancy bitmap of the level (globals 0x00c85bd0..0x00c85bfc). */
typedef struct {
    int32_t x, y, z;        /* 0x00c85bd0 origin */
    int32_t sx, sy, sz;     /* 0x00c85bdc extents */
    int32_t cell;           /* 0x00c85be8 */
    int32_t row;            /* 0x00c85bec cells per row */
    int32_t layer;          /* 0x00c85bf8 cells per layer */
    const uint32_t* bits;   /* 0x00c85bfc, NULL = no grid */
} CollideGrid;

/* 1 when the point is off the grid or in an empty cell (the caller skips such points). */
int collide_grid_reject(const CollideGrid* g, int32_t x, int32_t y, int32_t z);

/* The globals the walk reads and writes (0x00a3c890..0x00a3cfdc). */
typedef struct {
    /* in */
    const CollideGrid* grid;
    int32_t shift;              /* 0x004e8430 world scale shift */
    uint32_t scene_root;        /* 0x009f7e38 entity whose +0x15C is the level tree */
    uint32_t entity_head;       /* 0x009f7e34, list linked through entity +0x160 */
    int32_t sphere_mode;        /* 0x00a3cf80: "-2" volumes test the entity's sphere, not points */
    uint8_t floor_kind;         /* 0x00a3cf9c: 1 plane, 2 sphere, else none */
    uint8_t floor_id;           /* 0x00a3cf9d */
    int16_t floor_n[3];         /* 0x00a3cf90 plane normal */
    int32_t floor_d;            /* 0x00a3cf98 plane offset */
    uint16_t floor_surface;     /* 0x00a3cfa0 */
    int32_t floor_c[3];         /* 0x00a3cfa4 sphere centre */
    int32_t floor_r2;           /* 0x00a3cfb0 sphere radius squared */
    /* state (kept between the walk calls of one entry call, readable after) */
    int32_t remaining;          /* 0x00a3cf78 points still to resolve */
    int32_t all_out;            /* 0x00a3cf7c */
    int32_t current;            /* 0x00a3cf84 entity being walked, stored in hit records */
    int32_t radius;             /* 0x00a3cf88 */
    int32_t first_hit_stops;    /* 0x00a3cf8c */
    int32_t depth;              /* 0x00a3c898 rotations carried into the walk */
    uint32_t mats[CAR_COLLIDE_MAX_DEPTH]; /* 0x00a3c89c matrix addresses, outermost first */
    uint32_t entity;            /* 0x00a3cee4 */
    uint32_t prev_mat;          /* 0x00a3cee0 last frame's matrix (packed multiply source) */
    uint32_t prev_pos;          /* 0x00a3cedc last frame's position */
    int32_t world[CAR_COLLIDE_MAX_POINTS][3]; /* 0x00a3cee8 points in world space */
    int32_t trigger_count;      /* 0x00a3cfb4 */
    uint32_t triggers[10];      /* 0x00a3cfb8 volumes whose trigger runs after the walk */
} CarCollideState;

/* 0x0048b5a0: out = v scaled to length 0x1000 (0xFFF0000 / isqrt(|v|^2), >> 16). */
void collide_normalize(const int32_t v[3], int32_t out[3]);

/* 0x0049cec0: entity, its hit records, level = test the level tree (param 3), prev_mat and
 * prev_pos = last frame's matrix and position (addresses). c gives the memory (one relocated
 * region holding the trees, entities and records) and the trigger animation callback. */
void car_collide(const DpcCollideCtx* c, CarCollideState* s, uint32_t entity, uint32_t records,
                 int level, uint32_t prev_mat, uint32_t prev_pos);

#endif /* TM2_CAR_COLLIDE_H */
