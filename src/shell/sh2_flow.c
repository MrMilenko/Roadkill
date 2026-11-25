/*
 * sh2_flow.c: front end flow pieces (see sh2_flow.h).
 */
#include "sh2_flow.h"
#include "../core/tm2rand.h"

#define GS 0x00624ea8u          /* game state */
#define DISP 0x00626010u        /* display object pointer */

static uint32_t callv(const Sh2Env* e, uint32_t fn, int n, const uint32_t* a) {
    return e->sh.call ? e->sh.call(e->sh.user, fn, n, a) : 0;
}
static uint32_t call(const Sh2Env* e, uint32_t fn, int n, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t a[4] = {a0, a1, a2, a3};
    return callv(e, fn, n, a);
}
#define C0(fn) call(e, fn, 0, 0, 0, 0, 0)
#define C1(fn, a) call(e, fn, 1, a, 0, 0, 0)
#define C2(fn, a, b) call(e, fn, 2, a, b, 0, 0)
#define C3(fn, a, b, c) call(e, fn, 3, a, b, c, 0)
#define C4(fn, a, b, c, d) call(e, fn, 4, a, b, c, d)
#define R8(a) sh_r8(&e->sh, a)
#define R16(a) sh_r16(&e->sh, a)
#define R32(a) sh_r32(&e->sh, a)
#define W8(a, v) sh_w8(&e->sh, a, v)
#define W32(a, v) sh_w32(&e->sh, a, v)

/* the display object's first method: (obj, id, 0, 0, 0x63, n, 0), 3 stack words */
static void disp_draw(const Sh2Env* e, uint32_t id, uint32_t n) {
    uint32_t obj = R32(DISP);
    uint32_t a[7] = {obj, id, 0, 0, 0x63, n, 0};
    callv(e, R32(R32(obj + 0x1c)), 7, a);
}
/* the display object's second method (obj) */
static void disp_flip(const Sh2Env* e) {
    uint32_t obj = R32(DISP);
    C1(R32(R32(obj + 0x1c) + 4), obj);
}

void sh2_sound_init(const Sh2Env* e) {
    W32(0x00c85040u, 0x8ca);
    C0(0x004b90d0u);
}

void sh2_sound_stop_if_on(const Sh2Env* e, uint32_t h) {
    if (C0(0x004b9150u)) C2(0x004b9268u, h, h);
}

uint32_t sh2_sound_update(const Sh2Env* e, uint32_t iface) {
    return C1(0x004b9270u, iface);
}

void sh2_overlay_draw(const Sh2Env* e, uint32_t id) {
    uint32_t n = R32(0x009f6c18u);
    W32(0x009f6c18u, n + 1);
    disp_draw(e, id, n);
    sh2_sound_update(e, R32(DISP));
}

void sh2_shell_loop(const Sh2Env* e) {
    const uint32_t title = 0x004780fcu;
    do {
        uint32_t prev = title, cur = title;
        do {
            uint32_t next = C2(cur, prev, title);
            prev = cur;
            cur = next;
        } while (cur);
        C0(0x00496184u);
        uint32_t hwnd = C0(0x004a8f34u);
        /* MessageBoxA through the import table (stdcall, 4 stack words) */
        if (C4(R32(0x00c90524u), hwnd, 0x004e2b02u, 0x004e2af2u, 0x134) == 6) return;
    } while (1);
}

static uint32_t sprintf4(const Sh2Env* e, uint32_t buf, uint32_t fmt, uint32_t x, uint32_t y) {
    return C4(0x004b27e6u, buf, fmt, x, y);     /* cdecl: all four on the stack */
}

void sh2_level_files_load(const Sh2Env* e, uint32_t name) {
    uint32_t l0 = sh_local(&e->sh, 0), l1 = sh_local(&e->sh, 1);
    sprintf4(e, l0, 0x004e2209u, name, 0x004e2205u);
    sprintf4(e, l1, 0x004e2213u, name, 0x004e220fu);
    C1(0x0049ee90u, 0x00626018u);               /* DPC base */
    C2(0x00499564u, l1, 1);
    C2(0x00499564u, l0, 1);
    C2(0x00498d18u, 0x0074af98u, l1);
    /* DECOMPILER TRAP: Ghidra shows FUN_00497390() without arguments and the TPC registration's
     * result unused; the machine code passes it in eax to 0x00497390 and to 0x0044f130. */
    uint32_t tpc = C2(0x00496fd8u, 0x0074af98u, 0);
    C1(0x00497390u, tpc);
    C2(0x00498d18u, 0x00626018u, l0);
    C0(0x0045029cu);
    C1(0x0044f130u, tpc);
}

void sh2_challenge_result(const Sh2Env* e, uint32_t a0, uint8_t win, int16_t wins, int16_t losses) {
    /* the width getter gets the incoming eax (it ignores it) and leaves edx = 0x14, the row step */
    int32_t w = (int32_t)C0(0x004958d8u);
    int16_t cx = (int16_t)(w / 2);
    int32_t h = (int32_t)C0(0x004958e4u);
    int16_t cy = (int16_t)(h / 2);
    (void)a0;
    C0(0x0049632cu);
    uint32_t au = R32(0x00626014u);
    C2(R32(R32(au + 8) + 8), au, 2);            /* music stop */
    /* the music volume set to 0 (at least the minimum); the mixer value is computed from 0 */
    int32_t vol = 0, mn = (int32_t)R32(0x00c8507cu);
    W32(0x00c85098u, (uint32_t)(vol < mn ? mn : vol));
    int32_t v = (int32_t)(((uint32_t)vol << 16) - (uint32_t)vol) / 100;
    if (v > 0xffff) v = 0xffff;
    C2(0x004b8f48u, 0x00c850bcu, (uint32_t)v + ((uint32_t)v << 16));
    C1(0x00495b48u, 0);
    C4(0x00451f28u, 6, (uint32_t)(int32_t)cx, (uint32_t)(int32_t)cy, win ? 0x004e2531u : 0x004e2539u);
    disp_draw(e, 0x1c, 0xffffffffu);
    sh2_sound_update(e, R32(DISP));
    disp_flip(e);
    C4(0x00451f28u, 0, (uint32_t)(int32_t)cx, (uint32_t)(cy + 0x28), 0x004e2542u);
    sprintf4(e, 0x009f6920u, 0x004e2550u, (uint32_t)(int32_t)wins, (uint32_t)(int32_t)losses);
    C4(0x00451f28u, 0, (uint32_t)(int32_t)cx, (uint32_t)(cy + 0x3c), 0x009f6920u);
    C0(0x0049635cu);
    C0(0x00495b14u);
}

void sh2_message_pages(const Sh2Env* e, uint32_t a0, uint32_t font0, uint32_t text0, uint32_t x0, const uint32_t t[9]) {
    (void)a0;
    for (int k = 0; k < 3; k++) {
        if (k) {
            /* half a second, then the buttons are only checked once (the original's second
             * timeout test can never be true there): the loop ends either way. input_poll gets
             * the start reading, then the last reading. */
            uint32_t t0 = C0(0x00493038u), last = t0, d;
            do {
                C1(0x00493748u, last);
                last = C0(0x00493038u);
                d = last - t0;
            } while (d < 500000u);
            if (!(uint8_t)C2(0x004936acu, 0x12, 0) && !(uint8_t)C2(0x004936acu, 0x11, 0) &&
                !(uint8_t)C2(0x004936acu, 0x16, 0))
                C2(0x004936acu, 0x14, 0);
        }
        disp_draw(e, 0x1c, 0xffffffffu);
        sh2_sound_update(e, R32(DISP));
        disp_flip(e);
        C0(0x0049632cu);
        C1(0x00495b48u, 0);
        if (text0) C4(0x00451f28u, font0, x0, t[0], text0);
        if (k >= 1 && t[2]) C4(0x00451f28u, t[1], t[3], t[4], t[2]);
        if (k >= 2 && t[6]) C4(0x00451f28u, t[5], t[7], t[8], t[6]);
        C0(0x0049635cu);
        C0(0x00495b14u);
    }
}

static uint32_t car_model(uint8_t id) {
    if (id <= 11) return 0x20u + 0x10u * id;
    if (id == 12) return 0x10;
    if (id == 13) return 0xe0;
    if (id == 14) return 0xf0;
    return 0xffffffffu;
}

void sh2_session_cars(const Sh2Env* e, uint32_t gs) {
    C1(0x004576a8u, (uint32_t)((int32_t)R32(gs + 0x1c) >> 24));
    for (uint32_t i = 0; i < R8(gs + 1); i++) {
        uint32_t rec = C1(0x0045afbcu, i);
        W32(rec + 0x894, i);
        C2(0x0045803cu, i, car_model(R8(gs + 5 + i)));
    }
    for (uint32_t i = 0; i < R8(gs + 2); i++)
        C2(0x00458340u, i, car_model(R8(gs + 0xd + i)));
    if ((uint8_t)C0(0x004632b0u)) {
        C0(0x00464dccu);
        C0(0x00464ff8u);
        C0(0x00464ef4u);
        C1(0x00463514u, 0x11);
        while (!C1(0x004632f4u, 0x11)) {
        }
    }
    C0(0x0046c3f4u);
}

/*
 * 0x00477b90. The saved state lives in one 0x34 byte frame f, at the original's stack offsets:
 * f[0..4] = state +0..+4, f[4 + i] = state +4 + i (i = 1..humans), f[0xc + i] = +0x0c + i
 * (i = 1..opponents), f[0x16 + i] = +0x16 + i (i = 1..humans), f[0x1f..0x22] = +0x1f..+0x22,
 * f[0x24] u32 = +0x24, f[0x28], f[0x29] = +0x28, +0x29, f[0x29 + i] = +0x29 + i (i = 1..humans),
 * f[0x32], f[0x33] = +0x32, +0x33.
 * ORIGINAL BUG: the loops have no bound: more than 7 humans or 9 opponents spill into the next
 * saved area (kept: same frame), past 10 humans / 0x27 opponents they write over the saved
 * registers and the return address (not reproduced; the test stays below).
 */
uint32_t sh2_attract_demo(const Sh2Env* e, uint32_t next) {
    uint8_t f[0x34] = {0};
    uint32_t i, n1, n2;
    for (i = 0; i < 5; i++) f[i] = R8(GS + i);
    n1 = f[1];
    for (i = 1; i <= n1 && 4 + i < sizeof f; i++) f[4 + i] = R8(GS + 4 + i);
    n2 = f[2];
    for (i = 1; i <= n2 && 0xc + i < sizeof f; i++) f[0xc + i] = R8(GS + 0xc + i);
    for (i = 1; i <= n1 && 0x16 + i < sizeof f; i++) f[0x16 + i] = R8(GS + 0x16 + i);
    for (i = 0x1f; i <= 0x22; i++) f[i] = R8(GS + i);
    uint32_t v24 = R32(GS + 0x24);
    f[0x24] = (uint8_t)v24; f[0x25] = (uint8_t)(v24 >> 8); f[0x26] = (uint8_t)(v24 >> 16); f[0x27] = (uint8_t)(v24 >> 24);
    f[0x28] = R8(GS + 0x28);
    f[0x29] = R8(GS + 0x29);
    for (i = 1; i <= n1 && 0x29 + i < sizeof f; i++) f[0x29 + i] = R8(GS + 0x29 + i);
    f[0x32] = R8(GS + 0x32);
    f[0x33] = R8(GS + 0x33);

    W8(GS + 1, 1);
    W8(GS + 0, 0);
    W8(GS + 3, 1);
    W8(GS + 0x1f, R8(0x004e8020u));
    W8(GS + 5, (uint32_t)(tm2_rand() % 13));
    W8(GS + 2, 4);
    for (i = 1; i <= 0xd; i++) W8(GS + 0xc + i, 0xff);
    if (R8(GS + 0x21)) {
        W8(GS + 0xd, 0xe);
    } else {
        uint8_t keep = R8(0x009f6c1cu);
        W8(0x009f6c1cu, 1);             /* Sweet Tooth allowed for the pick */
        for (uint8_t k = 0; k < R8(GS + 2); k++) {
            uint32_t car;
            do car = (uint32_t)(tm2_rand() % 13);
            while (!C1(0x0047766cu, car));
            W8(GS + 0xd + k, car);
        }
        W8(0x009f6c1cu, keep);
    }
    C1(0x00495890u, 0);
    C1(0x00475d34u, GS);                /* sessionRun */

    for (i = 0; i < 5; i++) W8(GS + i, f[i]);
    n1 = R8(GS + 1);
    for (i = 1; i <= n1 && 4 + i < sizeof f; i++) W8(GS + 4 + i, f[4 + i]);
    n2 = R8(GS + 2);
    for (i = 1; i <= n2 && 0xc + i < sizeof f; i++) W8(GS + 0xc + i, f[0xc + i]);
    n1 = R8(GS + 1);
    for (i = 1; i <= n1 && 0x16 + i < sizeof f; i++) W8(GS + 0x16 + i, f[0x16 + i]);
    for (i = 0x1f; i <= 0x22; i++) W8(GS + i, f[i]);
    W32(GS + 0x24, (uint32_t)f[0x24] | (uint32_t)f[0x25] << 8 | (uint32_t)f[0x26] << 16 | (uint32_t)f[0x27] << 24);
    W8(GS + 0x28, f[0x28]);
    W8(GS + 0x29, f[0x29]);
    n1 = R8(GS + 1);
    for (i = 1; i <= n1 && 0x29 + i < sizeof f; i++) W8(GS + 0x29 + i, f[0x29 + i]);
    W8(GS + 0x32, f[0x32]);
    W8(GS + 0x33, f[0x33]);
    int8_t ev = (int8_t)R8(0x004e8020u);
    W8(0x004e8020u, ev < 8 ? (uint32_t)(ev + 1) : 1);
    return next;
}

/*
 * 0x0047b934. Returns what the original leaves in eax: item when item +0x0c is 0; item with its
 * low byte replaced by the game type when that is below 4 (signed); else the player count of the
 * last check.
 */
uint32_t sh2_net_player_names(const Sh2Env* e, uint32_t item, uint32_t list) {
    uint8_t own[0x100];
    uint8_t* buf = sh_local_host(&e->sh, sh_local(&e->sh, 0), own, sizeof own);
    Sh2Env* me = (Sh2Env*)e;
    if (R16(item + 0xc) == 0) return item;
    int8_t gt = (int8_t)R8(GS);
    if (gt < 4) return (item & 0xffffff00u) | (uint8_t)gt;
    uint32_t y = 0x28, n;
    for (uint32_t i = 0; ; i++) {
        n = C0(0x004632c8u);
        if ((int32_t)i >= (int32_t)n) break;
        uint32_t s = C1(0x004632e0u, i);
        /* ORIGINAL BUG: an unbounded copy into a 256 byte buffer (names are short) */
        size_t k = 0;
        for (;;) {
            uint8_t c = R8(s + (uint32_t)k);
            if (k < sizeof own) buf[k] = c;
            if (!c) break;
            k++;
        }
        if (k > 0xb) { buf[8] = buf[9] = buf[10] = '.'; buf[11] = 0; }
        me->local[0] = buf;
        uint32_t a[7] = {2, 0x1c2, y, 0x009f693cu, 0x009f6948u, sh_local(&e->sh, 0), R32(list)};
        callv(e, 0x00452204u, 7, a);
        me->local[0] = 0;
        uint32_t car = C2(0x004986dcu, R32(0x0050f9dcu), i);
        uint32_t l = R32(list);
        uint32_t name = C2(0x00474d68u, GS, (uint32_t)(int32_t)(int8_t)car);
        uint32_t b[7] = {2, 0x226, y, 0x009f693cu, 0x009f6948u, name, l};
        y += 0x14;
        callv(e, 0x00452204u, 7, b);
    }
    return n;
}
