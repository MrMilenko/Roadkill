/*
 * mt2_rt.h: the in-game frame loop rtMainLoop, TM2.EXE 0x0046c6fc (PS1 decomp: rtMainLoop;
 * Ghidra / old name gameloop_run), ported from the PC machine code.
 *
 * Globals it touches:
 *   0x00520e94 u32 loop active (1 inside, 0 on exit)
 *   0x004e7c68 u32 current draw buffer handle (set to 0x00510e70 at start and every frame;
 *              callees may change it; [handle] +0 u32 is the display list given to 0x0049681c)
 *   0x00520e98 i32 target frame rate: refresh / (refresh / 20), 20 when the refresh rate is 0
 *   0x0058a93c i32 event count; entries k = 0..count-1: 0x0058a91c + 8k i32 time left,
 *              0x0058a920 + 8k u8 event (same table as mt_rt.h's 0x0058a914 + 8(k + 1))
 *   0x0058a940 u8 current event (the return value)
 *   0x0058a944 i32 total ticks
 *   0x00518e74 u32 frame timer -> 0x00518e78 u32 previous, every frame
 *   0x00c84da8 i32 display mode (1 = the one where the split path takes exclusive mode)
 *
 * Every call outside goes through one callback, call(user, address, eax, edx, ebx), the
 * original's address as its id and the register arguments it passes (0 where it passes none);
 * the result is the callee's eax (the loop tests only al where the original does).
 *
 * Verified against the original under emulation (tools/emu/check_mt2_rt.sh).
 */
#ifndef MT2_RT_H
#define MT2_RT_H

#include <stdint.h>

typedef uint32_t (*Mt2RtCall)(void* user, uint32_t addr, uint32_t eax, uint32_t edx, uint32_t ebx);

typedef struct {
    uint32_t* active;        /* 0x00520e94 */
    uint32_t* cur_buffer;    /* 0x004e7c68 */
    int32_t* target_fps;     /* 0x00520e98 */
    int32_t* event_count;    /* 0x0058a93c */
    uint8_t* events;         /* 0x0058a91c: entry k at +8k (i32 time, u8 event at +4) */
    uint8_t* event_cur;      /* 0x0058a940 */
    int32_t* ticks_total;    /* 0x0058a944 */
    uint32_t* frame_timer;   /* 0x00518e74 (and +4 = 0x00518e78) */
    const int32_t* display_mode; /* 0x00c84da8 */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    Mt2RtCall call;
    void* user;
} Mt2Rt;

/* 0x0046c6fc rtMainLoop: runs frames until a queued event's time runs out; returns it. */
uint8_t mt2_rt_main_loop(const Mt2Rt* r);

#endif /* MT2_RT_H */
