/*
 * en_scene.h: the scene drivers that feed the polygon renderer, ported from TM2.EXE
 *
 *   0x00492a40 en_entity_setup     (entity_setup_render / PS1 csUpdMat): cull the entity's model
 *                                   sphere, then build its view position and matrices for the walk
 *   0x004a0e20 en_scene_draw        (render_overlays / PS1 renderScene): scene root + 20 extra
 *                                   models + the viewport's own entity + every entity in the list
 *                                   through the tree walk, the renderer, then the queued animations
 *   0x004a10fc en_scene_draw_one    (renderDrawModel): one entity
 *   0x004a1194 en_scene_light_one   (renderDrawObjectUnculled): one entity, no culling and no LOD,
 *                                   into the light / shadow pass 0x0048b3f8 instead of the renderer
 *   0x00492c3c / 0x00492c44         entity list head [0x009f7e34] / scene root [0x009f7e38] (getters)
 * Verified under emulation: tools/emu/check_engine_scene.sh (real disc DPCs, the walk ported in
 * src/scene/dpc_walk.c).
 *
 * Entity (memory at entity address, the original's 0x168+ byte object):
 *   +0x04 u32 owner object: its +0x3c holds a 12 int matrix (the scene root's view matrix chain B)
 *   +0x08 u32 active (list entities are drawn when nonzero)
 *   +0x10 u32 view (camera) entity V: V+0x14 i32[3] position, V+0x20 / +0x50 / +0x80 i32[9]
 *             matrices (view rotation, the two culling matrices), V+0x04 owner as above
 *   +0x14 i32[3] position in view space (written)       +0x20 i32[9] matrix (written)
 *   +0x50 / +0x80 i32[9] culling matrices (written when forced)
 *   +0xb0 i32[9] rotation       +0xe8 i32[3] world position
 *   +0x15c u32 model (DPC node)  +0x160 u32 next entity  +0x166 i16 visible (written)
 *
 * The walk's globals (counters, slot matrices, render list) are the DpcWalk; view matrix chain B
 * 0x00a720c8 is DpcWalk mat[1].
 */
#ifndef TM2_EN_SCENE_H
#define TM2_EN_SCENE_H

#include <stddef.h>
#include <stdint.h>

#include "../scene/dpc_walk.h"

#define EN_SCENE_EXTRA 20
#define EN_SCENE_ANIM 100

typedef struct {
    DpcWalk* w;               /* buf / addr_base / size must cover the entities as well */
    uint32_t scene_root;      /* 0x009f7e38 */
    uint32_t list_head;       /* 0x009f7e34 */
    uint32_t viewer[2];       /* 0x00a03acc per viewport entity (0x00494c48) */
    uint32_t extra[EN_SCENE_EXTRA];   /* 0x00a81c4c models drawn with the scene root */
    uint32_t anim_node[EN_SCENE_ANIM]; /* 0x00a81c9c queued type 3 nodes */
    int32_t anim_count;       /* 0x00a81e2c */
    int32_t anim_on;          /* 0x00a81e30 */
    int32_t force_visible;    /* 0x00a81c44 g_render_force_visible */
    int32_t cull_force;       /* 0x00a81c3c (read by the walk's visibility test) */
    int32_t cull_depth;       /* 0x004e8440 (likewise) */
    int32_t cull_depth_init;  /* 0x004e8444 */
    int32_t root_shift;       /* 0x004e8448 */
    int32_t cleared[6];       /* 0x00a81c18..0x00a81c2c, zeroed by en_scene_draw */
    int32_t anim_ticks;       /* 0x00a81c48 */
    int16_t ticks60;          /* frame_get_ticks60 0x004931d4 ([0x004e8300]) */

    /* 0x0048e9a8 polygon_renderer_wrapper(param, 0x00a658c8 list, count, 0x00a6d5c8, 0x00a720c8) */
    void (*draw)(void* user, uint32_t param, int32_t count);
    /* 0x0048b3f8 (0x00a658c8 list, count, 0x00a6d5c8) */
    void (*lights)(void* user, int32_t count);
    /* type 3 animation steps 0x004a09f8 / 0x004a0a78 / 0x004a0b08 / 0x004a0b7c by kind 0..3
     * (src/engine/en_anim.h en_anim_step) */
    void (*anim)(void* user, uint32_t node, int kind);
    void* user;
} EnScene;

/* out: the 9 int view matrix of the entity's slot (DpcWalk mat[1][slot]) */
void en_entity_setup(EnScene* s, uint32_t ent, int32_t out[9], int force);
void en_scene_draw(EnScene* s, uint32_t param, int32_t view);
void en_scene_draw_one(EnScene* s, uint32_t param, uint32_t ent);
void en_scene_light_one(EnScene* s, uint32_t ent);

#endif /* TM2_EN_SCENE_H */
