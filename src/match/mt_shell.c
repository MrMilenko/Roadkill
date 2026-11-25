/*
 * mt_shell.c: front end helpers (see mt_shell.h).
 */
#include "mt_shell.h"

#include <string.h>

void mt_inputcfg_init_state_table(const MtShell* s) {
    /* offsets from 0x0050e5cc, in the original's order */
    static const uint8_t zero[] = {0x38, 0xb8, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44,
                                   0x57, 0x58, 0x55, 0x56, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x4a,
                                   0x4e, 0x54, 0xb7, 0x2a, 0x45, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                   0x09, 0x0a, 0x0b, 0x01};
    memset(s->state_table, 1, 0x100);
    for (size_t k = 0; k < sizeof zero; k++) s->state_table[zero[k]] = 0;
}

void mt_inputcfg_load_data(const MtShell* s) {
    memset(s->cars, 0, 50);
    s->cars[0] = 1;
    uint32_t h = s->save_data(s->user, 0x00a82560u);
    if (!h) return;
    const uint8_t* p = s->resolve(s->user, h);
    for (int k = 0; k < 32; k++) s->cars[1 + k] = p[0x24c + k];
    for (int k = 0; k < 6; k++) {
        int32_t v;
        memcpy(&v, p + 0x21c + 4 * k, 4);
        if (v) {
            s->cars[0x22 + 2 * k] = 1;
            s->cars[0x21 + 2 * k] = 1;
        }
    }
    int32_t v;
    memcpy(&v, p + 0x218, 4);
    if (v) {
        s->cars[0x2e] = 1;
        s->cars[0x2f] = 1;
        s->cars[0x30] = 1;
        s->cars[0x2d] = 1;
    }
}

void mt_arena_next(const MtShell* s) {
    int32_t e = (int32_t)((uint32_t)*s->arena_cur + 1);
    for (;;) {
        if (e == 9) { e++; continue; }
        if (e >= 10 && e <= 12) {
            if (!s->unlocked(s->user, 0x23 + e - 10)) { e++; continue; }
            break;
        }
        if (e > 12) { e = 1; continue; }
        break;
    }
    *s->arena = (uint8_t)e;
}

void mt_arena_prev(const MtShell* s) {
    int32_t e = (int32_t)((uint32_t)*s->arena_cur - 1);
    for (;;) {
        if (e < 1) { e = 12; continue; }
        if (e >= 10 && e <= 12) {
            if (!s->unlocked(s->user, 0x23 + e - 10)) { e--; continue; }
            break;
        }
        if (e == 9) { e--; continue; }
        break;
    }
    *s->arena = (uint8_t)e;
}

static void btn_select(const MtShell* s, uint32_t obj, int32_t on) {
    const uint8_t* o = s->resolve(s->user, obj);
    uint32_t t, fn;
    memcpy(&t, o + 0x34, 4);
    memcpy(&fn, s->resolve(s->user, t) + 0xd0, 4);
    s->button_select(s->user, fn, obj, on);
}

void mt_game_type_7(const MtShell* s, uint32_t obj) {
    s->net_set_type(s->user, 0x0050f9a8u, 7);
    *s->game_type = 7;
    btn_select(s, obj, 1);
    btn_select(s, s->buttons[1], 0);
    btn_select(s, s->buttons[2], 0);
}

void mt_game_type_6(const MtShell* s, uint32_t obj) {
    s->net_set_type(s->user, 0x0050f9a8u, 6);
    *s->game_type = 6;
    btn_select(s, s->buttons[0], 0);
    btn_select(s, obj, 1);
    btn_select(s, s->buttons[2], 0);
}

void mt_game_type_5(const MtShell* s, uint32_t obj) {
    s->net_set_type(s->user, 0x0050f9a8u, 5);
    *s->game_type = 5;
    btn_select(s, s->buttons[0], 0);
    btn_select(s, s->buttons[1], 0);
    btn_select(s, obj, 1);
}
