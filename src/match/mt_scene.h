/*
 * mt_scene.h: the front end scene engine's setup, frame loop and redraw, ported from TM2.EXE
 * 0x00469c78..0x0046a5fc (PS1 decomp: sceneInit, sceneRun). The list bookkeeping and the event
 * dispatch are src/game/scene.c (layout of the scene block there); this adds:
 *
 *   +0x000 u32 front draw buffer   +0x004 u32 back draw buffer (0x00510e70 or 0x8010 past it)
 *   +0x008 u32 / +0x00c u32 a second pair, swapped the same way
 *   +0x010 i32[3], +0x01c i16[3]   cleared, handed to 0x00494d34 / 0x00494cdc every frame
 *   +0x024 u32 (stack argument)    +0x028 i32 = 1
 *   +0x02c i32 redraw requested    +0x030 i32[4] redraw rect, set to 1, 1, 0x27f, 0x1df
 *   +0x040 i32 quit (the frame loop runs until it is not 0)   +0x044 u8   +0x048 i32
 *   +0x04c u32 background image record (+0x12c u32, +0x130 u8 loaded)
 *   +0xf80 / +0xf84 / +0xf88 u32 (edx, ebx, ecx of sceneInit)
 *   +0xf8c i32 draw the decor with level textures   +0xf90 i32 frame rate cap (60 = none)
 *   +0xf94 i32 result of sceneRun                      +0xf98, +0xf9c i32 cleared
 * A draw buffer (0x00510e70) has +0x0000 u32 display list, +0x8004 u32, +0x8008 u32.
 * Objects and commands are C++ like: a vtable pointer at +8; a command's run is vtable[0]
 * (eax = command), an object's draw vtable[1] (eax = object, edx = draw buffer).
 *
 * Every pointer kept in a record is a 32 bit handle (the original's pointer); resolve turns one
 * into memory. Everything else the functions call goes through MtSceneEnv, in the original order.
 * Verified against the original under emulation (tools/emu/check_match_scene.sh).
 */
#ifndef MT_SCENE_H
#define MT_SCENE_H

#include <stdint.h>

typedef struct {
    uint8_t* (*resolve)(void* user, uint32_t handle);
    /* sceneInit */
    uint32_t (*screen_buffer)(void* user);                                  /* 0x0046cb9c */
    void (*input_start)(void* user);                                         /* 0x00493aa4 */
    void (*input_finalize)(void* user);                                      /* 0x00492df4 */
    void (*entity_create)(void* user, int32_t a, const uint32_t stack[6]);   /* 0x00494610 */
    void (*textures_setup)(void* user, int32_t a);                           /* 0x0049456c */
    void (*light_a)(void* user, int32_t a, int32_t b, int32_t c, int32_t d, uint32_t stack); /* 0x0049527c */
    void (*light_b)(void* user, int32_t a, int32_t b, int32_t c, int32_t d); /* 0x0049517c */
    void (*light_c)(void* user, int32_t a, int32_t b, int32_t c, int32_t d); /* 0x004952f4 */
    /* sceneRun */
    void (*pad_a)(void* user, uint8_t* p, int32_t b);                        /* 0x00494d34 (scene + 0x10, 0) */
    uint32_t (*pad_b)(void* user, uint8_t* p, int32_t b);                    /* 0x00494cdc (scene + 0x1c, 0) */
    void (*input_poll)(void* user, uint32_t v);                              /* 0x00493748 */
    void (*input_update)(void* user);                                        /* 0x0049364c */
    uint32_t (*timer_now)(void* user);                                       /* 0x00493038 */
    void (*render_effects)(void* user);                                      /* 0x004a0dd4 */
    void (*run_command)(void* user, uint32_t command);                       /* command vtable[0] */
    int32_t (*tics_to_rate)(void* user, uint32_t tics);                      /* 0x00493274 */
    void (*idle)(void* user, int32_t a);                                     /* 0x00495890 (0) */
    void (*frame_timer_update)(void* user);                                  /* 0x004930ac */
    /* redraw */
    void (*bg_draw)(void* user, int32_t a, int32_t b, int32_t c, int32_t d); /* 0x0046f7c4 */
    void (*draw_begin)(void* user);                                          /* 0x0049632c */
    void (*buffer_reset)(void* user, uint32_t buffer, int32_t v);            /* 0x004966d8 */
    void (*draw_object)(void* user, uint32_t object, uint32_t buffer);       /* object vtable[1] */
    void (*list_flush)(void* user, uint32_t list);                           /* 0x0049681c */
    void (*draw_end)(void* user);                                            /* 0x0049635c */
    void (*present_rect)(void* user, uint8_t* rect);                         /* 0x00495b20 (scene + 0x30) */
    /* image / resources */
    void (*bg_load)(void* user, uint8_t* record, int32_t b);                 /* 0x0046f784 */
    void (*file_load)(void* user, uint32_t v);                               /* 0x004740a8 */
    void* user;
} MtSceneEnv;

/* 0x00469f08 sceneInit(scene, a -> +0xf80, b -> +0xf84, c -> +0xf88, stack -> +0x24): both
 * buffer pairs = screen_buffer(), counts and flags cleared, redraw rect 1, 1, 0x27f, 0x1df, frame
 * cap 60; input start / finalize, entity_create(0, {0, 0, 1.0f, 1.0f, 0x46, 0x23}),
 * textures_setup(0), light_a(0, k, 0xff, 0xff, 0xff) and light_b(0, k, 200, 30) for k = 0..2,
 * light_c(0, 0x20, 0x20, 0x20). DECOMPILER TRAP: Ghidra drops the six stack arguments of
 * 0x00494610 and the fifth of 0x0049527c, and names the fields at +0xf98 / +0xf9c [0x3e6] / [999]. */
void mt_scene_init(uint8_t* scene, uint32_t a, uint32_t b, uint32_t c, uint32_t stack, const MtSceneEnv* env);

/* 0x0046a5fc sceneRun: one pad / input step, posts event 0 (source 0) when the pending list has
 * room, then until +0x40 is set: render_effects, swap both buffer pairs, run every running
 * command, dispatch events (0x0046a36c), redraw (0x0046a4f8), then while the frame rate cap
 * (+0xf90) is below 60 idle until tics_to_rate(now - start) is not above it, frame timer update.
 * Returns +0xf94. */
int32_t mt_scene_run(uint8_t* scene, const MtSceneEnv* env);

/* 0x0046a4f8: when a redraw is requested: the background image if loaded
 * (bg_draw(buffer != screen_buffer(), image +0x12c, 0, 0); DECOMPILER TRAP: Ghidra shows both
 * calls without arguments), buffer +0x8008 = +0x8004, decor objects (with level textures when
 * +0xf8c), items, flush, present the rect, redraw flag cleared. */
void mt_scene_draw(uint8_t* scene, uint32_t buffer, const MtSceneEnv* env);

/* 0x00469e24: is (x, y) inside a scene object's hit area: +0x44 != 0: the open rect
 * +0x34 < x < +0x3c, +0x38 < y < +0x40; else the polygon +0x48 (i32 x, y pairs, +0x4c count):
 * no edge with the point on its right (32 bit wrapping cross products); count <= 0: outside. */
int32_t mt_scene_hit(const uint8_t* object, int32_t x, int32_t y, const MtSceneEnv* env);

/* 0x00469c78: loads the background image record's TIM (bg_load(record, 0)), loaded = 1. */
void mt_scene_image_load(uint8_t* record, const MtSceneEnv* env);

/* 0x0046a324: idle(0), file_load(b), returns 1 (a, c, d unused). */
int32_t mt_scene_load_resources(uint32_t a, uint32_t b, const MtSceneEnv* env);

#endif /* MT_SCENE_H */
