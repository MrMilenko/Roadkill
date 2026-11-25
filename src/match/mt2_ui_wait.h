/*
 * mt2_ui_wait.h: two scene "wait" commands, TM2.EXE 0x0046ace8 (wait for a key or a click) and
 * 0x0046b15c (wait for a timeout), ported from the PC machine code. Both are run once per frame
 * by the scene engine (src/game/scene.c) with the command record in eax.
 *
 * Command record (handle cmd):
 *   +0x00 i32 duration in microseconds of the timer (< 0: none), +0x04 i32 copy of it when done
 *   +0x0c u32 scene handle, +0x10 i32 player (0x0046ace8: set to 0) / timeout id (0x0046b15c)
 *   0x0046ace8: +0x14 u8 key result (0xff none), +0x18 i32 start time, +0x1c i32 last time
 *   0x0046b15c: +0x14 i32 start time, +0x18 i32 last time
 * Scene (handle): +0x48 i32 current item, +0x74 i32 count / +0x78 u32[] hot spots,
 *   +0xb8 i32 count / +0xbc u32[] items (item +4: handle of a record, byte 9 at +0 = a
 *   highlightable record, its +7 u8 highlight), +0xaa0 i32 count / +0xaa4 u32[] running commands,
 *   +0xacc i32 count / +0xad0 + 8k pending events {u8 kind, u32 value at +4}, at most 150.
 * Globals: 0x004e7b98 / 0x004e7b9c i32 last mouse x / y, 0x0050fad4 i32 button latch.
 *
 * The timer test: duration >= 0 and duration < (double)(u32)(now - start) * 1e-6 (x87 at 53 bit,
 * the same as IEEE double here). When done: pending event 2 (cmd), the command leaves the running
 * list (entries after it move down; the last move reads one entry past the end), +4 = +0.
 *
 * Verified against the original under emulation (tools/emu/check_mt2_ui_wait.sh).
 */
#ifndef MT2_UI_WAIT_H
#define MT2_UI_WAIT_H

#include <stdint.h>

typedef uint32_t (*Mt2UiCall)(void* user, uint32_t addr, uint32_t eax, uint32_t edx, uint32_t ebx);

typedef struct {
    int32_t* last_xy;        /* 0x004e7b98 x, 0x004e7b9c y */
    int32_t* mouse_latch;    /* 0x0050fad4 */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    /* every other callee by its address: 0x00493038 timer (-> eax), 0x00493748 input poll,
     * 0x004b2b00 / 0x004b2ee0 (the mouse state buffer, not passed: -> eax), 0x00469e24
     * scene_obj_hit(obj, x, y), 0x00493728(0x13, player), 0x004936ac key ready(key, player) -> al,
     * 0x00493694 timeout(id) -> al */
    Mt2UiCall call;
    /* 0x004b2820 (buffer, &x, &y): fills the mouse state buffer and the position */
    void (*mouse_read)(void* user, int32_t* x, int32_t* y);
    void* user;
} Mt2UiWait;

/* 0x0046ace8 */
void mt2_ui_wait_input(const Mt2UiWait* w, uint32_t cmd);
/* 0x0046b15c */
void mt2_ui_wait_timeout(const Mt2UiWait* w, uint32_t cmd);

#endif /* MT2_UI_WAIT_H */
