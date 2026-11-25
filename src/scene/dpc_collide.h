/*
 * dpc_collide.h: point against level collision surfaces, ported from TM2.EXE 0x0048d0b8 (old
 * name math_unproject_point, wrong) with 0x0048d010 (group sphere test, old name
 * math_project_point), 0x0048dc3c (surface test) and 0x004a0ccc (surface trigger).
 *
 * Walks a relocated DPC tree with a point, like the scene walk (dpc_walk.h) but moving the point
 * into each child's frame: translate nodes subtract, rotate nodes subtract then rotate, scale nodes
 * shift. Type 8 nodes are convex collision volumes (3 to 6 planes); the first one containing the
 * point stops the walk and reports its ids. Verified against the original under emulation
 * (tools/emu/check_dpc_collide.sh).
 *
 * Type 8 node: +2 i16 id a (0 = inherit), +4 u8 plane count, +5 u8 0xFE = "report as -2" volume,
 * +6 u8 id b, +7 u8 1 = disabled, +0x0B u8 trigger mode, +0x0C trigger node,
 * +0x10 matrix + 0x34 offsets (planes 1..3), +0x40 matrix + 0x64 offsets (planes 4..6).
 */
#ifndef TM2_DPC_COLLIDE_H
#define TM2_DPC_COLLIDE_H

#include <stddef.h>
#include <stdint.h>

#include "dpc_walk.h"

typedef struct {
    int32_t hit;        /* +0x00: 1 = inside a volume */
    int32_t pad;        /* +0x04 */
    int16_t unused8;    /* +0x08 */
    int16_t id_a;       /* +0x0A */
    int16_t id_b;       /* +0x0C */
    int16_t pad2;
    int32_t stop;       /* +0x10: nonzero ends the walk (1 hit, -2 a "-2" volume) */
} DpcCollision;

typedef struct {
    uint8_t* buf;
    size_t size;
    uint32_t addr_base;
    int32_t slack;          /* 0x009f7888, added to the squared group radius */
    DpcAnimFn anim;         /* animation steps run by the surface trigger (type 3 kinds 0..3) */
    void* user;
} DpcCollideCtx;

/* 0x0048d0b8: root = the scene root's collision tree (scene root +0x15C), p = the point, shift =
 * the starting scale shift (0x004e8288). r->stop must be 0 on entry. */
void dpc_collide(const DpcCollideCtx* c, uint32_t root, const int32_t p[3], int32_t shift, DpcCollision* r);

/* 0x0048d010 (exported; EAX centre, EDX radius, EBX point): 1 when the point is within the group's
 * radius (r * r + slack, the slack 0x009f7888) or the radius is above 0xB504 (r * r overflows). */
int dpc_group_sphere(const int32_t centre[3], int32_t radius, const int32_t p[3], int32_t slack);

/* 0x0048dc3c (exported; EAX volume node, EDX point, EBX result, ECX flag): convex volume of 3..6
 * planes (node +4 the count): result +0 = 1 inside; with flag a hit becomes result +0x10 = -2
 * (returned) and +0 = 0; returns 1 inside, else 0. */
int32_t dpc_volume_test(const uint8_t* node, const int32_t p[3], DpcCollision* r, int flag);

/* 0x004a0ccc: advance (1), reset (0) or end (2) the animation switch chain starting at node. */
void dpc_trigger(const DpcCollideCtx* c, uint32_t node, uint32_t mode);

#endif /* TM2_DPC_COLLIDE_H */
