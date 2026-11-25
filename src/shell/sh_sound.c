/*
 * sh_sound.c: sound request wrappers (see sh_sound.h).
 */
#include "sh_sound.h"

static int on(const ShEnv* e) { return sh_r32(e, 0x00c850e0u) != 0; }

static void play(const ShEnv* e, int32_t a, int32_t b, int32_t c, int32_t d, int32_t s0, int32_t s1, int32_t s2) {
    int32_t v[8] = {a, b, c, d, s0, s1, s2, 13};
    if (e->snd_play) e->snd_play(e->user, v);
}

static void stop(const ShEnv* e, int32_t a, int32_t b, int32_t c) {
    if (e->snd_stop) e->snd_stop(e->user, a, b, c);
}

/* the per sound loop flags (0x0046fc58 / 0x0046fc68 / 0x0046fc78) by sound id */
static void loop_flag(const ShEnv* e, uint32_t id, uint8_t v) {
    if (id < 9) {
        if (id == 0) sh_w8(e, 0x00621184u, v);
    } else if (id == 9) {
        sh_w8(e, 0x00621186u, v);
    } else if (id == 0x20) {
        sh_w8(e, 0x00621182u, v);
    }
}

void sh_sound_play_car(const ShEnv* e, int32_t a, int32_t id, int32_t b, int32_t c, int32_t p5, int32_t p6) {
    if (!on(e)) return;
    if ((id >= 0x24 && id <= 0x3c && !(id & 1)) || id == 0x41)
        play(e, a, id, -1, 99, b, c, p5);
    else if (id == 0x19 && p5 != 99)
        play(e, a, id, p6, 0x4b, b, c, 99);
    else
        play(e, a, id, p6, p5, b, c, 0);
}

void sh_sound_stop(const ShEnv* e, int32_t a, int32_t id, int32_t b) {
    if (on(e)) stop(e, a, id, b);
}

void sh_sound_play_loop(const ShEnv* e, int32_t a, uint32_t id, int32_t b, int32_t c, int32_t p5, int32_t p6) {
    if (!on(e)) return;
    if (p6 == 0) {
        loop_flag(e, id, 1);
    } else {
        if (id != 9 || !on(e)) return;
        sh_w8(e, 0x00621188u, 1);
        id = 0x11;
    }
    play(e, a, (int32_t)id, p6, p5, b, c, 0);
}

void sh_sound_stop_loop(const ShEnv* e, int32_t a, uint32_t id, int32_t b) {
    if (!on(e)) return;
    if (b == 0) {
        loop_flag(e, id, 0);
    } else {
        if (id != 9 || !on(e)) return;
        sh_w8(e, 0x00621188u, 0);
        id = 0x11;
    }
    stop(e, a, (int32_t)id, b);
}

void sh_sound_play_generic(const ShEnv* e, int32_t a, int32_t id, int32_t b, int32_t c, int32_t p5, int32_t p6) {
    if (!on(e)) return;
    sh_w8(e, 0x00621188u, 1);
    if (id == 9) id = 0x11;
    play(e, a, id, p6, p5, b, c, 0);
}

void sh_sound_stop_generic(const ShEnv* e, int32_t a, int32_t id, int32_t b) {
    if (!on(e)) return;
    sh_w8(e, 0x00621188u, 0);
    if (id == 9) id = 0x11;
    stop(e, a, id, b);
}
