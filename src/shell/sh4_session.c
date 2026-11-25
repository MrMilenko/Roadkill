/*
 * sh4_session.c: the session runner, TM2.EXE 0x00475d34 (see sh4_session.h).
 *
 * DECOMPILER TRAPS: Ghidra merges the result branches by game type into one switch with wrong
 * cases (the jump table 0x00475d14 is: 0 free play, 1 / 2 tournament, 3 challenge, 4 and > 7
 * the same end of round code, 5..7 network) and loses the register values of several calls:
 * the first display mode call gets ebx = 7 (left from the input drain), the challenge result
 * gets "lost" = 0 for r 1 and 1 for r 3. The scores table halves the screen size with a plain
 * arithmetic shift (rounds down) where the messages use signed division (rounds to zero).
 * Machine detail kept: the wins count of the end of round code (+0x40) is stored after the
 * timer read that starts its wait.
 */
#include "sh4_session.h"

#define R8(a) sh_r8(e, a)
#define R16(a) sh_r16(e, a)
#define R32(a) sh_r32(e, a)
#define W8(a, v) sh_w8(e, a, v)
#define W16(a, v) sh_w16(e, a, v)
#define W32(a, v) sh_w32(e, a, v)
#define FB(o) (fb + (uint32_t)(o))
#define C0(f) sh3_call(e, f, 0, 0, 0, 0, 0, 0)
#define C1(f, a) sh3_call(e, f, 1, a, 0, 0, 0, 0)
#define C2(f, a, b) sh3_call(e, f, 2, a, b, 0, 0, 0)
#define C3(f, a, b, c) sh3_call(e, f, 3, a, b, c, 0, 0)
#define C4(f, a, b, c, d) sh3_call(e, f, 4, a, b, c, d, 0)

#define DONE FB(8)
#define RES FB(0x48)
#define BUF FB(0x3c)

static int32_t height(const ShEnv* e) { return (int32_t)C0(0x004958e4u); }
static int32_t width(const ShEnv* e) { return (int32_t)C0(0x004958d8u); }
static void set_flag(const ShEnv* e, uint32_t v, uint32_t i) { C2(0x004575b8u, v, i); }

/* waits at least lo us, then until one of the buttons 0x12, 0x11, 0x16, 0x14 or hi us */
static void wait_button(const ShEnv* e, uint32_t lo, uint32_t hi) {
    uint32_t t0 = C0(0x00493038u);
    for (;;) {
        C0(0x00493748u);
        uint32_t d = C0(0x00493038u) - t0;
        if (d < lo) continue;
        if ((uint8_t)C2(0x004936acu, 0x12, 0)) return;
        if ((uint8_t)C2(0x004936acu, 0x11, 0)) return;
        if ((uint8_t)C2(0x004936acu, 0x16, 0)) return;
        if ((uint8_t)C2(0x004936acu, 0x14, 0)) return;
        if (d >= hi) return;
    }
}

static void hud(const ShEnv* e, const uint32_t a[13]) {
    if (e->call) e->call(e->user, 0x0047506cu, 13, a);
}

/* three centered lines (line 2 below, line 0 above), the screen size asked for line 2 first */
static void message3(const ShEnv* e, uint32_t gs, uint32_t f0, uint32_t t0, uint32_t t1, uint32_t t2) {
    int32_t y2 = height(e) / 2 + 0x14, x2 = width(e) / 2;
    int32_t y1 = height(e) / 2, x1 = width(e) / 2;
    int32_t y0 = height(e) / 2 - 0x14, x0 = width(e) / 2;
    uint32_t a[13] = {gs, f0, t0, (uint32_t)x0, (uint32_t)y0, 2, t1, (uint32_t)x1, (uint32_t)y1, 2, t2,
                      (uint32_t)x2, (uint32_t)y2};
    hud(e, a);
}

static uint32_t level_name(uint8_t i) {
    static const uint32_t names[12] = {0x004e2308u, 0x004e2328u, 0x004e238cu, 0x004e2377u, 0x004e23a8u,
                                       0x004e233fu, 0x004e23c0u, 0x004e235au, 0x004e2422u, 0x004e23dbu,
                                       0x004e23f3u, 0x004e240au};
    return i <= 11 ? names[i] : 0x004e2422u;   /* "It's a Secret" */
}

/* the level and car models again (next stage) */
static void reload(const ShEnv* e, uint32_t gs) {
    C1(0x004753a4u, gs);
    C1(0x00475af8u, gs);
}

/* one or two digits of v % 100 (signed) into the text buffer */
static void digits(const ShEnv* e, uint32_t fb, int32_t v) {
    int32_t r = v % 100;
    if (r > 9) {
        W8(BUF, (uint8_t)(r / 10 + 0x30));
        W8(BUF + 1, (uint8_t)(v % 10 + 0x30));
        W8(BUF + 2, 0);
    } else {
        W8(BUF, (uint8_t)(v % 10 + 0x30));
        W8(BUF + 1, 0);
    }
}

/* tournament after a won round: the boss stages, the next level or the final message */
static void tournament_next(const ShEnv* e, uint32_t fb, uint32_t gs) {
    uint8_t lv = R8(gs + 0x1f);
    if (lv == 4) {      /* New York: the minion stage first */
        if ((int32_t)R32(gs + 2) >> 24 == 0xd || (R8(gs + 1) == 2 && (int32_t)R32(gs + 3) >> 24 == 0xd))
            W8(gs + 0x20, 1);
        if (R8(gs + 0x20) == 0) {
            W8(gs + 0x20, 1);
            message3(e, gs, 2, 0x004e25cfu, 0x004e259fu, 0x004e2570u);
            wait_button(e, 2000000, 10000000);
            W8(0x00624eb5u, 0xd);
            W8(0x00624eaau, 1);
            set_flag(e, 0, 0);
            set_flag(e, 0, 1);
            reload(e, gs);
            return;
        }
        if (R8(gs + 0x28) > 0) {
            set_flag(e, 0, 0);
            set_flag(e, 0, 1);
            message3(e, gs, 6, 0x004e260au, 0x004e25feu, level_name(R8(gs + 0x1f)));
            wait_button(e, 2000000, 10000000);
        } else {
            set_flag(e, 1, 0);
            set_flag(e, 1, 1);
        }
        W32(DONE, 1);
        return;
    }
    if (lv == 8) {      /* Dark Tooth */
        if (R8(gs + 0x21) == 0) {
            W8(gs + 0x21, 1);
            message3(e, gs, 2, 0x004e2670u, 0x004e2642u, 0x004e261bu);
            wait_button(e, 2000000, 10000000);
            W8(0x00624eb5u, 0xe);
            W8(0x00624eaau, 1);
            set_flag(e, 0, 0);
            set_flag(e, 0, 1);
            reload(e, gs);
            C0(0x0046fc0cu);
            return;
        }
        if (R8(gs + 0x22) == 0) {
            W8(gs + 0x22, 1);
            message3(e, gs, 2, 0x004e26f6u, 0x004e26c6u, 0x004e269du);
            wait_button(e, 2000000, 10000000);
            return;
        }
        message3(e, gs, 6, 0x004e277fu, 0x004e2753u, 0x004e2728u);
        wait_button(e, 2000000, 10000000);
        set_flag(e, 1, 0);
        W32(DONE, 1);
        set_flag(e, 1, 1);
        return;
    }
    {   /* the line 2 (level name) position is asked before the name is chosen */
        int32_t y2 = height(e) / 2 + 0x14, x2 = width(e) / 2;
        uint32_t name = level_name(R8(gs + 0x1f));
        int32_t y1 = height(e) / 2, x1 = width(e) / 2;
        int32_t y0 = height(e) / 2 - 0x14, x0 = width(e) / 2;
        uint32_t a[13] = {gs, 6, 0x004e279cu, (uint32_t)x0, (uint32_t)y0, 2, 0x004e2790u, (uint32_t)x1,
                          (uint32_t)y1, 2, name, (uint32_t)x2, (uint32_t)y2};
        hud(e, a);
    }
    wait_button(e, 2000000, 10000000);
    set_flag(e, 0, 0);
    set_flag(e, 0, 1);
    W32(DONE, 1);
}

/* quit: flags 1, done */
static void quit(const ShEnv* e, uint32_t fb) {
    set_flag(e, 1, 0);
    W32(DONE, 1);
    set_flag(e, 1, 1);
}

static void challenge_result(const ShEnv* e, uint32_t fb, uint32_t gs, uint32_t won) {
    C4(0x00474ee4u, gs, won, (uint32_t)(int32_t)(int16_t)R16(FB(0x40)), (uint32_t)(int32_t)(int16_t)R16(FB(0x44)));
    wait_button(e, 2000000, 2000000);
    set_flag(e, 1, 0);
}

/* network game, round result 8: the scores table, then the next round */
static void network_round_end(const ShEnv* e, uint32_t fb, uint32_t gs) {
    W32(0x00624dbcu, R32(0x00624dbcu) + C1(0x0045b9c4u, 0));
    W32(0x00624db8u, R32(0x00624db8u) + (C1(0x0045b9c4u, 0) == 0));
    W32(0x00624dc0u, R32(0x00624dc0u) + C1(0x0045b9ccu, 0));
    C1(0x00463514u, 0x12);
    while (!C1(0x004632f4u, 0x12)) {}
    C0(0x00464decu);
    C0(0x0049632cu);
    C1(0x00495b48u, 0);
    C1(0x004ad5e0u, 2);
    int32_t w = (int32_t)C0(0x004958d8u) >> 1;
    int32_t h = (int32_t)C0(0x004958e4u) >> 1;
    W32(FB(0x24), (uint32_t)h);
    W32(FB(0x28), (uint32_t)w);
    W32(FB(0x2c), (uint32_t)(w - 0x1e));
    C4(0x00451f28u, 2, (uint32_t)(w - 0x1e), (uint32_t)(h - 0x32), 0x004e27adu);   /* headings */
    W32(FB(0x14), (uint32_t)(w + 0x1e));
    C4(0x00451f28u, 2, (uint32_t)(w + 0x1e), (uint32_t)(h - 0x32), 0x004e27b2u);
    W32(FB(0x10), (uint32_t)(w + 0x5a));
    C4(0x00451f28u, 2, (uint32_t)(w + 0x5a), (uint32_t)(h - 0x32), 0x004e27b9u);
    W32(FB(0x20), R32(FB(0x10)));
    W32(FB(0x38), R32(FB(0x14)));
    W32(FB(0x34), R32(FB(0x2c)));
    W32(FB(0x30), (uint32_t)(w - 0x78));
    uint32_t y = (uint32_t)(h - 0x23);
    for (int32_t i = 0; i < (int32_t)C0(0x004632c8u); i++, y += 0xf) {
        uint32_t p = C1(0x00464cdcu, (uint32_t)i);
        W32(FB(0x1c), p);
        W32(FB(0), p);
        uint32_t q = C1(0x004632e0u, (uint32_t)i);
        W32(FB(0x2c), q);
        uint8_t keep = R8(q + 0xd);
        W8(FB(0x4c), keep);
        W8(q + 0xd, 0);                         /* the name cut to 13 characters */
        C4(0x00451f28u, 2, R32(FB(0x30)), y, q);
        W8(q + 0xd, R8(FB(0x4c)));
        uint32_t k = R32(FB(0x1c)) * 12;
        digits(e, fb, (int32_t)R32(0x00624db8u + k));
        C4(0x00451f28u, 2, R32(FB(0x34)), y, BUF);
        k = R32(FB(0)) * 12;
        digits(e, fb, (int32_t)R32(0x00624dbcu + k));
        C4(0x00451f28u, 2, R32(FB(0x38)), y, BUF);
        k = R32(FB(0)) * 12;
        digits(e, fb, (int32_t)R32(0x00624dc0u + k));
        C4(0x00451f28u, 2, R32(FB(0x20)), y, BUF);
    }
    C0(0x0049635cu);
    C0(0x00495b14u);
    wait_button(e, 4000000, 6000000);
    C0(0x00463f78u);
    C1(0x00458034u, C0(0x004632c8u));
    C0(0x00464d68u);
    W8(gs + 1, (uint8_t)C0(0x004632c8u));
    for (int32_t i = 0; i < (int32_t)C0(0x004632c8u); i++) {
        uint32_t car = C1(0x00464cf0u, (uint32_t)i);
        uint32_t p = C1(0x00464cdcu, (uint32_t)i);
        W8(p + gs + 5, (uint8_t)car);
    }
    if ((int32_t)C0(0x004632c8u) < 2) W32(DONE, 1);
    C0(0x00473160u);
    C1(0x00463514u, (uint32_t)((int32_t)R32(gs + 0x1c) >> 24));
    while (!C1(0x004632f4u, (uint32_t)((int32_t)R32(gs + 0x1c) >> 24))) {}
    uint8_t ty = R8(gs);
    if (ty == 5) {
        set_flag(e, 1, 0);
        int32_t lv = (int32_t)R32(gs + 0x1c) >> 24;
        W8(gs + 0x1f, (uint8_t)(lv % 12 + 1));
        W8(gs + 0x1f, (uint8_t)(R8(gs + 0x1f) + (((int32_t)R32(gs + 0x1c) >> 24) == 9)));
        reload(e, gs);
    } else if (ty == 6) {
        set_flag(e, 1, 0);
        reload(e, gs);
    }
}

int32_t sh4_session_run(const ShEnv* e, uint32_t fb, uint32_t gs) {
    W32(FB(4), gs);
    W32(DONE, 0);
    W8(RES, 0);
    W16(FB(0x44), 0);
    for (int i = 0; i < 3; i++) W8(BUF + i, R8(0x004e8010u + i));
    W16(FB(0x40), 0);
    while ((uint8_t)C2(0x004936acu, 7, 0)) C0(0x00493748u);
    C1(0x0045c328u, (uint32_t)(int32_t)(int8_t)R8(gs));
    if ((uint8_t)C0(0x004632b0u)) {
        C0(0x00464d68u);
        C0(0x00463268u);
        W8(gs + 1, (uint8_t)C0(0x004632c8u));
    }
    C1(0x00458034u, R8(gs + 1));
    C1(0x0045b370u, R8(gs + 0x28));
    C1(0x0049e0c0u, 1);
    C1(0x0049ea68u, 3);
    reload(e, gs);
    C3(0x004b3d80u, 0x00c84840u, 1, 7);
    C1(0x00495af8u, 1);
    C0(0x0046c17cu);
    C0(0x0046b354u);
    while (R32(DONE) == 0) {
        C1(0x00473f60u, (uint32_t)((int32_t)R32(gs + 0x1c) >> 24));   /* the level's music */
        C0(0x00451458u);
        C1(0x0047312cu, R32(0x00626010u));
        C1(0x00452d48u, 6);
        C1(0x00452d48u, 2);
        C1(0x00452d48u, 0);
        W8(RES, (uint8_t)C0(0x0046c6fcu));     /* rtMainLoop */
        uint32_t au = R32(0x00626014u);
        C2(R32(R32(au + 8) + 8), au, 2);        /* music stop */
        W32(FB(0xc), 0);
        int32_t s = (int32_t)R32(0x00c85080u);
        W32(0x00c85098u, s > 0 ? 0 : (uint32_t)s);
        int32_t mn = (int32_t)R32(0x00c8507cu);
        W32(0x00c85098u, (uint32_t)(0 < mn ? mn : 0));
        W32(FB(0xc), 0);                        /* 0 * 0xffff / 100, its clamp never taken */
        W32(FB(0xc), 0);
        C2(0x004b8f48u, 0x00c850bcu, 0);        /* cd_set_volume */
        C1(0x0047312cu, R32(0x00626010u));
        C0(0x0046cb44u);
        C0(0x00452d28u);
        C0(0x00452d28u);
        C0(0x00452d28u);
        uint8_t r = R8(RES);
        switch (R8(gs)) {
        case 0:
            set_flag(e, 1, 0);
            set_flag(e, 1, 1);
            if ((int8_t)r == 1) W32(DONE, 1);
            break;
        case 1: case 2:
            if (r == 1) {           /* a life lost */
                W8(gs + 0x17, (uint8_t)(R8(gs + 0x17) - 1));
                if (R8(gs + 0x17) == 0) {
                    int32_t y = height(e) / 2, x = width(e) / 2;
                    uint32_t a[13] = {gs, 6, 0x004e2566u, (uint32_t)x, (uint32_t)y, 0, 0, 0, 0, 0, 0, 0, 0};
                    hud(e, a);      /* GAME OVER */
                    wait_button(e, 2000000, 10000000);
                    W32(DONE, 1);
                    set_flag(e, 1, 0);
                    set_flag(e, 1, 1);
                }
                uint32_t car = C1(0x0045afbcu, 0);
                if ((int32_t)R32(car + 0x1d8) > 0) set_flag(e, 1, 1);
                else set_flag(e, 1, 0);
            } else if (r == 2) {
                quit(e, fb);
            } else {
                tournament_next(e, fb, gs);
            }
            break;
        case 3:
            C1(0x00475af8u, gs);
            if (r == 1) {
                W16(FB(0x44), (uint16_t)(R16(FB(0x44)) + 1));
                challenge_result(e, fb, gs, 0);
            } else if (r == 2) {
                quit(e, fb);
            } else if (r == 3) {
                W16(FB(0x40), (uint16_t)(R16(FB(0x40)) + 1));
                challenge_result(e, fb, gs, 1);
            }
            break;
        case 5: case 6: case 7:
            if (r == 2) quit(e, fb);
            else if (r == 8) network_round_end(e, fb, gs);
            break;
        default:    /* 4 and > 7 */
            if (r == 2) {
                quit(e, fb);
            } else if (r == 3) {
                uint16_t w = (uint16_t)(R16(FB(0x40)) + 1);
                uint32_t t0 = C0(0x00493038u);
                W16(FB(0x40), w);
                for (;;) {      /* wait_button with the timer already read */
                    C0(0x00493748u);
                    uint32_t d = C0(0x00493038u) - t0;
                    if (d < 2000000) continue;
                    if ((uint8_t)C2(0x004936acu, 0x12, 0)) break;
                    if ((uint8_t)C2(0x004936acu, 0x11, 0)) break;
                    if ((uint8_t)C2(0x004936acu, 0x16, 0)) break;
                    if ((uint8_t)C2(0x004936acu, 0x14, 0)) break;
                    if (d >= 2000000) break;
                }
                set_flag(e, 1, 0);
                set_flag(e, 1, 1);
            } else if (r == 4) {
                wait_button(e, 2000000, 2000000);
                set_flag(e, 1, 0);
                set_flag(e, 1, 1);
            }
            break;
        }
    }
    if ((uint8_t)C0(0x004632b0u)) {
        while ((int32_t)R32(FB(0x18)) < 8) {
            uint32_t c = R32(FB(0x18));
            C1(0x0045ba64u, c);
            W32(FB(0x18), c + 1);
        }
    }
    C0(0x0046c384u);
    C0(0x0046c5fcu);
    C3(0x004b3d80u, 0x00c84840u, 0, 0x10);
    C1(0x00495af8u, 0);
    C3(0x00496304u, 0x280, 0x1e0, 0x10);
    C1(0x0049e0c0u, 0);
    C1(0x0049ea68u, 1);
    C1(0x00452d48u, 6);
    C1(0x00452d48u, 2);
    C1(0x00452d48u, 0);
    W8(gs + 0x22, 0);
    W8(gs + 0x21, R8(gs + 0x22));
    W8(gs + 0x20, R8(gs + 0x22));
    C0(0x0046fc14u);
    C0(0x00458004u);
    return (int8_t)R8(RES);
}
