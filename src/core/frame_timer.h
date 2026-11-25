/*
 * frame_timer.h: the game's frame clock, ported from TM2.EXE
 *   0x004930ac frame_timer_update   0x00493274 frame_tics_to_rate   getters 0x004931cc / 0x004931d4
 *   / 0x004931dc / 0x004931f4
 * The clock itself (timeGetTime * 1000, or QueryPerformanceCounter scaled, in 0x00493038) is the
 * platform's: the caller passes the time in microseconds, wrapping at 32 bits as the original.
 * Verified against the original under emulation (tools/emu/check_frame_timer.sh).
 */
#ifndef TM2_FRAME_TIMER_H
#define TM2_FRAME_TIMER_H

#include <stdint.h>

typedef struct {
    uint32_t last_us;       /* 0x00a03654 */
    uint32_t now_us;        /* 0x00a03658 */
    uint32_t dt_us;         /* 0x00a03650 */
    uint32_t max_fps;       /* 0x00a0365c */
    uint32_t frames;        /* 0x00a0364c (frame_parity reads it) */
    uint32_t fps;           /* 0x004e8304: clamped to 12..max_fps */
    uint32_t prev_fps;      /* 0x004e8308 */
    uint32_t raw_fps;       /* 0x004e830c: 1000000 / dt, unclamped */
    uint32_t ticks60;       /* 0x004e8300: 60 / fps, at least 1 (read as i16) */
} TM2FrameTimer;

/* dt = now - last (1000000 / max_fps when 0); raw = 1000000 / dt (unsigned); fps = raw clamped below
 * at 12 and above at max_fps; ticks60 = 60 / fps or 1; frames + 1. */
void frame_timer_update(TM2FrameTimer* t, uint32_t now_us);
/* 1000000 / tics (max_fps when tics is 0), at least 1 */
int32_t frame_tics_to_rate(const TM2FrameTimer* t, uint32_t tics);

#endif /* TM2_FRAME_TIMER_H */
