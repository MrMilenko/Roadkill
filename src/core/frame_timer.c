/*
 * frame_timer.c: the game's frame clock (see frame_timer.h).
 */
#include "frame_timer.h"

void frame_timer_update(TM2FrameTimer* t, uint32_t now) {
    uint32_t prev = t->last_us;
    t->now_us = now;
    t->last_us = now;
    uint32_t dt = now - prev;
    if (dt == 0) dt = 1000000u / t->max_fps;
    uint32_t raw = 1000000u / dt;
    t->prev_fps = t->fps;
    uint32_t lo = raw > 12 ? raw : 12;
    uint32_t fps = lo < t->max_fps ? lo : t->max_fps;
    t->fps = fps;
    t->ticks60 = 60u / fps;
    if (t->ticks60 == 0) t->ticks60 = 1;
    t->frames++;
    t->dt_us = dt;
    t->raw_fps = raw;
}

int32_t frame_tics_to_rate(const TM2FrameTimer* t, uint32_t tics) {
    int32_t r = (int32_t)t->max_fps;
    if (tics != 0) r = (int32_t)(1000000u / tics);
    if (r < 1) r = 1;
    return r;
}
