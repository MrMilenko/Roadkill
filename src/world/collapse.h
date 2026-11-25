/*
 * collapse.h: collapsing arena sections (the ice arena's breaking floor), ported from TM2.EXE
 * (PS1 decomp: worldCollapse*, src/semantic/world_collapse*.c, world_object_register.c).
 *
 * The section table at 0x00502988: 35 records of 0x24 bytes, one per GRP zone (section):
 *   +0x00 u32 object (handle; its kind byte +0: 4 keeps z at +0x0C and "active" at +0x12,
 *             5 at +0x30 and +0x36)
 *   +0x04 i32 z step this frame      +0x08 i32 phase timer        +0x10 i32 count (added up)
 *   +0x14 i32 passed to the AI reroute call
 *   +0x18 u8 armed      +0x19 u8 falling       +0x1A u8 shaking
 *   +0x1C..+0x21 i8 linked sections along the six edges (-1 = none)
 * The state that follows it at 0x00502e74 (offsets from there):
 *   +0x00 u8 busy (a section is collapsing)   +0x04 i32 that section   +0x08 i32 total count
 *   +0x0C i32 done count (picks the group of sections that may go next)
 *   +0x10 i32 timer (60 Hz ticks)             +0x14 i32 debris cooldown   +0x18 i32 blasts
 * GRP fields used (block 0x004fca18): zone origin x, y, z at +0x0C + 12 s, extents u at
 * +0x1B0 + 4 s and v at +0x23C + 4 s (see src/physics/grp.h). The z of a fallen zone is set to -1.
 * The marker object (pointer at 0x00502f5c): +0x08 i32 visible, +0xE8/+0xEC/+0xF0 position.
 *
 * Verified against the original under emulation (tools/emu/check_world_collapse.sh).
 */
#ifndef TM2_WORLD_COLLAPSE_H
#define TM2_WORLD_COLLAPSE_H

#include <stdint.h>

#define WORLD_COLLAPSE_SECTIONS 35
#define WORLD_COLLAPSE_REC 0x24

typedef struct {
    uint8_t* table;             /* 0x00502988 */
    uint8_t* state;             /* 0x00502e74 */
    uint8_t* grp;               /* 0x004fca18 */
    uint32_t marker;            /* *(0x00502f5c), 0 = none */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    int32_t (*car_count)(void* user);                       /* 0x0045afb4 */
    const uint8_t* (*car)(void* user, int32_t i);           /* 0x0045afbc: player car block */
    int32_t (*game_mode)(void* user);                       /* 0x0045c338 */
    int (*spawn_blast)(void* user, int32_t* p);             /* 0x00457314 (result in al) */
    /* fx(which, p, 0): 0 = 0x00450be0, 1 = 0x00450b94, 2 = 0x004512f8, 3 = 0x004512b4 */
    void (*fx)(void* user, int which, int32_t* p, int32_t arg);
    void (*section_disable)(void* user, int32_t s);         /* 0x0042f2dc */
    void (*section_start)(void* user, int32_t s);           /* 0x0042f520 */
    void (*ai_reroute)(void* user, int32_t s, int32_t v);   /* 0x0042f6d4 */
    void (*section_move)(void* user, int32_t s, int32_t dz);/* 0x0042f7d0 (full i32; Ghidra
                                                               shows a short: DECOMPILER TRAP) */
    void (*section_sink)(void* user, int32_t s, int32_t v); /* 0x0042f850 */
    void (*set_step)(void* user, int32_t id, int32_t a, int32_t b);  /* 0x00470320 */
    void* user;
} WorldCollapseEnv;

/* 0x00454fb8 [worldCollapseRegister]: an object of kind 4 (section at +0x10, u16) or 5 (+0x34)
 * becomes its section's object, armed, and the section's count is added to the total. */
void world_collapse_register(const WorldCollapseEnv* env, uint32_t handle, const uint8_t* obj);

/* 0x00455954 [worldCollapseUnlink]: every edge link of every section that names s becomes -1. */
void world_collapse_unlink(const WorldCollapseEnv* env, int32_t s);

/* 0x00455014 [worldCollapseClearDone]: for sections 0 .. done count - 1 (reread every step):
 * unlink, disarm, section_disable. */
void world_collapse_clear_done(const WorldCollapseEnv* env);

/* 0x00455678 [worldCollapseMoveSection]: moves section s (its object and its GRP zone z) by its
 * z step; sections 6, 13, 19, 22 and 26 also move the zones after them (7 and 8, 14, 20, 23, 27)
 * by those zones' own steps. Places the marker on the section's centre and set_step(0xC1F, 0, 1).
 * PC differs from PS1: no platform sound call, the marker instead. */
void world_collapse_move_section(const WorldCollapseEnv* env, int32_t s);

/* 0x00455d34 [worldCollapseDebris]: maybe a blast at p (z lowered by rand % 120; at most
 * timer / 30 + 1 per collapse), then, unless one went off or the game mode is 2 or above 3
 * (PC differs from PS1, which tests 2 and 4), counts the cooldown down or spawns one of the
 * four fx kinds and restarts the cooldown (120 / 60 / 20 by timer). */
void world_collapse_debris(const WorldCollapseEnv* env, int32_t* p);

/* 0x004559e0 [worldCollapseExplosions]: timer < 300 ? timer / 60 + 1 : 1 rounds; each round puts
 * debris at a random point along each linked edge of section s. */
void world_collapse_explosions(const WorldCollapseEnv* env, int32_t s);

/* 0x00455050 [worldCollapseUpdate]: per frame. While idle, after 360 ticks, picks the next armed
 * section of the current group (by done count: 0..5, 6..17, 18..24, 25..28) that a player car
 * sits on (all four probe zones, up to 780 ticks) or, later, faces (within 0xE3) or by chance
 * (after 1080 ticks); it starts shaking (480 ticks of a random wobble, kept within z 0x1290..
 * 0x1338), then falls with a quadratic speed until its z is below 0, then is done (count added,
 * zones sunk, marker hidden). */
void world_collapse_update(const WorldCollapseEnv* env);

#endif /* TM2_WORLD_COLLAPSE_H */
