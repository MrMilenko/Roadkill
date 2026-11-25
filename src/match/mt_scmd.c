/*
 * mt_scmd.c: scene commands (see mt_scmd.h).
 */
#include "mt_scmd.h"

#include <string.h>

static int32_t r32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static void w32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }
static int32_t wadd(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }

static uint8_t* target(uint32_t cmd, const MtSceneCmd* e) {
    return e->resolve(e->user, (uint32_t)r32(e->resolve(e->user, cmd) + 0xc));
}

void mt_scmd_place(uint32_t cmd, const MtSceneCmd* e) {
    const uint8_t* c = e->resolve(e->user, cmd);
    int32_t v0 = r32(c + 0x10), v1 = r32(c + 0x14), v2 = r32(c + 0x18);
    uint8_t* t = target(cmd, e);
    w32(t + 0x1c, v0);
    w32(t + 0x20, v1);
    w32(t + 0x24, v2);
}

void mt_scmd_orient(uint32_t cmd, const MtSceneCmd* e) {
    const uint8_t* c = e->resolve(e->user, cmd);
    int32_t v0 = r32(c + 0x10), v1 = r32(c + 0x14), v2 = r32(c + 0x18);
    uint8_t* t = target(cmd, e);
    w32(t + 0x28, v0);
    w32(t + 0x2c, v1);
    w32(t + 0x30, v2);
}

static void step_timer(uint32_t cmd, const MtSceneCmd* e) {
    uint8_t* c = e->resolve(e->user, cmd);
    int32_t left = r32(c + 4);
    if (left > 0) {
        w32(c + 4, left - 1);
        return;
    }
    if (left < 0) return;
    uint8_t* sc = e->scene;
    int32_t n = r32(sc + 0xacc);
    if (n < 0x96) {
        sc[0xad0 + 8 * n] = 2;
        n = r32(sc + 0xacc);
        w32(sc + 0xad4 + 8 * n, (int32_t)cmd);
        w32(sc + 0xacc, wadd(r32(sc + 0xacc), 1));
    }
    int32_t at = -1;
    if (r32(sc + 0xaa0) > 0) {
        int32_t k = 0;
        do {
            if ((uint32_t)r32(sc + 0xaa4 + 4 * k) == cmd) { at = k; break; }
            k++;
        } while (k < r32(sc + 0xaa0));
    }
    if (at != -1) {
        for (int32_t k = at; k < r32(sc + 0xaa0); k++) w32(sc + 0xaa4 + 4 * k, r32(sc + 0xaa8 + 4 * k));
        w32(sc + 0xaa0, r32(sc + 0xaa0) - 1);
    }
    c = e->resolve(e->user, cmd);
    w32(c + 4, r32(c));
}

static void add3(uint32_t cmd, const MtSceneCmd* e, int off, int wrap) {
    const uint8_t* c = e->resolve(e->user, cmd);
    uint8_t* t = target(cmd, e);
    int32_t v0 = wadd(r32(t + off), r32(c + 0x10));
    int32_t v1 = wadd(r32(t + off + 4), r32(c + 0x14));
    int32_t v2 = wadd(r32(t + off + 8), r32(c + 0x18));
    if (wrap) {
        v2 %= 0x1000;
        v1 %= 0x1000;
        v0 %= 0x1000;
    }
    t = target(cmd, e);
    w32(t + off, v0);
    w32(t + off + 4, v1);
    w32(t + off + 8, v2);
}

void mt_scmd_move(uint32_t cmd, const MtSceneCmd* e) {
    if (r32(e->resolve(e->user, cmd) + 4) != 0) add3(cmd, e, 0x1c, 0);
    step_timer(cmd, e);
}

void mt_scmd_turn(uint32_t cmd, const MtSceneCmd* e) {
    if (r32(e->resolve(e->user, cmd) + 4) != 0) add3(cmd, e, 0x28, 1);
    step_timer(cmd, e);
}

void mt_scmd_move_turn(uint32_t cmd, const MtSceneCmd* e) {
    if (r32(e->resolve(e->user, cmd) + 4) != 0) {
        const uint8_t* c = e->resolve(e->user, cmd);
        uint8_t* t = target(cmd, e);
        int32_t v[6];
        for (int k = 0; k < 3; k++) v[k] = wadd(r32(t + 0x1c + 4 * k), r32(c + 0x10 + 4 * k));
        for (int k = 0; k < 3; k++) v[3 + k] = wadd(r32(t + 0x28 + 4 * k), r32(c + 0x1c + 4 * k));
        t = target(cmd, e);
        for (int k = 0; k < 3; k++) w32(t + 0x1c + 4 * k, v[k]);
        t = target(cmd, e);
        for (int k = 0; k < 3; k++) w32(t + 0x28 + 4 * k, v[3 + k]);
    }
    step_timer(cmd, e);
}

void mt_scmd_choice(uint32_t cmd, const MtSceneCmd* e) {
    const uint8_t* c = e->resolve(e->user, cmd);
    uint8_t* node = e->resolve(e->user, (uint32_t)r32(target(cmd, e) + 4));
    if (node[0] != 9) return;
    uint8_t mode = c[0x10];
    int32_t value = r32(c + 0x14);
    uint32_t fn = (uint32_t)r32(c + 0x18);
    if (mode == 0) {
        do {
            int32_t cnt = node[6];
            node[7] = (uint8_t)(wadd(value, node[7] + cnt) % cnt);
            fn = (uint32_t)r32(c + 0x18);
            value = r32(c + 0x14);
        } while (fn && e->validate(e->user, fn, node[7]) == 0);
    } else if (mode == 1) {
        if (fn && e->validate(e->user, fn, (uint32_t)value) == 0) return;
        node[7] = (uint8_t)(value % (int32_t)node[6]);
    } else if (mode == 2) {
        do {
            int32_t cnt = node[6];
            int32_t cur = node[7];
            node[7] = (uint8_t)(wadd(cur, e->rand_fn(e->user)) % cnt);
            fn = (uint32_t)r32(c + 0x18);
        } while (fn && e->validate(e->user, fn, node[7]) == 0);
    }
}
