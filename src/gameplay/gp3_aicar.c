/*
 * gp3_aicar.c: the AI car per frame update, TM2.EXE 0x00422e08 (see gp3_aicar.h).
 */
#include "gp3_aicar.h"

#include <string.h>

#include "../math/tm2math.h"

static int32_t g8(const uint8_t* p, int o) { return (int8_t)p[o]; }
static int32_t g16(const uint8_t* p, int o) { int16_t v; memcpy(&v, p + o, 2); return v; }
static int32_t g32(const uint8_t* p, int o) { int32_t v; memcpy(&v, p + o, 4); return v; }
static void s16(uint8_t* p, int o, int32_t v) { int16_t x = (int16_t)v; memcpy(p + o, &x, 2); }
static void s32(uint8_t* p, int o, int32_t v) { memcpy(p + o, &v, 4); }

static int32_t wmul(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
static int32_t wadd(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
static int32_t wsub(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
static int32_t iabs(int32_t v) { int32_t s = v >> 31; return (int32_t)(((uint32_t)v ^ (uint32_t)s) - (uint32_t)s); }
static int32_t wrap(int32_t v) { tm2_angle_wrap32(&v); return v; }

#define B(o) (car[(o)])
#define S(o) g16(car, (o))
#define I(o) g32(car, (o))
#define WS(o, v) s16(car, (o), (v))
#define WI(o, v) s32(car, (o), (v))
#define TICKS ((int32_t)tm2_frame_ticks60())

typedef struct {
    const Gp3AiCarEnv* e;
    uint8_t* car;
} Ctx;

static uintptr_t call(const Ctx* c, int k, uintptr_t a, uintptr_t b, uintptr_t cc, uintptr_t d) {
    return c->e->fn[k](c->e->user, a, b, cc, d);
}
#define CALL0(k) call(&cx, (k), 0, 0, 0, 0)
#define CALLP(k) call(&cx, (k), (uintptr_t)car, 0, 0, 0)
#define CALLP2(k, b) call(&cx, (k), (uintptr_t)car, (uintptr_t)(uint32_t)(b), 0, 0)

/* ai_target_car: the chased car's block and its human flag */
static uint8_t* target(const Ctx* c, uint8_t* hum) {
    *hum = 0;
    return (uint8_t*)call(c, GP3A_TARGET, (uintptr_t)(uint32_t)g16(c->car, 4), (uintptr_t)hum, 0, 0);
}

static int32_t node_zone(const Gp3AiCarEnv* e, int32_t n) { return (int8_t)e->nodes[n * 24 + 0x17]; }

static void reset_turn(const Ctx* c) {
    uint8_t* car = c->car;
    call(c, GP3A_427710, (uintptr_t)car, 0, 0, 0);
    B(0x39) = 0;
    WS(0x466, 0);
    B(0x44b) = 0;
}

static void drop_route(const Ctx* c, int32_t dl) {
    uint8_t* car = c->car;
    call(c, GP3A_4334C8, (uintptr_t)car, (uintptr_t)(uint32_t)-1, 0, 0);
    call(c, GP3A_4259EC, (uintptr_t)car, 0, 0, 0);
    call(c, GP3A_433000, (uintptr_t)car, (uintptr_t)(uint32_t)dl, 0, 0);
}

static void ram_kind(const Ctx* c) {
    uint8_t* car = c->car;
    B(0x841) = 6;
    if (I(0x28) < 4000 && (call(c, GP3A_RAND, 0, 0, 0, 0) & 1)) B(0x841) = 2;
}

void gp3_ai_car_update(const Gp3AiCarEnv* e, uint8_t* car) {
    Ctx cx = { e, car };
    uint8_t hum;
    uint8_t* t;
    uint8_t* body;
    int32_t v, w, a;

    if (B(0x68) && g8(car, 0x4f1) == 4) return;
    if (B(0x4ef)) {
        WI(0x5bc, (int32_t)CALLP2(GP3A_42C3E0, 0));
        CALLP(GP3A_432940);
        B(0x4ef) = 0;
    }
    int32_t old_heading = I(0x73c);
    call(&cx, GP3A_43A944, (uintptr_t)(car + 0x4ec), 0, 0, 0);
    int32_t fps = tm2_frame_fps();
    if (S(0x500) != fps) {
        WS(0x712, wmul(S(0x500), S(0x712)) / fps);
        WS(0x708, wmul(S(0x500), S(0x708)) / fps);
        WS(0x71c, wmul(S(0x500), S(0x71c)) / fps);
        v = S(0x544) * 0x13;
        WS(0x500, fps);
        v = wmul(v, TICKS) / 100;
        WI(0x5ac, wmul(S(0x562), (int32_t)((uint32_t)v << 5)) / 100);
    }
    if (B(0x75)) {
        WS(0x506, S(0x506) - TICKS);
        if (S(0x506) <= 0) {
            B(0x75) = 0;
            if (S(0x508) == 1) {
                call(&cx, GP3A_470940, (uintptr_t)(uint32_t)S(0x30), 3, 0, 0);
                WS(0x508, 0);
            }
        }
    }
    B(0x71) = 0;
    WS(0xda, 0);

    if (!B(0) || B(0x68)) {
        if (B(0x68)) WI(0x28, 0);
    } else {
        t = target(&cx, &hum);
        body = t + (hum ? 0x6c4 : 0x708);
        {
            int32_t dx = wsub(g32(body, 0x20), I(0x728)), dy = wsub(g32(body, 0x24), I(0x72c));
            int32_t n = wadd(wmul(dx, dx), wmul(dy, dy));
            uint32_t r = 0, bit = 0x40000000;
            do {
                if (n >= (int32_t)bit && wsub(n, (int32_t)bit) >= (int32_t)r) {
                    n = wsub(wsub(n, (int32_t)bit), (int32_t)r);
                    r = (r >> 1) | bit;
                } else {
                    r >>= 1;
                }
                bit >>= 2;
            } while (bit);
            WI(0x28, (int32_t)r);
        }
        WS(0x26, g16(body, 0x28) - S(0x730));
        if (!B(0x3e)) {
            if (I(0x28) > I(0x2c)) {
                WS(0x462, 0);
                WS(0x460, S(0x460) + TICKS);
            } else if (I(0x28) < I(0x2c)) {
                WS(0x460, 0);
                WS(0x462, S(0x462) + TICKS);
            } else {
                WS(0x462, 0);
                WS(0x460, 0);
            }
        }
        if (!B(0x73)) WI(0x2c, I(0x28));
        if (I(0x28) < 0x3020 || (B(0x449) && S(0x45c) > 0)) {
            int32_t d = I(0x28);
            B(0x32) = 1;
            B(0x33) = S(0x26) < 0xa0;
            B(0x34) = S(0x16) > d;
            if (S(0x18) > d) {
                B(0x36) = 1; B(0x37) = 1; B(0x38) = 1; B(0x35) = 1;
            } else {
                B(0x35) = 0;
                if (S(0x1a) > d) {
                    B(0x37) = 1; B(0x38) = 1; B(0x36) = 1;
                } else {
                    B(0x36) = 0;
                    if (S(0x1c) > d) {
                        B(0x38) = 1; B(0x37) = 1;
                    } else {
                        B(0x37) = 0;
                        B(0x38) = S(0x1e) > d;
                    }
                }
            }
        } else {
            B(0x33) = 0; B(0x34) = 0; B(0x35) = 0; B(0x36) = 0; B(0x37) = 0; B(0x38) = 0; B(0x32) = 0;
        }
        CALLP(GP3A_425674);
        t = target(&cx, &hum);
        body = t + (hum ? 0x6c4 : 0x708);
        if (!B(0x73)) {
            int32_t dy = wsub(g32(body, 0x24), I(0x72c));
            if (dy == 0) dy = 1;
            WS(0x22, tm2_atan2(wsub(g32(body, 0x20), I(0x728)), dy));
            tm2_angle_wrap16((int16_t*)(car + 0x22));
        }
        WS(0x20, S(0x22) - S(0x73c));
        tm2_angle_wrap16((int16_t*)(car + 0x20));
        WS(0x24, S(0x22) - g16(body, 0x34) - 0x800);
        tm2_angle_wrap16((int16_t*)(car + 0x24));
        if (iabs(S(0x20)) > 0x600) B(0xc8) = 1;
        else if (S(0x20) > 0x200) B(0xc8) = 3;
        else if (S(0x20) < -0x200) B(0xc8) = 2;
        else B(0xc8) = 0;

        if (!B(0x449)) {
            CALL0(GP3A_DIFF);
            if ((uint8_t)CALL0(GP3A_EVENT) == 5 && B(0x36)) {
                uint8_t* st;
                t = target(&cx, &hum);
                body = t + (hum ? 0x6c4 : 0x708);
                st = t + (hum ? 0x138 : 0x4ec);
                if (g16(body, 0xa) < g32(st, 0xc4) && iabs(wsub(g32(body, 0x28), I(0x730))) < 0x50 &&
                    (uint8_t)call(&cx, GP3A_45B3FC, (uintptr_t)(uint32_t)S(0x30), 0, 0, 0) == 0) {
                    CALLP(GP3A_427838);
                    B(0x454) = 1;
                    B(0x449) = 1;
                    WS(0x45c, S(0x458));
                }
            } else {
                B(0x454) = 0;
            }
        }
        if (g8(car, 1) < 2 && S(0x578) < 0) {
            B(1) = 2;
            v = (int32_t)CALL0(GP3A_RAND);
            WS(0x10, 0);
            WS(0xe, 0);
            WS(0x12, S(0x574) + S(0x574) * (v & 3));
        }
        WS(0x10, S(0x10) + TICKS);
        if (S(0x12) < S(0x10)) {
            int32_t pct = wmul(I(0x58c), 100) / S(0x526), inv, rem, r;
            if (pct > 100) pct = 100;
            inv = 100 - pct;
            WS(0x570, wmul(S(0x568), pct) / 100 + wmul(S(0x56a), inv) / 100);
            r = (int32_t)CALL0(GP3A_RAND);
            rem = r % 100;
            if (rem < S(0x570)) {
                v = S(0x572);
                B(1) = 1;
            } else if (rem < wmul(S(0x56c), pct) / 100 + wmul(S(0x56e), inv) / 100 + S(0x570)) {
                v = S(0x574);
                B(1) = 2;
            } else {
                v = S(0x576);
                B(1) = 3;
            }
            WS(0x12, v);
            WS(0xe, 0);
            WS(0x10, 0);
            CALLP(GP3A_410010);
        } else {
            if (g8(car, 1) == 0) {
                int32_t lim;
                if (I(0x28) > 0x4b0) lim = 0;
                else if (I(0x28) < 1) lim = 0x800;
                else lim = 0x21340 / I(0x28);
                if (iabs(S(0x24)) < lim) {
                    WS(0xe, 0);
                    WS(0x10, 0);
                    B(1) = 2;
                    WS(0x12, S(0x574));
                }
            }
            CALLP(GP3A_410010);
        }
    }

    call(&cx, GP3A_CLEAR, (uintptr_t)(car + 0x40), 0, 0, 0);
    if (B(0x44c)) {
        a = wrap(I(0x73c) - S(0x22));
        if (iabs(a) < 0x200) {
            B(0x39) = 1;
            WS(0x466, S(0x466) + TICKS);
            if (S(0x466) > 300) {
                B(0x44c) = 0;
                if (B(0x39) || (B(0x449) && S(0x472) > 0x1800)) reset_turn(&cx);
            }
        }
    }
    if (!B(0x68) && B(0)) WS(0x466, S(0x466) + TICKS);
    v = (!B(0x39) && !B(0x449)) ? 600 : 3000;
    if (S(0x466) > v) B(0x3b) = 1;
    if (S(0x472) > 0x1800) {
        if (!B(0x39)) {
            B(0x3b) = 1;
            WS(0x8, 0xb4);
        } else if (B(0x39) || (B(0x449) && S(0x472) > 0x1800)) {
            reset_turn(&cx);
        }
    }
    if (B(0x3b)) {
        WS(0x8, S(0x8) + TICKS);
        if (S(0x8) > 0xb4 || S(0x472) > 0x1800 || S(0x468) > 3 || B(0x3f)) {
            CALLP(GP3A_427710);
            B(0x3f) = 0;
        }
    }

    if (B(0x447)) {
        int c4 = 0;
        if (B(0x68)) {
            WS(0x46a, 0x1e);
            B(0x3f) = 0;
        }
        CALLP2(GP3A_4291DC, 0);
        if (!B(0x75)) {
            if (!B(0x3f)) {
                if (I(0x588) == 0) WS(0xd2, S(0xd2) - TICKS);
                WI(0x73c, tm2_angle_interp(I(0x73c), S(0x488), 3));
                if ((S(0xd2) < 0x1f && e->grp[0x1218 + S(0x45e)] == 0) || S(0x712) == 0) {
                    c4 = 1;
                } else {
                    int32_t st = ((uint8_t)CALLP(GP3A_42CE2C) == 0) ? I(0x5b4) / 3 : I(0x5b4);
                    if (S(0x712) < 1) {
                        WS(0x712, S(0x712) + st);
                        if (S(0x712) > 0) WS(0x712, 0);
                    } else {
                        WS(0x712, S(0x712) - st);
                        if (S(0x712) < 0) WS(0x712, 0);
                    }
                }
                if (c4) {
                    call(&cx, GP3A_418CF0, (uintptr_t)(car + 0x708), (uintptr_t)(car + 0x68), 0, 0);
                    B(0xce) = 0;
                    B(0xcf) = 0;
                    B(0xd0) = 0;
                    WS(0xd2, 0);
                    if (!B(0x3d)) {
                        B(0x447) = 0;
                        B(0x3f) = 0;
                    } else {
                        B(0x3f) = 1;
                    }
                    CALLP(GP3A_427710);
                }
            } else {
                WS(0x46a, S(0x46a) + TICKS);
                call(&cx, GP3A_418CF0, (uintptr_t)(car + 0x708), (uintptr_t)(car + 0x68), 0, 0);
                WS(0x712, tm2_angle_step(S(0x712), (int32_t)((uint32_t)((TICKS * -0x2f8) / 100) << 5), 3));
                CALLP(GP3A_42577C);
                WI(0x73c, tm2_angle_interp(I(0x73c), S(0x482), 3));
                if (S(0x46a) > 0x1e) {
                    B(0x3f) = 0;
                    WS(0x46a, 0);
                    WS(0x712, 0);
                    B(0x447) = 0;
                }
            }
        } else if (S(0x712) != 0 || B(0x69)) {
            if (S(0x712) < 1) {
                WS(0x712, S(0x712) + (int16_t)I(0x5b4));
                if (S(0x712) > 0) {
                    WS(0x712, 0);
                    call(&cx, GP3A_418CF0, (uintptr_t)(car + 0x708), (uintptr_t)(car + 0x68), 0, 0);
                }
            } else {
                WS(0x712, S(0x712) - (int16_t)I(0x5b4));
                if (S(0x712) < 0) {
                    WS(0x712, 0);
                    call(&cx, GP3A_418CF0, (uintptr_t)(car + 0x708), (uintptr_t)(car + 0x68), 0, 0);
                }
            }
        }
        goto tail;
    }
    if (!B(0) || B(0x68)) goto tail;

    if (B(0x39)) {
        v = wmul(I(0x28), 2);
        WI(0x4b4, v);
        WI(0x4b8, v);
        WS(0x47a, v / 2);
        B(0x44b) = 0;
        WI(0x4c8, I(0x4b4) / 2);
        WS(0x47e, S(0x22));
        WS(0x488, S(0x22));
    }
    if (B(0x37)) {
        a = wrap(I(0x73c) - S(0x22));
        if (iabs(a) < 0x38 && (8 - (int8_t)CALL0(GP3A_EVENT)) * 0x3c + 0xb4 < S(0x86a)) {
            B(0x5b) = 1;
            WS(0x85a, 0);
        }
    }
    if (B(0x449) && S(0x45c) > 0) {
        t = target(&cx, &hum);
        v = hum ? g32(t, 0xc4) : g32(t, 0x45c);
        if (S(0x45c) != (int8_t)e->grp[0xce6 + node_zone(e, S(0x458)) * 0x23 + (v >> 16)] && !B(0x454))
            B(0x449) = 0;
    }

    if (B(0x43c) == 0) {
        int c40 = 0;
        if (!B(0x69)) call(&cx, GP3A_418CF0, (uintptr_t)(car + 0x708), (uintptr_t)(car + 0x68), 0, 0);
        if (I(0x4c8) == 0) {
            WS(0x47a, S(0x478));
            if (S(0x712) < I(0x5ac)) {
                WS(0x47a, wmul(S(0x712), S(0x478)) / I(0x5ac));
                if (S(0x47a) < 0xa0) WS(0x47a, 0xa0);
            }
        }
        if (I(0x4b8) < I(0x4b4) && !B(0x446)) {
            B(0x44f) = 1;
            B(0x39) = 0;
        }
        if (!B(0x44f)) {
            if (I(0x4b4) < S(0x47a)) {
                if (I(0x4c8) == 0) {
                    CALLP(GP3A_427838);
                    if (!B(0x446) && ((uint8_t)CALL0(GP3A_SINGLE) == 0 || S(0x46e) < 2)) CALLP(GP3A_427DD8);
                } else {
                    drop_route(&cx, 1);
                    WI(0x4c8, 0);
                }
                goto l49b8;
            }
            if (I(0x4b4) < I(0x4c8)) {
                drop_route(&cx, 1);
                WI(0x4c8, 0);
            }
            if (!B(0x39)) {
                v = I(0x4b4);
            } else {
                if (S(0x30) == 0xc0) {
                    v = wsub(I(0x28), 0x78);
                } else if (!B(0x3a)) {
                    v = wsub(I(0x28), S(0x16));
                } else {
                    int32_t u;
                    v = I(0x28);
                    t = target(&cx, &hum);
                    body = t + (hum ? 0x6c4 : 0x708);
                    u = wrap(wsub(I(0x73c), g32(body, 0x34)));
                    if (!B(0x3a) && !(iabs(u) < 0x400 && g16(body, 0xa) > 0) && g16(body, 0xa) >= 0) {
                        int32_t lim = (int32_t)((uint32_t)((TICKS * 0xbe) / 100) << 5);
                        if (iabs(g16(body, 0xa)) < lim) {
                            (void)TICKS;
                            CALLP2(GP3A_4291DC, 0);
                            goto l47c6;
                        }
                    }
                }
                CALLP2(GP3A_4291DC, 0);
            }
        l47c6:
            if (v < 1) {
                if (iabs(S(0x20)) > 0x70) {
                    B(0x39) = 0;
                    B(0x52) = 1;
                    goto l49b8;
                }
                B(0x53) = 1;
            } else {
                int32_t i8c;
                if (iabs(S(0x712)) < ((TICKS * 0x17c) / 100) * 0x20 || v > 0x1fffff) {
                    B(0x52) = 1;
                    goto l49b8;
                }
                if (S(0x45e) == node_zone(e, S(0x458))) {
                    i8c = I(0x5ac);
                } else {
                    i8c = (int32_t)call(&cx, GP3A_430414, (uintptr_t)(uint32_t)node_zone(e, S(0x456)),
                                        (uintptr_t)(uint32_t)node_zone(e, S(0x458)), 0, 0);
                    if (i8c == 0) {
                        i8c = I(0x5ac);
                    } else {
                        int32_t sp = S(0x712);
                        c40 = 1;
                        if (sp < i8c) {
                            B(0x52) = 1;
                            if (I(0x5a0) < i8c) B(0x43) = 1;
                        } else if (wadd(i8c / 4, i8c) < sp) {
                            B(0x53) = 1;
                        }
                    }
                }
                if (!c40) {
                    int32_t acc = I(0x5b4), sp, gap, lim;
                    if ((int32_t)CALL0(GP3A_DIFF) < 1) acc /= 8;
                    else if ((int32_t)CALL0(GP3A_DIFF) < 2) acc /= 2;
                    sp = S(0x712);
                    gap = wsub(sp, i8c);
                    lim = (int32_t)((uint32_t)v << 5);
                    if (S(0x712) != 0) lim = wmul(lim / sp, acc);
                    if (gap < 0) B(0x52) = 1;
                    else if (lim < gap) B(0x53) = 1;
                    else if (gap < lim / 2) B(0x52) = 1;
                }
            }
        } else {
            WI(0x754, 0);
            B(0x53) = 1;
            B(0x52) = 0;
            if (S(0x712) < I(0x5a4)) {
                WS(0x712, S(0x712) / 2);
                WI(0x4b8, I(0x4b4));
                WI(0x73c, tm2_angle_interp(I(0x73c), S(0x488), 3));
                a = wrap(I(0x73c) - S(0x488));
                if (iabs(a) < 0x38) {
                    call(&cx, GP3A_418CF0, (uintptr_t)(car + 0x708), (uintptr_t)(car + 0x68), 0, 0);
                    B(0x44f) = 0;
                }
            }
        }
    l49b8:
        if (S(0x30) == 0xf0) {
            CALLP(GP3A_432F2C);
            if (!B(0x450) && !B(0x451)) {
                if (B(0x452)) {
                    WI(0x73c, tm2_angle_interp(I(0x73c), S(0x22) < 1 ? -0x400 : 0x400, 5));
                    if (!B(0x74)) {
                        int32_t n;
                        B(0x53) = 1;
                        WS(0x712, 0);
                        B(0x52) = 0;
                        B(0x449) = 0;
                        n = S(0x458);
                        B(0x454) = 0;
                        if (n == 0x70 || n == 0x75) {
                            B(0x449) = 1;
                            WS(0x45c, S(0x458));
                        }
                        if (!B(0x449)) {
                            B(0x453) = 1;
                            B(0x452) = 0;
                        } else {
                            const uint8_t* nd = e->nodes + S(0x458) * 24;
                            WI(0x728, tm2_angle_step(I(0x728), g32(nd, 0), 5));
                            nd = e->nodes + S(0x458) * 24;
                            WI(0x72c, tm2_angle_step(I(0x72c), g32(nd, 4), 5));
                            nd = e->nodes + S(0x458) * 24;
                            WI(0x730, tm2_angle_step(I(0x730), g16(nd, 8) + S(0x52e), 5));
                        }
                        if (S(0x86a) < 0x79) {
                            WS(0x86a, S(0x86a) + TICKS);
                        } else {
                            ram_kind(&cx);
                            WS(0x85a, 0);
                            WS(0x86a, 0);
                            B(0x5b) = 1;
                            CALLP2(GP3A_43E6E0, 0);
                        }
                    }
                    if ((uint8_t)CALL0(GP3A_456C70) == 0) B(0x452) = 0;
                }
            } else if (I(0x4b4) < 0xe10 && I(0x4b4) > 0xc80) {
                int32_t dy;
                if (!B(0x450)) {
                    B(0x451) = 0;
                    B(0x452) = 1;
                    call(&cx, GP3A_421920, (uintptr_t)car, 0, 0xd, 0);
                    WS(0x712, (int32_t)((uint32_t)((TICKS * 0x23a) / 100) << 5));
                    v = S(0x458);
                    if (v == 2 || v == 0x28) WS(0x458, 0x75);
                    else if (v == 0xb || v == 0x2c) WS(0x458, 0x70);
                } else {
                    B(0x450) = 0;
                    call(&cx, GP3A_421920, (uintptr_t)car, 0, 0xc, 0);
                    WS(0x712, (int32_t)((uint32_t)((TICKS * 0x70d) / 100) << 5));
                }
                dy = wsub(I(0x4d8), I(0x72c));
                if (dy == 0) dy = 1;
                WI(0x748, tm2_atan2(wsub(I(0x4d4), I(0x728)), dy));
                WI(0x748, wrap(I(0x748)));
                WI(0x73c, I(0x748));
            }
        }
        if (!B(0x452)) {
            if (!B(0x39)) {
                if (!B(0x48a)) {
                    WI(0x73c, tm2_angle_interp(I(0x73c), S(0x488), S(0x468) == 0 ? 5 : 9));
                    if (iabs(S(0x24)) < 0x71) {
                        int c28 = 0;
                        if (iabs(S(0x20)) < 0xe3) {
                            t = target(&cx, &hum);
                            v = hum ? g32(t, 0x1b0) : g32(t, 0x564);
                            if (S(0x566) < (v >> 16)) {
                                if (I(0x28) < 600) {
                                    c28 = 0;
                                    call(&cx, GP3A_421920, (uintptr_t)car, hum, 7, 1);
                                } else if (I(0x28) < 0x4b0) {
                                    c28 = 1;
                                }
                            }
                        } else if (iabs(S(0x20)) > 0x71c && !B(0x44e) &&
                                   (int32_t)CALL0(GP3A_RAND) % 100 < S(0x57c)) {
                            c28 = 1;
                            B(0x44e) = 1;
                        }
                        if (c28) {
                            int32_t out = 0;
                            call(&cx, GP3A_430460, (uintptr_t)car, (uintptr_t)&out, 1, 0);
                            CALLP2(GP3A_4291DC, out < 1 ? 2 : 1);
                        }
                    }
                } else {
                    int32_t i7c, u;
                    if (!B(0x44d)) {
                        int32_t out = 0;
                        call(&cx, GP3A_430460, (uintptr_t)car, (uintptr_t)&out, 1, 0);
                        if (iabs(I(0x49c)) / 2 < iabs(wsub(out, I(0x4a0)))) {
                            B(0x44d) = 1;
                            i7c = S(0x488);
                        } else {
                            i7c = wrap(S(0x488) + (B(0x48a) == 2 ? 0xaa : -0xaa));
                        }
                    } else {
                        i7c = S(0x488);
                    }
                    u = wrap(wsub(i7c, wadd(S(0x71c), I(0x73c))));
                    if (iabs(u) < 0x39) {
                        if (B(0x44d)) CALLP2(GP3A_4291DC, 0);
                    } else if (B(0x48a) == 2) {
                        if (!B(0x44d)) {
                            if (u > 0) {
                                B(0x40) = 1;
                                B(0x50) = 1;
                            }
                        } else if (u < 0) {
                            B(0x4f) = 1;
                        }
                    } else if (B(0x48a) == 1) {
                        if (!B(0x44d)) {
                            if (u < 0) {
                                B(0x41) = 1;
                                B(0x4f) = 1;
                            }
                        } else if (u > 0) {
                            B(0x50) = 1;
                        }
                    }
                }
            } else {
                if ((uint8_t)CALLP2(GP3A_4295FC, I(0x4b4)) == 0) B(0x39) = 0;
                else WI(0x73c, tm2_angle_interp(I(0x73c), S(0x22), 5));
            }
        }
        if (B(0x33) && g8(car, 1) < 2 && S(0x510) < S(0x460) && B(0xc8) == 1 && S(0x46e) > 0) {
            CALLP(GP3A_427838);
            CALLP2(GP3A_4334C8, S(0x456));
            CALLP(GP3A_4259EC);
            CALLP2(GP3A_433000, 0);
        }
    } else {
        if (B(0x452) && (uint8_t)CALL0(GP3A_456C70) == 0) B(0x452) = 0;
        if (!B(0x449) || S(0x458) != S(0x45c)) {
            int c4 = 0;
            int32_t u;
            CALLP(GP3A_42577C);
            if ((uint8_t)CALLP2(GP3A_4295FC, I(0x4bc)) == 0) {
                u = wsub(I(0x73c), S(0x482));
                if (B(0x39) || (B(0x449) && S(0x472) > 0x1800)) reset_turn(&cx);
            } else {
                u = wsub(I(0x73c), S(0x22));
                B(0x39) = 1;
                t = target(&cx, &hum);
                v = hum ? g32(t, 0x1b0) : g32(t, 0x564);
                if (S(0x30) == 0x20 || B(0x44c)) B(0x3a) = 1;
                if (S(0x566) < (v >> 16) || (CALL0(GP3A_RAND) & 3) != 1) B(0x3a) = 0;
                else B(0x3a) = 1;
            }
            u = wrap(u);
            if (!B(0x44a)) {
                if (S(0x466) < 0x259 && S(0x472) < 0x1801) {
                    a = iabs(u);
                    if (a < 0x39) {
                        if (!B(0x446) && S(0x30) != 0xf0) goto l414d;
                        WS(0x544, 0);
                        B(0x44a) = 1;
                    } else if (a < 0x156) {
                        if (iabs(S(0x484)) < a) {
                            if (!B(0x446) && S(0x30) != 0xf0) goto l414d;
                            WS(0x544, 0);
                            B(0x44a) = 1;
                        } else {
                            WS(0x484, u);
                        }
                    } else if (S(0x30) == 0xf0 && (I(0x4b8) < I(0x4b4) || I(0x4b4) < 0x50)) {
                        goto l414d;
                    }
                } else {
                l414d:
                    c4 = 1;
                }
            } else {
                if (!B(0x446)) {
                    if (S(0x30) != 0xf0) goto l4027;
                    w = wrap(wsub(I(0x73c), I(0x748)));
                    if (w < 1) {
                        WI(0x748, wadd(I(0x748), TICKS * -0xaa));
                        w = wrap(wsub(I(0x73c), I(0x748)));
                        if (w > 0) goto l4002;
                    } else {
                        WI(0x748, wadd(I(0x748), TICKS * 0xaa));
                        w = wrap(wsub(I(0x73c), I(0x748)));
                        if (w < 0) {
                        l4002:
                            c4 = 1;
                            call(&cx, GP3A_418CF0, (uintptr_t)(car + 0x708), (uintptr_t)(car + 0x68), 0, 0);
                        }
                    }
                    WS(0x708, S(0x708) / 2);
                } else if (iabs(S(0x712)) < wmul(I(0x59c), 2)) {
                l4027:
                    c4 = 1;
                } else if (!B(0x74)) {
                    WS(0x712, tm2_angle_step(S(0x712), 0, 9));
                }
                if (!B(0x74)) {
                    WS(0x708, tm2_angle_interp(S(0x708), 0, 5));
                    WI(0x73c, tm2_angle_interp(I(0x73c), S(0x47e), 5));
                }
            }
            if (!c4) {
                int b3;
                a = iabs(u);
                if (a < iabs(S(0x708)) * 2) b3 = 1;
                else if (a < 0x401) b3 = a < 0x39;
                else b3 = 0;
                if (iabs(S(0x71c)) < S(0x55c)) b3 = 0;
                if (a < 0xaa) B(0x44b) = 0;
                if (b3) {
                    B(0x4f) = 0;
                    B(0x50) = 0;
                } else if (u < 1) {
                    B(0x50) = 1;
                } else {
                    B(0x4f) = 1;
                }
                if (!B(0x446) && S(0x30) != 0xf0) {
                    if (!B(0x69)) call(&cx, GP3A_418CF0, (uintptr_t)(car + 0x708), (uintptr_t)(car + 0x68), 0, 0);
                } else {
                    WI(0x754, 0);
                    B(0x42) = 1;
                    if (!B(0x4f)) {
                        if (B(0x50)) B(0x40) = 1;
                    } else {
                        B(0x41) = 1;
                    }
                }
                if (!B(0x44b) || B(0x73)) {
                    int32_t h = iabs(u / 2), cur = S(0x708);
                    if (h < cur) WS(0x708, h);
                    else if (cur < -h) WS(0x708, -h);
                } else {
                    int32_t dy = wsub(I(0x4e8), I(0x72c)), g, r, q, k;
                    if (dy == 0) dy = 1;
                    g = wrap(S(0x47e) - tm2_atan2(wsub(I(0x4e4), I(0x728)), dy));
                    r = S(0x476) == 0 ? 0 : wmul(g, 100) / S(0x476);
                    q = wrap(S(0x47e) - S(0x474));
                    k = S(0x47e) - wmul(r, q) / 100;
                    w = wrap(wsub(k, I(0x73c)));
                    if (TICKS * 0x4f < iabs(w)) {
                        B(0x44b) = 0;
                    } else {
                        WS(0x708, w);
                        WI(0x73c, wrap(wsub(k, w)));
                        if (S(0x550) < S(0x708)) WS(0x708, S(0x550));
                        else if (S(0x708) < -S(0x550)) WS(0x708, -S(0x550));
                    }
                }
            } else {
                CALLP(GP3A_432BA0);
                if (B(0x446)) WS(0x712, I(0x59c));
                if (!B(0x69)) call(&cx, GP3A_418CF0, (uintptr_t)(car + 0x708), (uintptr_t)(car + 0x68), 0, 0);
            }
            if (!B(0x44a) && S(0x712) < I(0x5ac) && !B(0x446)) {
                B(0x52) = 1;
                B(0x53) = 0;
            }
        } else if (I(0x28) < 0x15e1 && S(0x466) < 0x1a5) {
            if (iabs(S(0x712)) < I(0x59c)) {
                B(0x53) = 1;
                if (!B(0x452)) {
                    a = wrap(I(0x73c) - S(0x22));
                    v = (iabs(S(0x708)) * 0x155) / S(0x550);
                    if (a >= -v) {
                        if (v < a) B(0x4f) = 1;
                    } else {
                        B(0x50) = 1;
                    }
                }
                if (I(0x28) > 0xc80) {
                    if ((8 - (int8_t)CALL0(GP3A_EVENT)) * 0x3c + 0x78 < S(0x86a)) {
                        if ((int8_t)CALL0(GP3A_EVENT) == 8 || S(0x30) == 0x20) {
                            if (!B(0x452)) {
                                if ((CALL0(GP3A_RAND) & 5) == 1) B(0x841) = 5;
                                else B(0x841) = (CALL0(GP3A_RAND) & 1) == 0 ? 1 : 2;
                            } else {
                                ram_kind(&cx);
                            }
                        } else {
                            B(0x841) = 0;
                        }
                        WS(0x86a, 0);
                        B(0x5b) = 1;
                        WS(0x85a, 0);
                    } else {
                        WS(0x86a, S(0x86a) + TICKS);
                    }
                }
            } else if (wmul(wmul(I(0x4b4), I(0x5b4)) / S(0x712), 0x20) < S(0x712)) {
                B(0x53) = 1;
            } else {
                B(0x53) = 0;
            }
        } else {
            uint8_t* st;
            t = target(&cx, &hum);
            st = t + (hum ? 0x138 : 0x4ec);
            if ((int8_t)st[8] == S(6)) {
                WS(0x466, 0);
                st[8] = 0xff;
            } else {
                a = wrap(I(0x73c) - S(0x482));
                v = (iabs(S(0x708)) * 0x155) / S(0x550);
                if (a >= -v) {
                    if (v < a) {
                        B(0x4f) = 1;
                    } else {
                        B(0x454) = 0;
                        B(0x449) = 0;
                    }
                } else {
                    B(0x50) = 1;
                }
                if (S(0x466) > 600) {
                    B(0x454) = 0;
                    B(0x449) = 0;
                }
                B(0x53) = 1;
            }
        }
    }
    B(0x50) = B(0x50) && !B(0x73);
    B(0x4f) = B(0x4f) && !B(0x73);
    B(0x6e) = B(0x4f) || B(0x50);
    B(0x53) = B(0x53) || B(0x75);
    B(0x52) = B(0x52) && !B(0x75) && !B(0x7a);
    B(0x4e) = B(0x4e) && !B(0x75) && !B(0x7a);
    if (B(0x85)) {
        B(0x43) = 0;
        B(0x52) = 0;
    }
    {
        int on;
        if (B(0x43)) {
            on = 1;
        } else {
            on = 0;
            if ((int32_t)CALL0(GP3A_DIFF) > 0 && I(0x5b0) < S(0x712)) on = 1;
        }
        if (on) {
            v = (S(0x53e) * 3) / 4;
            if (v < (int32_t)call(&cx, GP3A_439588, (uintptr_t)(uint32_t)S(0x30), 0, 0, 0)) on = 0;
        }
        if (!on) {
            if (B(0x84)) {
                B(0x84) = 0;
                call(&cx, GP3A_470320, (uintptr_t)(uint32_t)(S(0x30) + 6), 0, 0, 0);
            }
        } else if (!B(0x84)) {
            B(0x84) = 1;
            call(&cx, GP3A_470320, (uintptr_t)(uint32_t)(S(0x30) + 6), 0, 1, 0);
        }
    }
    CALLP2(GP3A_4168E4, 0);

tail:
    CALLP2(GP3A_42C4D4, 0);
    CALLP2(GP3A_42A020, 0);
    CALLP2(GP3A_4220B8, 0);
    if (B(0x7c)) CALLP2(GP3A_4305FC, 0);
    CALLP2(GP3A_420E48, 0);
    CALLP2(GP3A_42D470, 0);
    if (B(0x68) && g8(car, 0x4f1) != 4) CALLP2(GP3A_422704, 0);
    if (S(0x30) == 0xf0) {
        t = target(&cx, &hum);
        body = t + (hum ? 0x6c4 : 0x708);
        if (!B(0x37) || I(0x5a4) <= g16(body, 0xa)) {
            if (B(0x6d)) B(0x6d) = 0;
        } else {
            call(&cx, GP3A_43953C, (uintptr_t)(car + 0x4ec), 0x19, 0, 0);
            B(0x6d) = 1;
            if (S(0x468) < 1) {
                v = (S(0x712) / 32) * 0x19;
                if ((v << 2) / (TICKS * 0x13) < 0x1e) {
                    if (S(0x610) < 0x10 || S(0x658) < 0x10 || S(0x6a0) < 0x10 || S(0x6e8) < 0x10) {
                        if (g32(body, 0x24) > -0x2620) WI(0x73c, tm2_angle_interp(I(0x73c), S(0x22), 3));
                    } else {
                        call(&cx, GP3A_421920, (uintptr_t)car, 0, 0xb, 1);
                    }
                }
            }
        }
    }
    call(&cx, GP3A_453538, (uintptr_t)(uint32_t)S(0x30), (uintptr_t)(uint32_t)S(0x712), 0, 0);
    if (I(0x87c)) {
        uint8_t* q = e->resolve(e->user, (uint32_t)I(0x87c));
        if (q[0] == 0) {
            if (q[7]) {
                call(&cx, GP3A_432340, (uintptr_t)(uint32_t)g16(q, 0xe), (uintptr_t)(uint32_t)S(0x73c), 1, 0);
                s16(q, 0xe, -1);
                q[7] = 0;
            }
        } else if (q[2] == 0 && q[1] == 0) {
            q[0] = 0;
        } else if (q[1] == 0) {
            CALLP2(GP3A_4316B4, 0);
        } else {
            CALLP2(GP3A_430BB4, 0);
        }
    }
    if (!B(0)) {
        WI(0x730, wadd(I(0x730), S(0x52e)));
    } else {
        CALLP2(GP3A_418450, 0);
        CALLP2(GP3A_41858C, 0);
    }
    {
        uint16_t d = (uint16_t)wsub(old_heading, I(0x73c));
        uint16_t sg = (uint16_t)(wsub(old_heading, I(0x73c)) >> 31);
        WS(0x472, (uint16_t)(S(0x472) + (uint16_t)((d ^ sg) - sg)));
    }
    if (!B(0x87)) {
        if (I(0x730) < (int32_t)CALLP2(GP3A_45C248, 0)) B(0x87) = 1;
    } else if (I(0x730) < (int32_t)CALL0(GP3A_45C2A0)) {
        WI(0x58c, -1);
    } else if ((int32_t)CALLP2(GP3A_45C248, 0) < I(0x730)) {
        B(0x87) = 0;
    }
    CALLP2(GP3A_453C78, 0);
    if (I(0x584)) {
        call(&cx, GP3A_43AF58, (uintptr_t)car, (uintptr_t)(uint32_t)S(0x30), 1, 0);
        WI(0x584, 0);
    }
    CALLP2(GP3A_418D38, 0);
    CALLP2(GP3A_417640, 0);
}
