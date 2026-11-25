/*
 * mt2_world.c: cars_update_all / worldUpdate, TM2.EXE 0x00458470. See mt2_world.h.
 *
 * Written from the PC machine code. Machine details kept (see the header for the fields):
 * - "dword >> 16" / "dword >> 24" reads are the high i16 / i8 of the dword (hi16 / hi8 below);
 *   DECOMPILER TRAP: Ghidra prints them as reads of the dword's own address, so its field names
 *   are 2 / 3 bytes off (the health percentage divides by the i16 at +0x172, not +0x170).
 * - The display callback id chosen from the AI car type (0x0046f918) lives in one variable set
 *   to -1 once per call, not per car: a car whose type has no id repeats the previous car's id.
 * - ORIGINAL BUG: the multiplayer "nearest car" search starts from -1, not from a large value,
 *   so only negative distances ever count. A negative distance also counts as "close", which
 *   skips the block that reads the result: the test at 0x0045909c is dead code (never reached
 *   under emulation; kept in the port).
 * - ORIGINAL BUG: the multiplayer mood reset at 0x004590cd writes `dh ^ ch` (always 0) as the
 *   new mood.
 * - The speed gauge divides the speed's i16 by 32 rounding toward zero (the shl / sbb idiom).
 */
#include "mt2_world.h"

#include <string.h>

#define HS 0x898u /* human car stride */
#define AS 0x880u /* AI car stride */
#define H(i, a) ((uint32_t)(a) + (uint32_t)(i) * HS)
#define A(j, a) ((uint32_t)(a) + (uint32_t)(j) * AS)
#define V(x) ((uintptr_t)(uint32_t)(x))

static uint8_t* P(const Mt2World* w, uint32_t a) { return w->blk + (a - MT2W_BASE); }
static int32_t r32(const Mt2World* w, uint32_t a) { int32_t v; memcpy(&v, P(w, a), 4); return v; }
static int16_t r16(const Mt2World* w, uint32_t a) { int16_t v; memcpy(&v, P(w, a), 2); return v; }
static uint8_t r8(const Mt2World* w, uint32_t a) { return *P(w, a); }
static void w32(const Mt2World* w, uint32_t a, int32_t v) { memcpy(P(w, a), &v, 4); }
static void w16(const Mt2World* w, uint32_t a, int32_t v) { int16_t s = (int16_t)v; memcpy(P(w, a), &s, 2); }
static void w8(const Mt2World* w, uint32_t a, int32_t v) { *P(w, a) = (uint8_t)v; }
static int32_t hi16(const Mt2World* w, uint32_t a) { return r16(w, a + 2); }
static int32_t hi8(const Mt2World* w, uint32_t a) { return (int8_t)r8(w, a + 3); }
static int32_t iabs(int32_t v) { uint32_t s = (uint32_t)(v >> 31); return (int32_t)(((uint32_t)v ^ s) - s); }
static int32_t wsub(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
static int32_t wmul(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }

static uintptr_t call(const Mt2World* w, int id, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e)
{
    return w->fn[id](w->user, a, b, c, d, e);
}
#define C0(id) call(w, id, 0, 0, 0, 0, 0)
#define C1(id, a) call(w, id, (uintptr_t)(a), 0, 0, 0, 0)
#define C2(id, a, b) call(w, id, (uintptr_t)(a), (uintptr_t)(b), 0, 0, 0)
#define C3(id, a, b, c) call(w, id, (uintptr_t)(a), (uintptr_t)(b), (uintptr_t)(c), 0, 0)
#define C4(id, a, b, c, d) call(w, id, (uintptr_t)(a), (uintptr_t)(b), (uintptr_t)(c), (uintptr_t)(d), 0)

/* health percentage (32 bit imul, idiv): > 40 no smoke, 13..40 state 1, 1..12 state 2, <= 0 dead */
static int32_t health_pct(int32_t hp, int32_t maxhp) { return wmul(hp, 100) / maxhp; }

/* radar blip when |dz| < 400, else on one random draw in five (rand called only then) */
static void radar(const Mt2World* w, int32_t type, int32_t* off, int32_t dz, int32_t word, int32_t screen)
{
    if (iabs(dz) >= 400) {
        int32_t r = (int32_t)(uint32_t)C0(MT2W_RAND);
        if (r % 5 != 0)
            return;
    }
    C4(MT2W_RADAR_BLIP, V(type), off, V(word), V(screen));
}

static void human_loop(const Mt2World* w)
{
    int32_t* latch = w->input_latch;
    for (int32_t i = 0; i < r32(w, 0x5075e0); i++) {
        uint32_t car = H(i, 0x5030a8);
        uint32_t obj = (uint32_t)r32(w, 0x503088 + 4 * (uint32_t)i);
        if (obj == 0)
            break;
        C2(MT2W_READ_INPUT, P(w, car), V(obj));
        if (r16(w, car) == 0 && r8(w, car + 0xa5) == 0 && r8(w, car + 0x9a) == 0) {
            int8_t view = (int8_t)r8(w, car + 0x106);
            uint8_t key = r8(w, car + 0xd3);
            if (key == 2) {
                if (*latch != 0) {
                    *latch = 0;
                    int8_t v = (int8_t)(view + 1);
                    if (v > 4)
                        v = 2;
                    C2(MT2W_SET_VIEW, P(w, car), V((int32_t)v));
                }
            } else if (key == 1) {
                if (*latch != 0) {
                    *latch = 0;
                    int8_t v = (int8_t)(view - 1);
                    if (v < 2)
                        v = 4;
                    C2(MT2W_SET_VIEW, P(w, car), V((int32_t)v));
                }
            } else if (r8(w, car + 0xd4) != 0) {
                if (*latch != 0 && (int32_t)r8(w, car + 0xd4) != (int32_t)view) {
                    *latch = 0;
                    uint8_t m = r8(w, car + 0xd4);
                    int32_t v = (m == 3) ? 3 : (m == 4) ? 4 : 2;
                    C2(MT2W_SET_VIEW, P(w, car), V(v));
                }
                w8(w, car + 0xd4, 0);
            } else if (r8(w, car + 0xd6) != 0) {
                if (*latch != 0) { *latch = 0; C1(MT2W_436_F60, 0); }
            } else if (r8(w, car + 0xd7) != 0) {
                if (*latch != 0) { *latch = 0; C1(MT2W_436_F80, 0); }
            } else if (r8(w, car + 0xe2) != 0) {
                if (*latch != 0) { *latch = 0; C1(MT2W_436_F98, 0); }
            } else {
                *latch = 1;
            }
        }
        C2(MT2W_PLAYER_UPDATE, P(w, car), V(obj));
        uint8_t* o = w->resolve(w->user, obj);
        int16_t s;
        int32_t d;
        s = r16(w, car + 0x6f0); memcpy(o + 0xe0, &s, 2);
        s = r16(w, car + 0x6f4); memcpy(o + 0xe2, &s, 2);
        s = r16(w, car + 0x6f8); memcpy(o + 0xe4, &s, 2);
        d = r32(w, car + 0x6e4); memcpy(o + 0xe8, &d, 4);
        d = r32(w, car + 0x6e8); memcpy(o + 0xec, &d, 4);
        d = r32(w, car + 0x6ec); memcpy(o + 0xf0, &d, 4);
        C3(MT2W_COPY_WORDS, o + 0xb0, P(w, car + 0x744), 12);

        if (r32(w, 0x50e0f4) != 0) {
            int32_t n = r32(w, 0x50e0f8) + 1;
            w32(w, 0x50e0f8, n);
            if (n > 5)
                w32(w, 0x50e0f4, 0);
        } else if ((C0(MT2W_NET_IS_MULTI) & 0xff) != 0 && i == 0) {
            C2(MT2W_473_254, P(w, 0x5030a8), 1);
            C0(MT2W_463_F78);
            C1(MT2W_473_534, 1);
        } else if (r32(w, 0x502fa4) == 7 && i > 0 && r8(w, H(i, 0x503142)) != 0 && r32(w, H(i, 0x503280)) > 0) {
            int32_t id = r16(w, car);
            C3(MT2W_CAR_SETUP, P(w, car), V(hi16(w, car + 0x8e)), V(r8(w, 0x502f80 + (uint32_t)id)));
            C3(MT2W_CHOICE_STATE, V(hi16(w, car + 0x8e)), 0, V(hi8(w, car + 0x139)));
            w8(w, car + 0x13d, 0);
        }

        int32_t dz0 = wsub(r32(w, 0x503794), r32(w, 0x50402c));
        int32_t off[3];
        if (i == 0) {
            if (r32(w, H(i, 0x503280)) > 0) {
                off[0] = wsub(r32(w, H(i, 0x50378c)), r32(w, 0x504024));
                off[1] = wsub(r32(w, H(i, 0x503790)), r32(w, 0x504028));
                off[2] = wsub(r32(w, H(i, 0x503794)), r32(w, 0x50402c));
                radar(w, hi16(w, H(i, 0x503136)), off, dz0, r32(w, 0x504038), 1);
            }
        } else if (r32(w, H(i, 0x503280)) > 0) {
            off[0] = wsub(r32(w, H(i, 0x50378c)), r32(w, 0x50378c));
            off[1] = wsub(r32(w, H(i, 0x503790)), r32(w, 0x503790));
            off[2] = wsub(r32(w, H(i, 0x503794)), r32(w, 0x503794));
            radar(w, hi16(w, H(i, 0x503136)), off, dz0, r32(w, 0x5037a0), 0);
        }

        C3(MT2W_CAR_SOUND, V(r32(w, 0x507570 + 4 * (uint32_t)i)), P(w, car), 1);

        int32_t pct = health_pct(r32(w, H(i, 0x503280)), hi16(w, H(i, 0x503218)));
        int32_t type_a = H(i, 0x503136);
        if (pct > 40) {
            if (hi8(w, H(i, 0x5031e2)) != 0) {
                C3(MT2W_CHOICE_STATE, V(hi16(w, type_a)), 0, V(hi8(w, H(i, 0x5031e1))));
                C1(MT2W_BODY_SHOW, V(hi16(w, type_a)));
                if (hi8(w, H(i, 0x5031e2)) != 4)
                    C2(MT2W_SMOKE, P(w, car + 0x6e4), 0);
                w8(w, H(i, 0x5031e5), 0);
            }
        } else if (pct > 12) {
            if (hi8(w, H(i, 0x5031e2)) != 1) {
                C3(MT2W_CHOICE_STATE, V(hi16(w, type_a)), 1, V(hi8(w, H(i, 0x5031e1))));
                if (hi8(w, H(i, 0x5031e2)) != 4)
                    C2(MT2W_SMOKE, P(w, car + 0x6e4), 0);
                w8(w, H(i, 0x5031e5), 1);
            }
        } else if (pct > 0) {
            if (hi8(w, H(i, 0x5031e2)) != 2) {
                C3(MT2W_CHOICE_STATE, V(hi16(w, type_a)), 2, V(hi8(w, H(i, 0x5031e1))));
                if (hi8(w, H(i, 0x5031e2)) != 4)
                    C2(MT2W_SMOKE, P(w, car + 0x6e4), 0);
                w8(w, H(i, 0x5031e5), 2);
            }
        } else if (r32(w, H(i, 0x503280)) <= 0 && r8(w, H(i, 0x503142)) == 0) {
            C1(MT2W_BODY_HIDE, V(hi16(w, type_a)));
            w8(w, H(i, 0x503142), 1);
            w8(w, 0x502f80 + (uint32_t)i, 1);
            C2(MT2W_DESTROYED, P(w, car), 1);
            int32_t mode = r32(w, 0x502fa4);
            if (mode < 5) {
                w32(w, 0x502f78, 1);
                int32_t ev = (mode == 4) ? (i == 0 ? 4 : 3) : 1;
                int32_t delay = r8(w, H(i, 0x503166)) != 0 ? 0xd2 : 0x1a4;
                C2(MT2W_SCHEDULE, V(ev), V(delay));
            } else if (mode == 7) {
                if ((uint32_t)C0(MT2W_463_2D4) != 0) {
                    C0(MT2W_432_7FC);
                    C1(MT2W_432_838, 0);
                }
            } else {
                w32(w, 0x50e0b0, 0);
                int32_t n = r32(w, 0x5075e0);
                if (n > 0) {
                    uint32_t end = (uint32_t)n * HS;
                    for (uint32_t k = 0; (int32_t)k < (int32_t)end; k += HS)
                        if (r32(w, 0x503280 + k) > 0)
                            w32(w, 0x50e0b0, r32(w, 0x50e0b0) + 1);
                }
                if (r32(w, 0x50e0b0) < 2) {
                    w32(w, 0x502f78, 1);
                    C2(MT2W_SCHEDULE, 8, 0x1a4);
                }
            }
        }

        /* speed gauge */
        int32_t si = (int16_t)i;
        int32_t sp = hi16(w, H(si, 0x503774));
        int32_t q = (sp < 0 ? sp + 31 : sp) >> 5;
        int32_t num = q * 100;
        int32_t den = (int16_t)C0(MT2W_TICKS) * 0x13;
        int32_t target = num / den;
        uint32_t ga = 0x5075c0 + 4 * (uint32_t)si;
        int32_t diff = iabs(wsub(target, r32(w, ga)));
        int32_t rate = diff > 15 ? 0xd : diff > 10 ? 9 : diff > 5 ? 5 : 3;
        w32(w, ga, (int32_t)(uint32_t)C3(MT2W_APPROACH, V(r32(w, ga)), V(target), V(rate)));

        if (i == r32(w, 0x502f8c) || (uint32_t)C0(MT2W_46C_C54) != 0)
            C2(MT2W_CHASE_CAMERA, P(w, car), V(i));

        int32_t vp[3];
        if (r8(w, 0x502f9c) != 0) {
            if (r32(w, 0x502fd8) != 0) {
                vp[0] = r32(w, 0x5030bc);
                vp[1] = r32(w, 0x5030c0);
            } else {
                uint32_t k = (uint32_t)r32(w, 0x502f90) * AS;
                vp[0] = r32(w, k + 0x507d34);
                vp[1] = r32(w, k + 0x507d38);
            }
        } else {
            vp[0] = r32(w, H(i, 0x50378c));
            vp[1] = r32(w, H(i, 0x503790));
        }
        vp[2] = 0;
        C2(MT2W_VIEWER_POS, vp, V(i));
    }
}

static void multi_moods(const Mt2World* w)
{
    int32_t best_frames_i = -1, best_frames = 0;
    int32_t near_i = -1, near_d = -1;
    int32_t mood1 = 0;
    int32_t top_i = -1, top_v = 0;
    int32_t low_i = -1, low_v = 100;
    int32_t timer_i = -1, timer_d = 32000;
    int32_t close = 0;
    for (int32_t j = 0; j < r32(w, 0x50c2b0); j++) {
        if (hi8(w, A(j, 0x507afa)) == 4)
            continue;
        int32_t v = hi16(w, A(j, 0x507616));
        if (v > best_frames) { best_frames_i = j; best_frames = v; }
        v = hi16(w, A(j, 0x507b7a));
        if (v > top_v) { top_i = j; top_v = v; }
        int32_t m = hi8(w, A(j, 0x50760a));
        if ((m == 1 || m == 3) && hi16(w, A(j, 0x50761a)) > 0xf0 && hi16(w, A(j, 0x507b7a)) < low_v) {
            low_i = j;
            low_v = hi16(w, A(j, 0x507b7a));
        }
        if (r32(w, A(j, 0x507634)) < 0x12c0)
            close++;
        if (r32(w, A(j, 0x507634)) < near_d) { near_i = j; near_d = r32(w, A(j, 0x507634)); }
        m = hi8(w, A(j, 0x50760a));
        if (m == 2) {
            int32_t t = (int32_t)(uint32_t)C0(MT2W_TICKS);
            w16(w, A(j, 0x50761a), (int32_t)((uint32_t)(uint16_t)r16(w, A(j, 0x50761a)) + (uint32_t)t));
        }
        if (m == 2 || m == 3) {
            int32_t half = hi16(w, A(j, 0x50761c));
            half = (half - (half >> 31)) >> 1;
            if (r32(w, A(j, 0x507634)) < timer_d && hi16(w, A(j, 0x50761a)) > half) {
                timer_i = j;
                timer_d = r32(w, A(j, 0x507634));
            }
        } else if (m == 1) {
            mood1 += 1;
        }
        w16(w, A(j, 0x507616), 0);
        for (int32_t k = 0; k < r32(w, 0x50c2b0); k++) {
            if (j == k || r8(w, A(k, 0x507674)) != 0)
                continue;
            if (r32(w, A(k, 0x507634)) < r32(w, A(j, 0x507634)))
                w16(w, A(j, 0x507616), r16(w, A(j, 0x507616)) + 1);
        }
        if (hi16(w, A(j, 0x507614)) < 3)
            w16(w, A(j, 0x507618), 0);
        else
            w16(w, A(j, 0x507618), r16(w, A(j, 0x507618)) + 1);
    }
    if (best_frames_i >= 0)
        w16(w, A(best_frames_i, 0x507618), 0);
    if (mood1 > 3 || close > 3) {
        if (low_i >= 0) {
            uint32_t k = (uint32_t)low_i;
            w8(w, A(k, 0x50760d), 2);
            int16_t ax = r16(w, A(k, 0x507b80));
            w16(w, A(k, 0x50761a), 0);
            w16(w, A(k, 0x50761e), ax);
            if ((uint32_t)C0(MT2W_RAND) & 1) {
                w16(w, A(k, 0x50761e), r16(w, A(k, 0x507b82)));
                w8(w, A(k, 0x50760d), 3);
            }
            w16(w, A(k, 0x50761c), 0);
        }
    } else if (close < 1) {
        if (near_i >= 0 && hi8(w, A(near_i, 0x50760a)) == 1)
            timer_i = -1;
        if (timer_i >= 0) {
            uint32_t k = (uint32_t)timer_i;
            w8(w, A(k, 0x50760d), 0);
            w16(w, A(k, 0x50761c), 0);
            int32_t v = wmul(hi16(w, A(k, 0x507b84)), r32(w, A(k, 0x507b98)));
            w16(w, A(k, 0x50761e), 600);
            w16(w, A(k, 0x507b84), v / 100);
        }
    }
    if (mood1 < 1 && top_i >= 0) {
        uint32_t k = (uint32_t)top_i;
        w8(w, A(k, 0x50760d), 1);
        w16(w, A(k, 0x50761e), 0xe10);
        w16(w, A(k, 0x507b84), 100);
        w16(w, A(k, 0x50761c), 0);
    }
}

static void single_timers(const Mt2World* w)
{
    w32(w, 0x502fac, (int32_t)((uint32_t)r32(w, 0x502fac) + (uint32_t)(int32_t)(int16_t)C0(MT2W_TICKS)));
    w32(w, 0x502fb0, (int32_t)((uint32_t)r32(w, 0x502fb0) + (uint32_t)(int32_t)(int16_t)C0(MT2W_TICKS)));
    if (r32(w, 0x502fb0) > 0x258) {
        w32(w, 0x502fac, 0);
        w32(w, 0x502fb0, 0);
        C0(MT2W_45C_56C);
        C0(MT2W_45C_350);
    } else if (r32(w, 0x502fac) > 0x12c) {
        C0(MT2W_45C_350);
    }
    uint32_t k = (uint32_t)r32(w, 0x502f90) * AS;
    if (hi16(w, k + 0x507e64) > 0xc) {
        w16(w, k + 0x507e66, 0x3c);
        if (hi16(w, k + 0x50763a) == 0xd0 && hi8(w, k + 0x507e4a) == 0)
            w8(w, k + 0x507e4d, 2);
    }
    if ((C1(MT2W_TIMEOUT, 0) & 0xff) != 0)
        C2(MT2W_SCHEDULE, 1, 0);
}

static int32_t display_id(int16_t t, int32_t cur)
{
    switch (t) {
    case 0x10: return 0x3c;
    case 0x20: return 0x24;
    case 0x30: return 0x26;
    case 0x40: return 0x28;
    case 0x50: return 0x2a;
    case 0x60: return 0x2c;
    case 0x70: return 0x2e;
    case 0x80: return 0x30;
    case 0x90: return 0x32;
    case 0xa0: return 0x34;
    case 0xb0: return 0x36;
    case 0xc0: return 0x38;
    case 0xd0: return 0x3a;
    case 0xe0: return 0x41;
    default: return cur;
    }
}

static void ai_loop(const Mt2World* w, int32_t* cb_id)
{
    for (int32_t j = 0; j < r32(w, 0x50c2b0); j++) {
        uint32_t car = A(j, 0x50760c);
        int active = !(r8(w, 0x502f9c) == 0 && hi8(w, A(j, 0x507afa)) == 4);
        if (active) {
            w32(w, A(j, 0x507b94), 1);
            uint32_t obj = (uint32_t)r32(w, 0x5075e8 + 4 * (uint32_t)j);
            C1(MT2W_AI_UPDATE, P(w, car));
            uint8_t* o = w->resolve(w->user, obj);
            int16_t s;
            int32_t d;
            s = r16(w, car + 0x734); memcpy(o + 0xe0, &s, 2);
            s = r16(w, car + 0x738); memcpy(o + 0xe2, &s, 2);
            s = r16(w, car + 0x73c); memcpy(o + 0xe4, &s, 2);
            d = r32(w, car + 0x728); memcpy(o + 0xe8, &d, 4);
            d = r32(w, car + 0x72c); memcpy(o + 0xec, &d, 4);
            d = r32(w, car + 0x730); memcpy(o + 0xf0, &d, 4);
            C3(MT2W_COPY_WORDS, o + 0xb0, P(w, car + 0x788), 12);
            w16(w, A(j, 0x507618), 0);
        } else {
            w32(w, A(j, 0x507b94), 0);
            w16(w, A(j, 0x507618), r16(w, A(j, 0x507618)) + 1);
        }
        if (r8(w, A(j, 0x50763e)) == 0 && r32(w, 0x502fa4) != 0) {
            *cb_id = display_id(r16(w, A(j, 0x50763c)), *cb_id);
            if (*cb_id != -1)
                C1(MT2W_DISPLAY_CB, V(*cb_id));
        } else {
            C3(MT2W_CAR_SOUND, V(r32(w, 0x50c28c + 4 * (uint32_t)j)), P(w, car), 0);
        }
        int32_t type = hi16(w, A(j, 0x50763a));
        if (r32(w, A(j, 0x507b98)) > 0) {
            int32_t off[3];
            off[0] = wsub(r32(w, A(j, 0x507d34)), r32(w, 0x50378c));
            off[1] = wsub(r32(w, A(j, 0x507d38)), r32(w, 0x503790));
            off[2] = wsub(r32(w, A(j, 0x507d3c)), r32(w, 0x503794));
            radar(w, type, off, wsub(r32(w, 0x503794), r32(w, A(j, 0x507d3c))), r32(w, 0x5037a0), 0);
            if (r32(w, 0x502fa4) == 2) {
                off[0] = wsub(r32(w, A(j, 0x507d34)), r32(w, 0x504024));
                off[1] = wsub(r32(w, A(j, 0x507d38)), r32(w, 0x504028));
                off[2] = wsub(r32(w, A(j, 0x507d3c)), r32(w, 0x50402c));
                radar(w, hi16(w, A(j, 0x50763a)), off, wsub(r32(w, 0x50402c), r32(w, A(j, 0x507d3c))),
                      r32(w, 0x504038), 1);
            }
        }
        int32_t pct = health_pct(r32(w, A(j, 0x507b98)), hi16(w, A(j, 0x507b30)));
        if (pct > 40) {
            if (hi8(w, A(j, 0x507afa)) != 0) {
                C3(MT2W_CHOICE_STATE, V(hi16(w, A(j, 0x50763a))), 0, V(hi8(w, A(j, 0x507af9))));
                C1(MT2W_BODY_SHOW, V(hi16(w, A(j, 0x50763a))));
                w8(w, A(j, 0x507afd), 0);
            }
        } else if (pct > 12) {
            if (hi8(w, A(j, 0x507afa)) != 1) {
                C3(MT2W_CHOICE_STATE, V(hi16(w, A(j, 0x50763a))), 1, V(hi8(w, A(j, 0x507af9))));
                w8(w, A(j, 0x507afd), 1);
            }
        } else if (pct > 0) {
            if (hi8(w, A(j, 0x507afa)) != 2) {
                C3(MT2W_CHOICE_STATE, V(hi16(w, A(j, 0x50763a))), 2, V(hi8(w, A(j, 0x507af9))));
                w8(w, A(j, 0x507afd), 2);
            }
        } else if (r32(w, A(j, 0x507b98)) <= 0) {
            if (r8(w, A(j, 0x507674)) == 0) {
                C1(MT2W_BODY_HIDE, V(hi16(w, A(j, 0x50763a))));
                w8(w, A(j, 0x507674), 1);
                C2(MT2W_DESTROYED, P(w, car), 0);
            }
            if ((C0(MT2W_EVENT_TYPE) & 0xff) == 3 && r32(w, 0x50c2b4) < 3)
                C0(MT2W_432_5C8);
        }
    }
}

static void boss_and_end(const Mt2World* w)
{
    w32(w, 0x50c2b4, 0);
    int32_t n = r32(w, 0x50c2b0);
    if (n > 0) {
        uint32_t end = (uint32_t)n * AS;
        for (uint32_t k = 0; (int32_t)k < (int32_t)end; k += AS)
            if (r32(w, 0x507b98 + k) > 0)
                w32(w, 0x50c2b4, r32(w, 0x50c2b4) + 1);
    }
    if (r32(w, 0x50c2b4) < 1 && (C0(MT2W_EVENT_TYPE) & 0xff) == 8 && r16(w, 0x50763c) == 0xf0 &&
        r32(w, 0x50c2c0) == 0) {
        w32(w, 0x50c2b4, 1);
        int32_t h = hi16(w, 0x507b1c);
        if (h <= 0) {
            w32(w, 0x50c2c0, 1);
            C1(MT2W_43A_6D8, P(w, 0x507af8));
            C2(MT2W_438_320, P(w, 0x50760c), 0);
            w8(w, 0x507674, 0);
            w32(w, 0x502f78, 0);
            C2(MT2W_SCHEDULE, 3, 0);
            if (r32(w, 0x507d38) < -0x3570) {
                w32(w, 0x507d3c, 0x1e0);
                w32(w, 0x507d38, -0x3200);
            }
            int32_t x = r32(w, 0x507d34);
            if (x > 0x2580) {
                w32(w, 0x507d3c, 0x1e0);
                w32(w, 0x507d34, 0x2580);
            } else if (x < -0x1c20) {
                w32(w, 0x507d3c, 0x1e0);
                w32(w, 0x507d34, -0x2580);
            } else if (x < -0x12c0) {
                w32(w, 0x507d3c, 0x1e0);
                w32(w, 0x507d34, -0x12c0);
            }
        } else if (h > 0x78) {
            C4(MT2W_421_920, P(w, 0x50760c), 0, 0xc, 0);
        }
    }
    if (r32(w, 0x50c2b4) <= 0 && r32(w, 0x502fa4) < 4 && r8(w, 0x503142) == 0 && r32(w, 0x502f94) == 0) {
        w32(w, 0x502f94, 1);
        C2(MT2W_SOUND_9, 9, V(-1));
        int32_t delay = 0x1a4;
        if (r32(w, 0x50c2c0) != 0) {
            uint32_t t = (uint32_t)C0(MT2W_TARGET_OBJECT);
            int32_t z = 0;
            memcpy(w->resolve(w->user, t) + 8, &z, 4);
            delay = 0xd2;
        }
        C2(MT2W_SCHEDULE, 3, V(delay));
        w32(w, 0x502f78, 1);
    }
}

static void collide(const Mt2World* w)
{
    C0(MT2W_RANDOM_EVENT);
    for (int32_t i = 0; i < r32(w, 0x5075e0); i++)
        for (uint32_t k = 0x24; k <= 0xd8; k += 0x24)
            w8(w, H(i, 0x503514) + k, 0);
    for (int32_t j = 0; j < r32(w, 0x50c2b0); j++)
        for (uint32_t k = 0x24; k <= 0xd8; k += 0x24)
            w8(w, A(j, 0x507928) + k, 0);
    for (int32_t i = 0; i < r32(w, 0x5075e0); i++) {
        uint32_t car = H(i, 0x5030a8);
        if (r32(w, H(i, 0x50327c)) == 0)
            continue;
        call(w, MT2W_COLLIDE, V(r32(w, 0x503088 + 4 * (uint32_t)i)), (uintptr_t)P(w, car + 0x234), 1,
             (uintptr_t)P(w, car + 0x7bc), (uintptr_t)P(w, car + 0x7a4));
        int flag = i != 0 && r32(w, 0x502fa4) >= 5;
        C3(MT2W_HITS_RESOLVE, P(w, car), 1, V(flag));
    }
    for (int32_t j = 0; j < r32(w, 0x50c2b0); j++) {
        uint32_t car = A(j, 0x50760c);
        if (r32(w, A(j, 0x507b94)) == 0)
            continue;
        call(w, MT2W_COLLIDE, V(r32(w, 0x5075e8 + 4 * (uint32_t)j)), (uintptr_t)P(w, car + 0xe4), 1,
             (uintptr_t)P(w, car + 0x800), (uintptr_t)P(w, car + 0x7e8));
        C3(MT2W_HITS_RESOLVE, P(w, car), 0, 0);
    }
    for (int32_t i = 0; i < r32(w, 0x5075e0); i++)
        C2(MT2W_HITS_GATHER, P(w, H(i, 0x5030a8)), 1);
    for (int32_t j = 0; j < r32(w, 0x50c2b0); j++) {
        C2(MT2W_HITS_GATHER, P(w, A(j, 0x50760c)), 0);
        if (hi16(w, A(j, 0x5076dc)) > 0 && iabs(wsub(r32(w, A(j, 0x507d48)), r32(w, A(j, 0x507d54)))) > 0x2aa)
            w8(w, A(j, 0x507649), 1);
    }
    for (int32_t i = 0; i < r32(w, 0x5075e0); i++) {
        uint32_t car = H(i, 0x5030a8);
        if (r8(w, H(i, 0x5032c6)) != 0) {
            C2(MT2W_HIT_PUSH, P(w, car), 1);
        } else if (hi16(w, H(i, 0x5032d2)) > 2) {
            w16(w, H(i, 0x503784), r16(w, H(i, 0x503774)));
            w16(w, H(i, 0x503786), r16(w, H(i, 0x503776)));
            w16(w, H(i, 0x503788), r16(w, H(i, 0x503778)));
            w32(w, H(i, 0x50384c), r32(w, H(i, 0x50378c)));
            w32(w, H(i, 0x503850), r32(w, H(i, 0x503790)));
            w32(w, H(i, 0x503854), r32(w, H(i, 0x503794)));
            w32(w, H(i, 0x503858), r32(w, H(i, 0x5037a4)));
            w32(w, H(i, 0x50385c), r32(w, H(i, 0x5037a8)));
            w32(w, H(i, 0x503860), r32(w, H(i, 0x5037ac)));
            C3(MT2W_COPY_WORDS, P(w, car + 0x7bc), P(w, car + 0x714), 12);
        }
        C2(MT2W_HIT_RECOVER, P(w, car), 1);
    }
    for (int32_t j = 0; j < r32(w, 0x50c2b0); j++) {
        uint32_t car = A(j, 0x50760c);
        if (r8(w, A(j, 0x5076da)) != 0) {
            C2(MT2W_HIT_PUSH, P(w, car), 0);
        } else if (hi16(w, A(j, 0x5076e6)) > 2) {
            w16(w, A(j, 0x507d2c), r16(w, A(j, 0x507d1c)));
            w16(w, A(j, 0x507d2e), r16(w, A(j, 0x507d1e)));
            w16(w, A(j, 0x507d30), r16(w, A(j, 0x507d20)));
            w32(w, A(j, 0x507df4), r32(w, A(j, 0x507d34)));
            w32(w, A(j, 0x507df8), r32(w, A(j, 0x507d38)));
            w32(w, A(j, 0x507dfc), r32(w, A(j, 0x507d3c)));
            w32(w, A(j, 0x507e00), r32(w, A(j, 0x507d4c)));
            w32(w, A(j, 0x507e04), r32(w, A(j, 0x507d50)));
            w32(w, A(j, 0x507e08), r32(w, A(j, 0x507d54)));
            C3(MT2W_COPY_WORDS, P(w, car + 0x800), P(w, car + 0x758), 12);
        }
        C2(MT2W_HIT_RECOVER, P(w, car), 0);
    }
}

static uint32_t slot_pos(const Mt2World* w, int32_t i)
{
    int32_t k = r32(w, 0x502fec + 4 * (uint32_t)i);
    if (r8(w, 0x502fdc + (uint32_t)i) == 0)
        return (uint32_t)k * HS + 0x5030a8 + 0x6e4;
    return (uint32_t)k * AS + 0x50760c + 0x728;
}

/* the original's bit by bit square root, signed compares */
static uint32_t isqrt(uint32_t v)
{
    uint32_t r = 0, b = 0x40000000;
    do {
        if ((int32_t)v >= (int32_t)b && (int32_t)(v - b) >= (int32_t)r) {
            v = v - b - r;
            r = (r >> 1) | b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    } while (b != 0);
    return r;
}

static void pairs(const Mt2World* w)
{
    for (int32_t i = 0; i < r32(w, 0x503084); i++) {
        uint32_t pa = slot_pos(w, i);
        for (int32_t j = 0; j < r32(w, 0x503084); j++) {
            if (j == i)
                continue;
            uint32_t pb = slot_pos(w, j);
            uint32_t v = 0x50c2c8 + (uint32_t)i * 0xb4 + (uint32_t)j * 0xc;
            uint32_t nrm = 0x50d0d8 + (uint32_t)i * 0xb4 + (uint32_t)j * 0xc;
            uint32_t ang = 0x50db62 + (uint32_t)i * 0x1e + (uint32_t)j * 2;
            uint32_t pj = 0x50cd54 + (uint32_t)i * 0x3c + (uint32_t)j * 4;
            w32(w, v, wsub(r32(w, pb), r32(w, pa)));
            w32(w, v + 4, wsub(r32(w, pb + 4), r32(w, pa + 4)));
            w32(w, v + 8, wsub(r32(w, pb + 8), r32(w, pa + 8)));
            C2(MT2W_NORMALIZE, P(w, v), P(w, nrm));
            int32_t old = hi16(w, ang);
            int32_t a = (int32_t)(uint32_t)C2(MT2W_ATAN2, V(r32(w, nrm)), V(r32(w, nrm + 4)));
            w16(w, ang + 2, a);
            w32(w, pj, wsub(a, old));
            C1(MT2W_ANGLE_WRAP, P(w, pj));
            int32_t x = r32(w, v), y = r32(w, v + 4), z = r32(w, v + 8);
            uint32_t sq = (uint32_t)wmul(z, z) + ((uint32_t)wmul(x, x) + (uint32_t)wmul(y, y));
            w32(w, 0x50dd28 + (uint32_t)i * 0x3c + (uint32_t)j * 4, (int32_t)isqrt(sq));
        }
    }
}

void mt2_world_update(const Mt2World* w)
{
    int32_t cb_id = -1;
    if ((C0(MT2W_EVENT_TYPE) & 0xff) == 8) {
        C0(MT2W_456_06C);
        if (r32(w, 0x502fa4) != 2)
            C0(MT2W_472_B70);
    }
    human_loop(w);
    if (r8(w, 0x502f9c) == 0)
        multi_moods(w);
    else
        single_timers(w);
    ai_loop(w, &cb_id);
    boss_and_end(w);
    collide(w);
    pairs(w);

    C1(MT2W_46F_CB0, (C0(MT2W_46F_C04) & 0xff) ? 0x46fcc0u : 0u);
    C0(MT2W_493_038A);
    for (int32_t j = 0; j < r32(w, 0x50c2b0); j++)
        if (r8(w, A(j, 0x507674)) != 0 || r8(w, A(j, 0x50763e)) == 0)
            C2(MT2W_SOUND_9, 9, V(hi16(w, A(j, 0x50760e))));
    if ((C0(MT2W_46F_CA8) & 0xff) != 0) {
        C1(MT2W_46F_CA0, 0);
        C4(MT2W_TRANSITION, 0x3f, 0, 0, 99);
    }
    C0(MT2W_46F_94C);
    C0(MT2W_46F_958);
    C0(MT2W_493_038B);
    C0(MT2W_44A_59C);
    C0(MT2W_44A_D14);
    C0(MT2W_44B_B18);
    C0(MT2W_44B_BE4);
    C0(MT2W_44B_D3C);
    C0(MT2W_44C_EE0);
    C0(MT2W_44D_1F4);
    C0(MT2W_454_50C);
    if ((C0(MT2W_EVENT_TYPE) & 0xff) == 6)
        C0(MT2W_455_050);
}
