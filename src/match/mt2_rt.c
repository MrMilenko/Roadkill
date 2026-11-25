/*
 * mt2_rt.c: rtMainLoop, TM2.EXE 0x0046c6fc. See mt2_rt.h.
 */
#include "mt2_rt.h"

#include <string.h>

#define BUF_510E70 0x00510e70u
#define DDRAW 0x00c84840u

static int32_t rd32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static void wr32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }

#define CALL(a, x, y, z) r->call(r->user, (a), (x), (y), (z))

/* 0x004931d4 frame_get_ticks60: the loop sign extends its 16 bit result (cwde) */
static int32_t ticks60(const Mt2Rt* r) { return (int16_t)CALL(0x004931d4, 0, 0, 0); }

/* the queue part at 0x0046c77f: 1 when an event fired */
static int queue_step(const Mt2Rt* r)
{
    int32_t found = -1, k;
    if (*r->event_count == 0)
        return 0;
    /* every entry's time goes down; the last one at or below 0 wins (count read each step) */
    for (k = 0; k < *r->event_count; k++) {
        uint8_t* t = r->events + 8 * k;
        wr32(t, (int32_t)((uint32_t)rd32(t) - (uint32_t)ticks60(r)));
        if (rd32(t) <= 0)
            found = k;
    }
    if (found == -1)
        return 0;
    *r->event_cur = r->events[8 * found + 4];
    for (k = found + 1; k < *r->event_count; k++) {
        r->events[8 * (k - 1) + 4] = r->events[8 * k + 4];
        memcpy(r->events + 8 * (k - 1), r->events + 8 * k, 4);
    }
    *r->event_count -= 1;
    return 1;
}

uint8_t mt2_rt_main_loop(const Mt2Rt* r)
{
    int32_t start, rate;
    uint8_t ev;

    *r->active = 1;
    start = (int32_t)CALL(0x00493038, 0, 0, 0);
    *r->cur_buffer = BUF_510E70;
    CALL(0x00463514, 0xf, 0, 0);
    while (CALL(0x004632f4, 0xf, 0, 0) == 0)
        ;
    CALL(0x00458470, 0, 0, 0);
    CALL(0x004a1494, 0, 0, 0);
    rate = (int32_t)CALL(0x004b5988, DDRAW, 0, 0);
    /* idiv: a refresh rate below 20 divides by 0 (trap in the original) */
    *r->target_fps = rate == 0 ? 0x14 : rate / (rate / 0x14);

    for (;;) {
        if (queue_step(r))
            break;
        r->frame_timer[1] = r->frame_timer[0];
        *r->cur_buffer = BUF_510E70;
        CALL(0x004966d8, BUF_510E70, 0x1000, 0);
        CALL(0x00493748, 0, 0, 0);
        CALL(0x0046be34, 0, 0, 0);
        CALL(0x0046b604, 0, 0, 0);
        CALL(0x00471b14, 0, 0, 0);
        CALL(0x00415b54, 0, 0, 0);
        CALL(0x00489ab0, 0, 0, 0);
        CALL(0x004534c8, 0, 0, 0);
        CALL(0x004948b4, 0, 0, 0);
        CALL(0x00492ed4, 0, 0, 0);
        CALL(0x0041321c, *r->cur_buffer, 0, 0);
        CALL(0x004133a8, 0, 0, 0);
        switch ((uint8_t)CALL(0x00474508, 0, 0, 0)) {
        case 1: CALL(0x004116b0, 0, 0, 0); CALL(0x0047259c, 0, 0, 0); break;
        case 2: CALL(0x00410df4, 0, 0, 0); CALL(0x00411b48, 0, 0, 0); break;
        case 3: CALL(0x00411fcc, 0, 0, 0); break;
        case 5: CALL(0x004727a8, 0, 0, 0); break;
        case 7: CALL(0x00472b74, 0, 0, 0); break;
        default: break;
        }
        CALL(0x00450454, 0, 0, 0);
        CALL(0x0045052c, *r->cur_buffer, 0, 0);
        CALL(0x00489d20, *r->cur_buffer, 0, 0);
        CALL(0x004a0dd4, 0, 0, 0);
        CALL(0x004a0e20, *r->cur_buffer, 0, 0);
        if (CALL(0x0046be2c, 0, 0, 0) == 0 || (uint8_t)CALL(0x004632b0, 0, 0, 0) != 0)
            CALL(0x00458470, 0, 0, 0);
        CALL(0x00499c74, 0, 0, 0);
        CALL(0x0046bbec, 0, 0, 0);
        CALL(0x00495b14, 0, 0, 0);
        if ((uint8_t)CALL(0x004958ac, 0, 0, 0) == 0 || *r->display_mode != 1)
            CALL(0x00495b60, 0, 0, 0);
        CALL(0x0049632c, 0, 0, 0);
        if ((uint8_t)CALL(0x004958ac, 0, 0, 0) == 0) {
            CALL(0x004ad5e0, 0, 0xb4, 0);
            CALL(0x004ad624, 0, 0xb4, 0);
            CALL(0x004ad5e0, 2, 0xb4, 0);
            CALL(0x0046cc68, 0, 0, 0);
            CALL(0x00434710, 0, *r->cur_buffer == BUF_510E70, 0);
            CALL(0x004ad5e0, 0, 0, 0);
            CALL(0x0049681c, (uint32_t)rd32(r->resolve(r->user, *r->cur_buffer)), 0, 0);
        } else {
            if (*r->display_mode == 1)
                CALL(0x004b4e98, DDRAW, 0, 0);
            CALL(0x004ad5e0, 0, 0, 0);
            CALL(0x0049681c, (uint32_t)rd32(r->resolve(r->user, *r->cur_buffer)), 0xb4, 0);
            CALL(0x00495b8c, 0, 0xb4, 0);
            CALL(0x004ad5e0, 2, 0xb4, 0);
            CALL(0x00434710, 0, *r->cur_buffer == BUF_510E70, 0);
            CALL(0x0046cc68, 0, 0, 0);
        }
        CALL(0x0049635c, 0, 0, 0);
        while ((int32_t)CALL(0x00493274, CALL(0x00493038, 0, 0, 0) - (uint32_t)start, 0, 0) > *r->target_fps)
            ;
        *r->ticks_total = (int32_t)((uint32_t)*r->ticks_total + (uint32_t)ticks60(r));
        CALL(0x004930ac, 0, 0, 0);
        start = (int32_t)CALL(0x00493038, 0, 0, 0);
    }
    CALL(0x004ad5e0, 2, 0, 0);
    CALL(0x00493748, 0, 0, 0);
    CALL(0x0049364c, 0, 0, 0);
    *r->active = 0;
    CALL(0x004a1488, 0, 0, 0);
    ev = *r->event_cur;
    return ev;
}
