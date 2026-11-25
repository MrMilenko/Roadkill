/*
 * scene.h: the front end's scene engine (every shell screen: title, menus, car select ...), ported
 * from TM2.EXE 0x00469c40..0x0046a6xx (PS1 decomp: sceneInit / sceneAdd* / sceneRun ...,
 * docs/CAR_SELECT_CAR.md there). PC scenes are a full screen TIM background plus a DPC / TPC
 * scene model (SHELLDB1..3), drawn by objects in lists.
 *
 * The scene block (0xf9c bytes as far as known), in the original's layout; object and command
 * fields hold 32 bit handles (the original's pointers):
 *   +0x02c i32 redraw requested    +0x030 i32[4] background rect (x, y, w, h; 0, 0, 640, 480)
 *   +0x04c u32 background image record (see below)
 *   +0x050 i32 aux objects, +0x054 u32[8]: get resources, not drawn
 *   +0x074 i32 decor objects, +0x078 u32[16]: drawn first
 *   +0x0b8 i32 items, +0x0bc u32[32]: selectable, drawn second (the cursor indexes them)
 *   +0x13c i32 action rows, +0x140 {u8 event, u32 source, u8 type, u32 command}[150] (16 bytes)
 *          type 0 starts the command, 1 stops it, 2 runs it once; source 0 = any
 *   +0xaa0 i32 running commands, +0xaa4 u32[10]
 *   +0xacc i32 pending events, +0xad0 {u8 event, u32 source}[150] (8 bytes)
 *   +0xf90 i32 frame time target (60)
 * Objects and commands are C++ like: a vtable pointer at +8; a command's run is vtable[0], an
 * object's draw is vtable[1].
 *
 * The background image record (0x134 bytes): name (zero ended, up to 0x12c bytes), +0x12c u32,
 * +0x130 u8 loaded.
 *
 * Verified against the original under emulation (tools/emu/check_scene.sh).
 */
#ifndef TM2_SCENE_H
#define TM2_SCENE_H

#include <stdint.h>

#define SCENE_SIZE 0xf9c
#define SCENE_IMAGE_SIZE 0x134

/* 0x00469c40: name copied (the original copies two bytes a step, stopping at the zero), loaded = 0 */
uint8_t* scene_image_init(uint8_t* rec, const char* name, uint32_t value);

/* 0x0046a114 / 0x0046a138 / 0x0046a15c: append to the aux / decor / item list; index or -1 when full */
int32_t scene_add_aux(uint8_t* scene, uint32_t obj);
int32_t scene_add_decor(uint8_t* scene, uint32_t obj);
int32_t scene_add_item(uint8_t* scene, uint32_t obj);

/* 0x0046a18c: an action row; index or -1 when full (150) */
int32_t scene_add_action(uint8_t* scene, uint8_t event, uint32_t source, uint8_t type, uint32_t command);
/* 0x0046a2e4: a pending event; index or -1 when full (150) */
int32_t scene_post_event(uint8_t* scene, uint8_t event, uint32_t source);
/* 0x0046a0d8: request a redraw of the background rect (NULL = 0, 0, 640, 480) */
void scene_request_redraw(uint8_t* scene, const int32_t rect[4]);

/* 0x00469cc4 / 0x00469cdc: the current state (byte 7) of an object's choice node (+4, type 9).
 * node is the bytes the object's +4 points at (NULL when it is 0). */
void scene_choice_set(uint8_t* node, uint8_t state);
int8_t scene_choice_get(const uint8_t* node);

/* 0x0046a36c: every pending event against every action row, then the pending list is cleared.
 * run_once(user, command) stands for the original's call of the command's vtable[0]. */
void scene_dispatch_events(uint8_t* scene, void (*run_once)(void* user, uint32_t command), void* user);

#endif /* TM2_SCENE_H */
