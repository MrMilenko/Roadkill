/*
 * pf5_sfx.c: the sound effect voice list, TM2.EXE 0x004bb9c4 / 0x004bbdcc / 0x004bc31c /
 * 0x004bc4e4 / 0x004bc5cc (see pf5_sfx.h).
 */
#include "pf5_sfx.h"

#define DEV 0x00c850ecu
#define SLOT(l, i) ((l) + 4 + 0x28u * (uint32_t)(i))
#define INUSE(l, i) ((l) + 0x824 + 4u * (uint32_t)(i))
#define NEXT(l, i) ((l) + 0x8f4 + 4u * (uint32_t)(i))
#define PREV(l, i) ((l) + 0x9c4 + 4u * (uint32_t)(i))

static int32_t r32s(Pf2Env* e, uint32_t a) { return (int32_t)pf2_r32(e, a); }

static void unlink_slot(Pf2Env* e, uint32_t l, uint32_t i) {
    pf2_w32(e, INUSE(l, i), 0);
    uint32_t nx = pf2_r32(e, NEXT(l, i));
    pf2_w32(e, NEXT(l, pf2_r32(e, PREV(l, i))), nx);
    pf2_w32(e, PREV(l, nx), pf2_r32(e, PREV(l, i)));
    pf2_w32(e, l, pf2_r32(e, l) - 1);
}

/* 0x004ba67c on every voice of the slot's instance, instance = 0 */
static void release(Pf2Env* e, uint32_t slot) {
    uint32_t in = pf2_r32(e, slot + 4);
    if (in) {
        for (int32_t k = 0; k < r32s(e, in + 0x10); k++) pf2_call1(e, 0x4ba67c, pf2_r32(e, in + 0x14) + 0x44u * (uint32_t)k);
    }
    pf2_w32(e, slot + 4, 0);
}

uint32_t pf5_sfx_slot_alloc(Pf2Env* e, uint32_t l, uint32_t before) {
    uint32_t i;
    for (i = 2; i < 0x34; i++)
        if (pf2_r32(e, INUSE(l, i)) != 1) break;
    if (i == 0x34) return 0xffffffffu;
    pf2_w32(e, INUSE(l, i), 1);
    pf2_w32(e, NEXT(l, i), before);
    pf2_w32(e, PREV(l, i), pf2_r32(e, PREV(l, before)));
    pf2_w32(e, PREV(l, before), i);
    pf2_w32(e, NEXT(l, pf2_r32(e, PREV(l, i))), i);
    pf2_w32(e, l, pf2_r32(e, l) + 1);
    return i;
}

static int negative(Pf2Env* e, uint32_t s) {
    int32_t hi = r32s(e, s + 0x24);
    return hi < 0 || (hi == 0 && r32s(e, s + 0x20) < 0);
}

uint32_t pf5_sfx_slot_find(Pf2Env* e, uint32_t l, uint32_t program, uint32_t key) {
    for (uint32_t i = pf2_r32(e, NEXT(l, 0)); i != 1; i = pf2_r32(e, NEXT(l, i)))
        if (pf2_r32(e, SLOT(l, i)) == program && pf2_r32(e, SLOT(l, i) + 8) == key) return SLOT(l, i);
    uint32_t at;
    if (r32s(e, l) >= 0x32) {
        at = pf2_r32(e, PREV(l, 1));
        if (at == 0) return 0;
        if (negative(e, SLOT(l, at))) {
            release(e, SLOT(l, at));
            unlink_slot(e, l, at);
            at = pf2_r32(e, NEXT(l, at));
        }
    } else {
        for (at = pf2_r32(e, NEXT(l, 0)); at != 1; at = pf2_r32(e, NEXT(l, at))) {
            uint32_t s = SLOT(l, at);
            if (r32s(e, s + 0x24) <= 0 && r32s(e, s + 0x20) < 0) break;
        }
    }
    uint32_t i = pf5_sfx_slot_alloc(e, l, at);
    if ((int32_t)i < 0) return 0;
    uint32_t s = SLOT(l, i);
    pf2_w32(e, s + 4, 0);
    pf2_w32(e, s + 0xc, 0);
    pf2_w32(e, s + 8, key);
    pf2_w32(e, s, program);
#define TONE() (r32s(e, program) > 0 ? pf2_r32(e, program + 4) : 0u)
    pf2_w32(e, s + 0x1c, pf2_r32(e, TONE() + 0x10));
    pf2_w32(e, s + 0x20, pf2_r32(e, TONE() + 0xc));
    pf2_w32(e, s + 0x24, pf2_r32(e, TONE() + 8));
#undef TONE
    return s;
}

uint32_t pf5_sfx_start(Pf2Env* e, uint32_t bank, uint32_t prog, uint32_t key, uint32_t volume, uint32_t pan,
                       uint32_t pitch) {
    uint32_t s = pf5_sfx_slot_find(e, PF5_SFX_LIST, pf2_r32(e, bank + 4) + prog * 12u, key);
    if (!s) return 1;
    pf2_w32(e, s + 0x10, volume);
    pf2_w8(e, s + 0xc, pf2_r8(e, s + 0xc) | 0x1d);
    pf2_w32(e, s + 0x14, pan);
    pf2_w32(e, s + 0x18, pitch);
    return 0;
}

uint32_t pf5_sfx_update_list(Pf2Env* e, uint32_t l, uint32_t stale) {
    uint32_t playing = stale;          /* the stack word [esp] of the original */
    uint32_t i = pf2_r32(e, NEXT(l, 0));
    /* pass 1: stop requests and finished sounds */
    while (i != 1) {
        uint32_t s = SLOT(l, i);
        if (pf2_r8(e, s + 0xc) & 2) {
            uint32_t in = pf2_r32(e, s + 4);
            if (in)
                for (int32_t k = 0; k < r32s(e, in + 0x10); k++)
                    pf2_call2(e, 0x4b9898, DEV, pf2_r32(e, pf2_r32(e, in + 0x14) + 0x44u * (uint32_t)k + 0x40));
            release(e, s);
            unlink_slot(e, l, i);
        } else {
            uint32_t in = pf2_r32(e, s + 4);
            if (in) {
                for (int32_t k = 0; k < r32s(e, in + 0x10); k++) {
                    pf2_call3(e, 0x4b999c, DEV, pf2_r32(e, pf2_r32(e, in + 0x14) + 0x44u * (uint32_t)k + 0x40), e->scratch);
                    playing = (pf2_r8(e, e->scratch) & 1) ? 1 : 0;
                    if (playing == 1) break;
                }
                if (playing == 0) {
                    release(e, s);
                    unlink_slot(e, l, i);
                }
            }
        }
        i = pf2_r32(e, NEXT(l, i));
    }
    /* pass 2: apply in order within the voice budget */
    int32_t sum = 0;
    i = pf2_r32(e, NEXT(l, 0));
    while (i != 1 && sum < r32s(e, l + 0xa94)) {
        uint32_t s = SLOT(l, i);
        sum += r32s(e, s + 0x1c);
        if (sum > r32s(e, l + 0xa94)) {
            sum -= r32s(e, s + 0x1c);
            release(e, s);
            unlink_slot(e, l, i);
        } else if (pf2_call1(e, 0x4bbaf8, s) != 0) {
            sum -= r32s(e, s + 0x1c);
            release(e, s);
            unlink_slot(e, l, i);
        }
        i = pf2_r32(e, NEXT(l, i));
    }
    /* pass 3: everything after that point */
    while (i != 1) {
        release(e, SLOT(l, i));
        unlink_slot(e, l, i);
        i = pf2_r32(e, NEXT(l, i));
    }
    return 0;
}

uint32_t pf5_sfx_update(Pf2Env* e, uint32_t stale) { return pf5_sfx_update_list(e, PF5_SFX_LIST, stale); }

uint32_t pf5_sfx_stop_all(Pf2Env* e) {
    uint32_t l = PF5_SFX_LIST;
    for (uint32_t i; (i = pf2_r32(e, NEXT(l, 0))) != 1;) {
        uint32_t s = SLOT(l, i);
        if (pf2_r32(e, s + 4)) pf2_call1(e, 0x4baac0, pf2_r32(e, s + 4));
        pf2_w32(e, s + 4, 0);
        unlink_slot(e, l, i);
    }
    return 0;
}
