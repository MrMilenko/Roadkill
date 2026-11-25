/*
 * mt_rt.h: frame loop leaves in TM2.EXE 0x0046be2c..0x0046cc54 (event queue, draw buffer
 * setup, getters), ported from the PC machine code.
 *
 *   0x0058a93c i32 event count, 0x0058a914 + 8k (k = 1..4): u32 value, u8 event (+4);
 *   0x0058a940 u8. Draw buffers: 0x00510e70 (two of 0x8010 bytes from there), 0x00520e9c,
 *   0x00551bdc, 0x0058291c, 0x0058691c (handles = the original's addresses).
 *
 * Verified against the original under emulation (tools/emu/check_match_rt.sh).
 */
#ifndef MT_RT_H
#define MT_RT_H

#include <stdint.h>

typedef struct {
    int32_t* event_count;       /* 0x0058a93c */
    uint8_t* events;            /* 0x0058a914 (entries 1..4 at +8k) */
    uint8_t* flag_940;          /* 0x0058a940 */
    uint32_t* v_520e94;         /* 0x00520e94 */
    uint32_t* v_520e90;         /* 0x00520e90 */
    uint32_t* cur_buffer;       /* 0x004e7c68 */
    uint32_t* v_510dc4;         /* 0x00510dc4 */
    uint32_t* buf_510e70;       /* 0x00510e70 */
    uint32_t* buf_518e74;       /* 0x00518e74, 0x00518e7c, 0x00518e80 */
    uint32_t* buf_520e84;       /* 0x00520e84, 0x00520e8c */
} MtRt;

/* 0x0046ca68 rtScheduleEvent(event al, value edx): when fewer than 4 queued, appended as entry
 * count + 1; the count is stored back either way. */
void mt_rt_schedule_event(const MtRt* r, uint8_t event, uint32_t value);
/* 0x0046cb44: meant to clear the 4 entries: ORIGINAL BUG (kept): the loop never advances, so
 * only entry count + 1 is cleared (event 0, value -1); then 0x0058a940 = 0, count = 0. */
void mt_rt_events_reset(const MtRt* r);
/* 0x0046cbfc: draw buffer links: 0x00518e74 = 0x00520e9c, 0x00520e84 = 0x00551bdc,
 * 0x00510e70 = 0x0058291c, 0x00518e80 = 0x0058691c, 0x00518e7c = 0x00520e9c + 0x30cf0,
 * 0x004e7c68 = 0x00510e70, 0x00520e8c = 0x00551bdc + 0x30cf0 */
void mt_rt_buffers_init(const MtRt* r);
/* getters: 0x0046ca60 (0x00520e94), 0x0046cc54 (0x00520e90), 0x0046cba4 (0x004e7c68),
 * 0x0046be2c (0x00510dc4); constants 0x0046cb9c 0x00510e70, 0x0046cbec 0x00520e9c,
 * 0x0046cbf4 0x0058291c */
uint32_t mt_rt_get_520e94(const MtRt* r);
uint32_t mt_rt_get_520e90(const MtRt* r);
uint32_t mt_rt_cur_buffer(const MtRt* r);
uint32_t mt_rt_get_510dc4(const MtRt* r);
uint32_t mt_rt_screen_buffer(void);
uint32_t mt_rt_buffer_520e9c(void);
uint32_t mt_rt_buffer_58291c(void);

#endif /* MT_RT_H */
