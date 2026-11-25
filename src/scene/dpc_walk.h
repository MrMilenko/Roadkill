/*
 * dpc_walk.h: DPC node tree walk, ported from TM2.EXE 0x0049f59c (old name entity_submit_model).
 *
 * Walks a relocated DPC tree from one node with an explicit stack (LIFO, 1000 deep) and collects
 * the visible mesh nodes into a render list, carrying for each one a position, a rotation slot, a
 * group counter and a scale shift. Verified against the original under emulation
 * (tools/emu/check_dpc_walk.sh) with the visibility test and the animation steps as callbacks.
 *
 * Node types as this walk treats them (byte offsets in the node):
 *   0  mesh: visible unless culled (bounds +0x1C, radius +0x28; +0x19 & 0x20 = always visible)
 *   1  group with bounds: +0x15 & 2 = skip, & 4 = no cull; bounds +0x04, radius +0x10;
 *      children +0x18, count byte +0x14
 *   2  level of detail: center +0x04; +0x10 entry count, +0x14 entries {u32 max, u32 min, child};
 *      first entry with min <= |pos + center >> shift|^2 < max (unsigned); first entry always
 *      when LOD is disabled
 *   3  animation switch: +0x0A kind (0..3 run an animation step), child +0x20 + 4 * byte +0x0C;
 *      per frame flags at +0x10 + frame index, +0x12 = keep flags
 *   4  translate: +0x13 & 2 = skip; pos += +0x04/08/0C; children +0x14, count byte +0x12; new group
 *   5  rotate + translate: +0x37 & 2 = skip; pos = R(+0x04) * (pos + +0x28/2C/30); new rotation
 *      slot (max 400); children +0x38, count byte +0x36; new group
 *   9  switch: child +0x10 + 4 * byte +0x07
 *   11 volume test: +0x04 & 2 = skip; byte +0x06 volumes at +0x10; child +0x08 when the viewer
 *      is in none of them, else +0x0C
 *   12 scale: new shift = u16 +0x04 (pos rescaled by the difference); children +0x08, count u16
 *      +0x06; new group
 *   15 group: children +0x0C, count byte +0x0A
 *   others: nothing below them is drawn
 */
#ifndef TM2_DPC_WALK_H
#define TM2_DPC_WALK_H

#include <stddef.h>
#include <stdint.h>

#define DPC_WALK_MAX_SLOTS 400
#define DPC_WALK_MAX_ITEMS 1000
#define DPC_WALK_STACK 1000

/* one render list entry (the original's 0x20 byte record at 0x00a658c8) */
typedef struct {
    uint32_t node;      /* relocated pointer to the mesh node */
    int32_t pos[3];     /* +0x04 */
    int32_t shift;      /* +0x10 */
    uint32_t root;      /* +0x14 caller's object (the walk's param_1) */
    int32_t slot;       /* +0x18 rotation slot */
    int32_t group;      /* +0x1C */
} DpcDrawItem;

typedef struct DpcWalk DpcWalk;

/* Culling test (the original's 0x004a043c). Return 1 to draw / descend; any other value culls. */
typedef int (*DpcVisibleFn)(void* user, uint32_t bounds, int32_t radius, const int32_t pos[3],
                            int32_t slot, uint8_t shift);
/* Animation step for a type 3 node (0x004a09f8, 0x004a0a78, 0x004a0b08, 0x004a0b7c by kind). */
typedef void (*DpcAnimFn)(void* user, uint32_t node, int kind);

struct DpcWalk {
    /* inputs */
    uint8_t* buf;           /* relocated DPC (type 3 nodes get their frame flags written) */
    size_t size;
    uint32_t addr_base;     /* the addr_base given to dpc_relocate */
    int lod_disabled;       /* 0x00a81c40 */
    int frame_clear;        /* 0x004e844c */
    int frame_set;          /* 0x00a81c38 */
    DpcVisibleFn visible;
    DpcAnimFn anim;
    void* user;

    /* state kept across walks, like the original's globals */
    int32_t slot_index;     /* 0x00a81c10, last used rotation slot */
    int32_t group_count;    /* 0x00a81c14 */
    int32_t item_count;     /* 0x00a81c0c */
    /* rotation slots: four chains, 9 ints used of each 12 int slot (0x00a6d5c8, 0x00a720c8,
     * 0x00a76bc8, 0x00a7b6c8) */
    int32_t mat[4][DPC_WALK_MAX_SLOTS + 1][12];
    /* per slot parent pointers, which matrix each chain composes from (0x00a801c8). New slots
     * point at their own mat entries; a root slot points at the caller's matrices (as in the
     * original, those must stay alive while later walks use the slot) */
    const int32_t* parent[DPC_WALK_MAX_SLOTS + 1][4];
    DpcDrawItem items[DPC_WALK_MAX_ITEMS];
};

/* Walks from `root` (relocated pointer). obj_pos and the four matrices are the caller's object
 * (param_1 +0x14, +0x20, chain B from the slot's existing mat[1], +0x50, +0x80); root_shift is
 * the starting scale shift (0x004e8448). obj is stored in each item's root field. Returns the
 * number of items added, or -1 on a pointer outside the buffer. */
int dpc_walk(DpcWalk* w, uint32_t root, uint32_t obj, const int32_t obj_pos[3],
             const int32_t obj_mat[12], const int32_t mat_c[9], const int32_t mat_d[9], int32_t root_shift);

#endif /* TM2_DPC_WALK_H */
