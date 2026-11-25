/*
 * en_debris.h: debris (flying chunks and burst models), ported from TM2.EXE (PS1 debris.c names):
 *
 *   0x00489a00 debrisInit          8 chunk entities + 2 burst entities from the entity pool
 *   0x00489a70 debrisClear         all chunks hidden and off, all bursts off
 *   0x00489ab0 debrisUpdateChunks  per frame: chunk spin / move / gravity / expiry, burst motion
 *   0x0048a738 debrisBurstAt       burst of a model node at a world position
 *   0x0048a8b8 debrisStartBurst    takes a free burst slot, random piece velocities and spin
 *   0x0048a488 debrisBreakModel    a model's type 4 children fly off as chunks, the rest bursts
 *   0x0048b3f8 (PS1 pair lightAddSources: wrong) debris model capture: render list -> burst meshes
 *
 * Data (one block at 0x009f6d8c..0x009f7797, layout kept byte for byte in EnDebrisData):
 *   0x009f6d8c 8 chunks x 0x24:
 *     +0x00 i32 velocity x, y, z (z -= 1 per frame once age > 10)
 *     +0x0c i16 spin x, y, z (added to the entity angles +0xe0/+0xe2/+0xe4)
 *     +0x14 entity (pool entry 0x170 bytes: +0xb0 matrix, +0xe0 angles, +0xe8 position)
 *     +0x18 active, +0x1c age, +0x20 life (hidden and off once age > life)
 *   0x009f6eac holder entity, 0x009f6eb0 draw entity (its +0x10 = the holder)
 *   0x009f6eb4 4 bursts x 0x238:
 *     +0x00 active, +0x04 captured mesh count (cleared on start / end), +0x08 frame, +0x0c end frame,
 *     +0x10 hold frames (pieces drift together until frame > hold),
 *     +0x14 4 x i32[3] piece velocity, +0x44 4 x i32[3] piece offset,
 *     +0x74 4 x i32[3] piece world position (offset + mesh 0's position +0xfc), +0xa4 i32[3] drift,
 *     +0xb0 i16[3] spin after the hold, +0xb8 i16[3] angles, +0xc0 i16[3] spin during the hold,
 *     +0xc8 i32[9] rotation (from the angles), +0xf8 16 captured meshes x 0x14 {node, i32[3]
 *     position, shift}, filled by the model capture 0x0048b3f8 through 0x009f7794
 *   0x009f7794 burst being captured (set by debrisStartBurst)
 *
 * Entities are handles (the original's addresses); env->entity() resolves one to its bytes.
 */
#ifndef TM2_EN_DEBRIS_H
#define TM2_EN_DEBRIS_H

#include <stdint.h>

#include "../scene/dpc_walk.h"   /* DpcDrawItem */

#define EN_DEBRIS_CHUNKS 8
#define EN_DEBRIS_BURSTS 4

typedef struct {
    int32_t vel[3];        /* +0x00 */
    int16_t spin[3];       /* +0x0c */
    int16_t pad;           /* +0x12 */
    uint32_t entity;       /* +0x14 */
    int32_t active;        /* +0x18 */
    int32_t age;           /* +0x1c */
    int32_t life;          /* +0x20 */
} EnDebrisChunk;

/* one captured mesh of a burst (0x14 bytes, from the render list by 0x0048b3f8) */
typedef struct {
    uint32_t node;           /* +0x00 mesh node */
    int32_t pos[3];          /* +0x04 position in the model frame (slot matrix * item position) */
    int32_t shift;           /* +0x10 scale shift */
} EnDebrisPiece;

typedef struct {
    int32_t active;          /* +0x00 */
    int32_t f04;             /* +0x04 */
    int32_t frame;           /* +0x08 */
    int32_t end;             /* +0x0c */
    int32_t hold;            /* +0x10 */
    int32_t vel[4][3];       /* +0x14 */
    int32_t pos[4][3];       /* +0x44 */
    int32_t world[4][3];     /* +0x74 */
    int32_t drift[3];        /* +0xa4 */
    int16_t spin_after[3];   /* +0xb0 */
    int16_t pad0;
    int16_t angle[3];        /* +0xb8 */
    int16_t pad1;
    int16_t spin_hold[3];    /* +0xc0 */
    int16_t pad2;
    int32_t mat[9];          /* +0xc8 */
    int32_t fec[3];          /* +0xec (not touched here) */
    EnDebrisPiece piece[16]; /* +0xf8 captured meshes (count +0x04) */
} EnDebrisBurst;

typedef struct {
    EnDebrisChunk chunk[EN_DEBRIS_CHUNKS];   /* 0x009f6d8c */
    uint32_t holder;                         /* 0x009f6eac */
    uint32_t draw;                           /* 0x009f6eb0 */
    EnDebrisBurst burst[EN_DEBRIS_BURSTS];   /* 0x009f6eb4 */
    uint32_t capture;                        /* 0x009f7794 (the burst's address in the original) */
} EnDebrisData;

typedef struct {
    EnDebrisData* d;
    uint32_t burst_base;   /* handle of burst 0 as stored in capture (0x009f6eb4 in the original) */
    void* user;
    uint8_t* (*entity)(void* user, uint32_t e);          /* entity bytes (0x170) */
    uint32_t (*alloc)(void* user);                       /* csAlloc 0x00492d90 (0 when full) */
    void (*list_add)(void* user, uint32_t e);            /* csListAdd 0x00492c4c */
    void (*set_draw)(void* user, uint32_t e, int on);    /* csSetDrawEnable 0x00492e50 */
    void (*draw_unculled)(void* user, uint32_t e);       /* 0x004a1194 (captures the model) */
    int32_t (*rand)(void* user);                         /* rand 0x004899c2 */
    void (*set_model)(void* user, uint32_t e, uint32_t model);   /* csSetModel 0x00492de4 */
    const uint8_t* (*node)(void* user, uint32_t p);      /* model node bytes (debrisBreakModel) */
} EnDebrisEnv;

void en_debris_init(EnDebrisEnv* e);
void en_debris_clear(EnDebrisEnv* e);
void en_debris_update(EnDebrisEnv* e);
/* dir: two i16 (x, y) packed like the original's int; spin / drift may be NULL. */
void en_debris_start_burst(EnDebrisEnv* e, uint32_t ent, const int16_t dir[2], const int16_t* spin,
                           int32_t hold, int32_t scale, int32_t end, int32_t random_spin, const int32_t* drift);
/* node: model node address (0 = nothing) and its bytes (type +0, a type 4 node's offset +4..+0xc
 * is taken off the position). */
void en_debris_burst_at(EnDebrisEnv* e, uint32_t node_addr, const uint8_t* node, const int32_t pos[3],
                        const int16_t dir[2], const int16_t* spin, const int32_t* drift, int32_t hold,
                        int32_t scale, int32_t end, int32_t random_spin);

/* 0x0048a488 (EAX ent, EDX node, EBX target, ECX dir, stack scale, life; ret 8).
 * ent: the breaking entity (+0xb0 matrix, +0xe8 position, +0x08 / +0x10 written);
 * node: group node bytes (+0x14 u8 child count, set to 0 when a chunk was made; children +0x18),
 * env->node(): resolves a child pointer to its bytes (type +0; type 4: +0x04 i32[3] offset, +0x14
 * model). Each type 4 child takes a free chunk: model, drawn, position ent + M^T * offset,
 * 1 in 64 straight along dir (x, y) * scale >> 12, else 3 * scale toward the target
 * (normalized to 0x1000) plus a growing 0, 3, 6 ... bias; spin (r + 15, rand & 63 + 10,
 * rand & 63 + 10). Then a burst of the rest: debrisStartBurst(ent, dir, no spin, hold 0, scale,
 * end = life, random spin, no drift) with ent +0x10 = the holder entity, then ent +0x08 = 0
 * (hidden) and +0x10 restored. */
void en_debris_break_model(EnDebrisEnv* e, uint32_t ent, uint8_t* node, const int32_t target[3],
                           const int16_t dir[2], int32_t scale, int32_t life);

/* 0x0048b3f8 (EAX items, EDX count, EBX matrices): for the first 16 items of the render list,
 * burst (capture) mesh i = {node, matrix[slot] * pos (M * v), shift}, mesh count += 1. */
void en_debris_capture(EnDebrisEnv* e, const DpcDrawItem* items, int32_t count, const int32_t (*mats)[12]);

#endif /* TM2_EN_DEBRIS_H */
