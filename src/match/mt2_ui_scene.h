/*
 * mt2_ui_scene.h: scene engine helpers in TM2.EXE 0x00469b60 (event dispatch to the scene's
 * object lists) and 0x00469d14 (a scene 3D model item: entity setup and draw), ported from the
 * PC machine code.
 *
 * 0x00469b60 (msg in eax; msg +2 u16 id): three lists of object handles, each object +0 u16 id,
 *   +8 u32 handle of a method table whose entry 0 is called (obj, msg):
 *     0x0050fb28 i32 count / 0x0050fb2c u32[], 0x0050fb4c / 0x0050fb50, 0x0050fb90 / 0x0050fb94.
 *   Every object with the msg's id gets it (counts read again each step); when none did and
 *   0x00510a74 (a function) is not 0 it is called (0x00510a70, msg).
 * 0x00469d14 (item in eax, draw target in edx). Item: +0x04 u32 model, +0x0c i16 visible,
 *   +0x0e i16 view mode (0: fixed camera), +0x10 i16 entity +0 value (-1: keep), +0x14 i32 camera
 *   distance, +0x18 u32 entity (created once), +0x1c / +0x20 / +0x24 i32 position,
 *   +0x28 / +0x2c / +0x30 i16 angles. Entity: +0 i32, +4 u32 light env, +0xb0 matrix,
 *   +0xe0 / +0xe2 / +0xe4 i16 angles (from item +0x2c, +0x30, +0x28), +0xe8 .. +0xf0 position.
 *   DECOMPILER TRAP: Ghidra reads +0x10 as "*(int *)(item + 0xe) >> 16".
 *   The fixed camera distance: d >= 0: d * 8 (wraps); d < 0: 8 / |d| (idiv; |INT_MIN| stays
 *   INT_MIN, giving 0).
 *
 * Verified against the original under emulation (tools/emu/check_mt2_ui_scene.sh).
 */
#ifndef MT2_UI_SCENE_H
#define MT2_UI_SCENE_H

#include <stdint.h>

typedef struct {
    const int32_t* list_a_count; const uint32_t* list_a;  /* 0x0050fb28 / 0x0050fb2c */
    const int32_t* list_b_count; const uint32_t* list_b;  /* 0x0050fb4c / 0x0050fb50 */
    const int32_t* list_c_count; const uint32_t* list_c;  /* 0x0050fb90 / 0x0050fb94 */
    const uint32_t* fallback;    /* 0x00510a70 argument, 0x00510a74 function */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    /* call a function the game holds as a value (method table entry / fallback): fn(eax, edx) */
    void (*call_fn)(void* user, uint32_t fn, uint32_t eax, uint32_t edx);
    /* direct callees by address: 0x00492d90 entity_alloc(item) -> entity, 0x00495864(0) -> light
     * env, 0x00492de4(entity, model), 0x0048c360 math_build_matrix(entity + 0xe0, entity + 0xb0),
     * 0x004948b4(0), 0x00494ec4(0, distance, 1), 0x004a10fc(target, entity, 0) */
    uint32_t (*call)(void* user, uint32_t addr, uint32_t eax, uint32_t edx, uint32_t ebx);
    void* user;
} Mt2UiScene;

/* 0x00469b60 */
void mt2_ui_scene_dispatch(const Mt2UiScene* s, uint32_t msg);
/* 0x00469d14 */
void mt2_ui_scene_model_item(const Mt2UiScene* s, uint32_t item, uint32_t target);

#endif /* MT2_UI_SCENE_H */
