/*
 * gp3_wfs.c: weaponFireSpecial, TM2.EXE 0x004401fc (see gp3_wfs.h).
 */
#include "gp3_wfs.h"

#include <string.h>

#include "../math/tm2math.h"

typedef const Gp3WfsEnv E;

static uint8_t* P(E* e, uint32_t a) {
    for (int i = 0; i < e->nreg; i++)
        if (a - e->reg[i].base < e->reg[i].size) return e->reg[i].host + (a - e->reg[i].base);
    static uint8_t sink[16];
    return sink;    /* outside the map: the diff notices through the recorded calls / memory */
}
static int32_t r32(E* e, uint32_t a) { int32_t v; memcpy(&v, P(e, a), 4); return v; }
static int16_t r16(E* e, uint32_t a) { int16_t v; memcpy(&v, P(e, a), 2); return v; }
static uint8_t r8(E* e, uint32_t a) { return *P(e, a); }
static void w32(E* e, uint32_t a, int32_t v) { memcpy(P(e, a), &v, 4); }
static void w16(E* e, uint32_t a, int32_t v) { int16_t s = (int16_t)v; memcpy(P(e, a), &s, 2); }
static void w8(E* e, uint32_t a, int32_t v) { *P(e, a) = (uint8_t)v; }

static uint32_t C0(E* e, uint32_t fn) { return e->call(e->user, fn, 0, 0, 0, 0, 0, 0); }
static uint32_t C1(E* e, uint32_t fn, uint32_t a) { return e->call(e->user, fn, a, 0, 0, 0, 0, 0); }
static uint32_t C2(E* e, uint32_t fn, uint32_t a, uint32_t b) { return e->call(e->user, fn, a, b, 0, 0, 0, 0); }
static uint32_t C3(E* e, uint32_t fn, uint32_t a, uint32_t b, uint32_t c) {
    return e->call(e->user, fn, a, b, c, 0, 0, 0);
}
static uint32_t C4(E* e, uint32_t fn, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    return e->call(e->user, fn, a, b, c, d, 0, 0);
}

static int32_t iabs(int32_t v) { return v < 0 ? (int32_t)(0u - (uint32_t)v) : v; }
static int32_t sub(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
static int32_t add(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }

/* the inline "largest axis distance" of the original: signed max of the three |d| (|INT_MIN| is
 * INT_MIN, compared signed, as cdq / xor / sub do) */
static int32_t maxabs3(int32_t dx, int32_t dy, int32_t dz) {
    int32_t ax = iabs(dx), ay = iabs(dy), az = iabs(dz);
    int32_t m = az > ay ? az : ay;
    return ax < m ? m : ax;
}
static int32_t dist_to(E* e, uint32_t a, uint32_t b) {
    return maxabs3(sub(r32(e, a), r32(e, b)), sub(r32(e, a + 4), r32(e, b + 4)), sub(r32(e, a + 8), r32(e, b + 8)));
}

/* homing link table 0x004ff7f8, 5 x 0x14 */
#define LT 0x4FF7F8u
#define LE(i) (LT + 0x14u * (uint32_t)(i))
/* the slot for owner: -1 when an active entry (timer > 0) already has it, else the first idle one */
static int link_find(E* e, int32_t owner) {
    for (int i = 0; i < 5; i++) {
        int32_t t = r16(e, LE(i) + 4);
        if (t > 0 && r16(e, LE(i)) == owner) return -1;
        if (t < 1) return i;
    }
    return -1;
}

/* 0x0046cc54 ? 0x0046f980(p, -1) : 0x0046f974(p) */
static void snd_pos(E* e, uint32_t p) {
    if (C0(e, 0x46CC54)) C2(e, 0x46F980, p, 0xFFFFFFFFu);
    else C1(e, 0x46F974, p);
}
/* position, then 0x0046f8b0(code, 0x0046f988(), 0x0046f990(), 99) (0x0046f990 first) */
static void snd_play(E* e, uint32_t p, int32_t code) {
    snd_pos(e, p);
    uint32_t b = C0(e, 0x46F990);
    uint32_t c = C0(e, 0x46F988);
    C4(e, 0x46F8B0, (uint32_t)code, c, b, 99);
}

typedef struct {
    uint32_t car, stats, body, ws, flags, model;
    uint8_t human;
    int32_t id;        /* i16 +0x00 / +0x04 */
    int32_t idx;       /* i16 +0x02 / +0x06 */
    int16_t base;      /* projectile id base: id + 1 / id + 0x32 */
    uint16_t type;
    int32_t w[3];      /* the head's (sin h, cos h, -sin p) >> 6 */
} Ctx;

/* scratch layout: locals the original keeps on its stack and passes by pointer */
#define S_ANG 0x00u
#define S_POS 0x0Cu
#define S_TGT 0x18u
#define S_BYTE 0x1Cu
#define S_VEC 0x20u

static void mtv(E* e, const Ctx* c, const int32_t v[3], uint32_t out) {
    int32_t m[9], o[3];
    memcpy(m, P(e, c->body + 0x50), sizeof m);
    tm2_mat_vec_mul_row(m, v, o);
    for (int i = 0; i < 3; i++) w32(e, out + 4 * i, add(o[i], r32(e, c->body + 0x20 + 4 * i)));
}

/* 0x00449048 (rear) or 0x00448de4 picks the target; the launch angles */
static void pick(E* e, const Ctx* c) {
    uint32_t s = e->scratch;
    if (r8(e, c->ws + 5)) {
        C3(e, 0x449048, (uint32_t)c->id, s + S_TGT, c->human);
        w32(e, s + S_ANG, r32(e, c->body + 0x2C));
        w32(e, s + S_ANG + 4, 0);
        w32(e, s + S_ANG + 8, (int32_t)(((uint32_t)r32(e, c->body + 0x34) + 0x800u) & 0xFFFu));
    } else {
        C3(e, 0x448DE4, (uint32_t)c->id, s + S_TGT, c->human);
        w32(e, s + S_ANG, r32(e, c->body + 0x2C));
        w32(e, s + S_ANG + 4, r32(e, c->body + 0x30));
        w32(e, s + S_ANG + 8, r32(e, c->body + 0x34));
    }
}

static int16_t launch(E* e, uint32_t fn, int32_t a, uint32_t s0, uint32_t s1) {
    uint32_t s = e->scratch;
    return (int16_t)e->call(e->user, fn, (uint32_t)a, (uint32_t)r32(e, s + S_TGT), s + S_ANG, s + S_POS, s0, s1);
}

/* the sound most specials end with: car 0 of a human hears it unpositioned */
static void fire_sound(E* e, const Ctx* c, int32_t code) {
    if (c->human && c->id == 0) C4(e, 0x46F8B0, (uint32_t)code, 0, 0, 99);
    else snd_play(e, c->body + 0x20, code);
}

/* type 0x30: link to the nearest other car within 0x50 */
static int type30(E* e, const Ctx* c, uint32_t model) {
    uint32_t pos = c->body + 0x20;
    int slot = link_find(e, c->base);
    if (slot < 0) return 0;
    int32_t n = (int32_t)C0(e, 0x45AFB4);
    int32_t m = (int32_t)C0(e, 0x45B078);
    int32_t my = r32(e, model + 0x158);
    int32_t best = 0x50, bestid = -1;
    uint32_t a8 = 0;
    uint8_t a0 = 0;
    for (int32_t i = 0; i < n; i++) {
        uint32_t o = C1(e, 0x45AE24, (uint32_t)i);
        int32_t d = dist_to(e, pos, o + 0xE8) >> 3;
        if (r32(e, o + 0x158) != my && d < 0x50 && d < best) {
            best = d;
            bestid = r32(e, o + 0x158);
            a8 = C1(e, 0x45AFBC, (uint32_t)i) + 0x6E4;
            a0 = 1;
            w32(e, 0x4FEF54, (int32_t)C1(e, 0x45AFBC, (uint32_t)i));
        }
    }
    for (int32_t i = 0; i < m; i++) {
        if (r32(e, C1(e, 0x45B080, (uint32_t)i) + 0x58C) < 1) continue;
        uint32_t o = C1(e, 0x45AE2C, (uint32_t)i);
        int32_t d = dist_to(e, pos, o + 0xE8) >> 3;
        if (r32(e, o + 0x158) != my && d < 0x50 && d < best) {
            best = d;
            bestid = r32(e, o + 0x158);
            a8 = C1(e, 0x45B080, (uint32_t)i) + 0x728;
            a0 = 0;
            w32(e, 0x4FEF54, (int32_t)C1(e, 0x45B080, (uint32_t)i));
        }
    }
    uint32_t le = LE(slot);
    w16(e, le + 4, 0x78);
    if (bestid < 0) {
        w16(e, le + 2, -1);
        w16(e, le + 8, -1);
        w16(e, le + 0, -1);
        w32(e, le + 0x10, 0);
    } else {
        w16(e, le + 0, c->base);
        w16(e, le + 2, r16(e, c->car + (c->human ? 2 : 6)));
        C4(e, 0x421920, (uint32_t)r32(e, 0x4FEF54), a0, 6, 0);
        w16(e, le + 8, bestid);
        w32(e, le + 0x10, (int32_t)a8);
    }
    w32(e, le + 0xC, (int32_t)pos);
    w16(e, le + 6, 0);
    return 1;
}

/* type 0x60: lock the nearest car ahead (within 400, +-0x200 of the heading) */
static void type60_pick(E* e, const Ctx* c, uint32_t o, int32_t* best, int32_t my) {
    uint32_t b = c->body;
    int32_t d = dist_to(e, b + 0x20, o + 0xE8) >> 3;
    int32_t dx = sub(r32(e, o + 0xE8), r32(e, b + 0x20));
    int32_t dy = sub(r32(e, o + 0xEC), r32(e, b + 0x24));
    if (dy == 0) dy = 1;
    int32_t a = tm2_atan2(dx, dy);
    if (r32(e, o + 0x158) == my || d >= 400 || d >= *best || iabs(sub(r32(e, b + 0x34), a)) >= 0x200) return;
    int ok;
    if ((uint8_t)C0(e, 0x474508) != 8) ok = 1;
    else if (iabs(r32(e, b + 0x20)) < 0xC80 && iabs(r32(e, b + 0x24)) < 0xC80) ok = r32(e, o + 0xF0) > 0x428;
    else ok = 1;
    if (!ok) return;
    w8(e, 0x4FFE34, 1);
    w16(e, 0x4FFE42, 0);
    *best = d;
    w16(e, 0x4FFE44, r32(e, o + 0x158));
}

static void type60(E* e, const Ctx* c) {
    uint32_t car = c->car;
    C4(e, 0x421920, car, c->human, 7, 1);
    uint32_t mdl;
    if (c->human) {
        mdl = C1(e, 0x45AE24, (uint32_t)(int32_t)r16(e, car));
        w16(e, 0x4FFE40, r16(e, car + 2));
        w32(e, 0x4FFE4C, (int32_t)(car + 0x1D8));
        w32(e, 0x4FFE50, r32(e, car + 0x1D8));
        w32(e, 0x4FFE48, (r32(e, car + 0x1EC) / 32) * 2);
        w32(e, 0x4FFE60, (int32_t)(car + 0x6E4));
        w32(e, 0x4FFE64, (int32_t)(car + 0x6F0));
        w32(e, 0x4FFE38, (int32_t)(car + 0x22C));
    } else {
        mdl = C1(e, 0x45AE2C, (uint32_t)(int32_t)r16(e, car + 4));
        w16(e, 0x4FFE40, r16(e, car + 6));
        w32(e, 0x4FFE48, (r32(e, car + 0x5A0) / 32) * 2);
        w32(e, 0x4FFE4C, (int32_t)(car + 0x58C));
        w32(e, 0x4FFE50, r32(e, car + 0x58C));
        w32(e, 0x4FFE60, (int32_t)(car + 0x728));
        w32(e, 0x4FFE64, (int32_t)(car + 0x734));
        w32(e, 0x4FFE38, (int32_t)(car + 0xDC));
    }
    int32_t n = (int32_t)C0(e, 0x45AFB4);
    int32_t m = (int32_t)C0(e, 0x45B078);
    int32_t my = r32(e, mdl + 0x158);
    w32(e, 0x4FFE54, 0);
    w32(e, 0x4FFE58, 0);
    int32_t best = 400;
    w32(e, 0x4FFE5C, 0);
    w8(e, 0x4FFE35, c->human);
    for (int32_t i = 0; i < n; i++) {
        uint32_t o = C1(e, 0x45AE24, (uint32_t)i);
        int32_t before = best;
        type60_pick(e, c, o, &best, my);
        if (best != before) {
            w32(e, 0x4FFE68, (int32_t)(C2(e, 0x45AFBC, (uint32_t)i, 0x3C) + 0x6E4));
            C2(e, 0x453B10, mdl, 0x3C);
        }
    }
    for (int32_t i = 0; i < m; i++) {
        if (r32(e, C1(e, 0x45B080, (uint32_t)i) + 0x58C) < 1) continue;
        uint32_t o = C1(e, 0x45AE2C, (uint32_t)i);
        int32_t before = best;
        type60_pick(e, c, o, &best, my);
        if (best != before) {
            w32(e, 0x4FFE68, (int32_t)(C2(e, 0x45B080, (uint32_t)i, 0x3C) + 0x728));
            C2(e, 0x453B10, mdl, 0x3C);
        }
    }
    fire_sound(e, c, 0x2D);
}

/* type 0xB0: area blast */
static void typeB0(E* e, const Ctx* c) {
    uint32_t b = c->body, s = e->scratch;
    C3(e, 0x470320, 0x914, 0, 1);
    C2(e, 0x450C08, b + 0x20, 0);
    for (int32_t i = 0; i < (int32_t)C0(e, 0x45AFAC); i++) {
        if (i == c->idx) continue;
        uint32_t o = C2(e, 0x45B028, (uint32_t)i, s + S_BYTE);
        if (!o) continue;
        uint8_t oh = r8(e, s + S_BYTE);
        uint32_t opos, ofl, omdl;
        int32_t oidx;
        if (oh) {
            opos = o + 0x6E4; oidx = r16(e, o + 2); ofl = o + 0x9A;
            omdl = C1(e, 0x45AE24, (uint32_t)(int32_t)r16(e, o));
        } else {
            opos = o + 0x728; oidx = r16(e, o + 6); ofl = o + 0x68;
            omdl = C1(e, 0x45AE2C, (uint32_t)(int32_t)r16(e, o + 4));
        }
        if ((int32_t)C2(e, 0x45C20C, (uint32_t)c->idx, (uint32_t)oidx) >= 0x320) continue;
        if (r8(e, ofl) != 0) continue;
        if (iabs(sub(r32(e, b + 0x28), r32(e, opos + 8))) >= 200) continue;
        C2(e, 0x450984, opos, 0);
        C4(e, 0x421920, o, oh, 7, 1);
        e->call(e->user, 0x4210E4, (uint32_t)r32(e, omdl + 0x158), 0xF, 0, 2, (uint32_t)(int32_t)(int16_t)c->idx, 0);
    }
    if (c->human) {
        uint32_t l = C0(e, 0x470D08);
        for (uint32_t p = l; p != l + 0x3E8; p += 0x14) {
            if (r16(e, p + 4) <= 0) continue;
            if ((dist_to(e, b + 0x20, p + 8) >> 3) < 100)
                C3(e, 0x470D3C, (uint32_t)(int32_t)r16(e, p), (uint32_t)(int32_t)r16(e, p + 2), 0xF);
        }
    }
    uint32_t l = C0(e, 0x471AEC);
    for (int i = 0; i < 6; i++, l += 0x4C) {
        if (r16(e, l + 4) == -1) continue;
        memcpy(P(e, s + S_VEC), P(e, 0x4E6A44), 12);
        if ((dist_to(e, b + 0x20, (uint32_t)r32(e, l + 0x48) + 0xE8) >> 3) < 100) C2(e, 0x4723BC, (uint32_t)i, s + S_VEC);
    }
    fire_sound(e, c, 0x37);
}

/* type 0xD0: toggle the guided object 0x004ffed0 */
static int typeD0(E* e, const Ctx* c) {
    uint32_t obj = (uint32_t)r32(e, 0x4FFED0);
    if (!obj) return 0;
    w8(e, 0x4FFE70, c->human);
    if (r16(e, 0x4FFE74) != 0 && c->human) {
        w16(e, 0x4FFE74, 0);
        w16(e, 0x4FFE76, 0);
        C3(e, 0x470320, 0x915, 0, 0);
        obj = (uint32_t)r32(e, 0x4FFED0);
        w32(e, obj + 8, 0);
        C2(e, 0x450A10, obj + 0xE8, 0);
        C3(e, 0x449F10, (uint32_t)r32(e, 0x4FFED0) + 0xE8, (uint32_t)(int32_t)(int16_t)c->idx, 0);
        return 0;
    }
    w32(e, 0x4FFECC, (int32_t)c->ws);
    w16(e, 0x4FFE74, 1);
    w16(e, 0x4FFE72, c->base);
    w16(e, 0x4FFE76, 0xF0);
    C3(e, 0x470320, 0x915, 0, 1);
    w16(e, c->ws + 0x26, 1);
    w16(e, c->ws + 0xE, r16(e, c->ws + 0xE) + 1);
    obj = (uint32_t)r32(e, 0x4FFED0);
    w32(e, obj + 8, 1);
    w32(e, obj + 0xC, 0);
    uint32_t b = c->body;
    int32_t a0, a1, a2;
    if (r8(e, c->ws + 5)) {
        a0 = (int32_t)(((uint32_t)r32(e, b + 0x2C) + 0x800u) & 0xFFFu);
        a1 = 0;
        a2 = (int32_t)(((uint32_t)r32(e, b + 0x34) + 0x800u) & 0xFFFu);
    } else {
        a0 = r32(e, b + 0x2C); a1 = r32(e, b + 0x30); a2 = r32(e, b + 0x34);
    }
    obj = (uint32_t)r32(e, 0x4FFED0);
    w16(e, obj + 0xE0, a0);
    w16(e, obj + 0xE2, a1);
    w16(e, obj + 0xE4, a2);
    w32(e, 0x4FFE7C, 10);
    memcpy(P(e, obj + 0xE8), P(e, b + 0x20), 12);
    fire_sound(e, c, 0x3B);
    return 0;
}

/* slot 0: the car's own special, by car type */
static void special(E* e, Ctx* c) {
    uint32_t b = c->body, st = c->stats, s = e->scratch, car = c->car;
    int32_t v[3];
    if (r16(e, c->ws + 0xE) < 1) return;
    snd_pos(e, b + 0x20);
    v[0] = 0; v[1] = r16(e, st + 0xD6); v[2] = r16(e, st + 0xD8);
    mtv(e, c, v, s + S_POS);
    w32(e, s + S_POS + 8, add(r32(e, s + S_POS + 8), 0x18));
    /* (a second M^T (0, -100, 0) product is overwritten at once: left out) */
    c->w[0] = tm2_sin(r32(e, b + 0x34)) >> 6;
    c->w[1] = tm2_cos(r32(e, b + 0x34)) >> 6;
    c->w[2] = (int32_t)(0u - (uint32_t)tm2_sin(r32(e, b + 0x2C))) >> 6;
    uint16_t t = c->type;
    int fired = 1;   /* every type but 0x20 ends with flags +0x09 = 1 */
    if (t == 0x10) {
        pick(e, c);
        w32(e, s + S_POS + 8, add(r32(e, s + S_POS + 8), 0x28));
        int16_t r = launch(e, 0x414244, r32(e, c->model + 0x158), 0xF, 0);
        if ((c->human || r8(e, car + 0x32)) && r >= 0) {
            if (c->human && c->id == 0) C4(e, 0x46F8B0, 0x3D, 0, 0, 99);
            else snd_play(e, s + S_POS, 0x3D);
        }
    } else if (t == 0x20) {
        w16(e, c->ws + 0xE, r16(e, c->ws + 0xE) + 1);
        fired = 0;
    } else if (t == 0x30) {
        uint32_t mdl = c->human ? C1(e, 0x45AE24, (uint32_t)(int32_t)r16(e, car))
                                : C1(e, 0x45AE2C, (uint32_t)(int32_t)r16(e, car + 4));
        if (type30(e, c, mdl)) {
            fire_sound(e, c, 0x27);
            if (!c->human && (int8_t)r8(e, car + 1) == 1) w8(e, car + 0x44C, 1);
        }
    } else if (t == 0x40) {
        pick(e, c);
        v[0] = r16(e, st + 0xDC); v[1] = 0; v[2] = r16(e, st + 0x44) + 8;
        mtv(e, c, v, s + S_POS);
        if (launch(e, 0x4146F8, c->base, 6, 0x4E69DC) >= 0) v[0] = r16(e, st + 0xD4);
        v[1] = r16(e, st + 0xD6); v[2] = r16(e, st + 0xD8);
        mtv(e, c, v, s + S_POS);
        if (launch(e, 0x4146F8, c->base, 6, 0x4E69E4) >= 0) v[0] = -r16(e, st + 0xD4);
        v[1] = r16(e, st + 0xD6); v[2] = r16(e, st + 0xD8);
        mtv(e, c, v, s + S_POS);
        if (launch(e, 0x4146F8, c->base, 6, 0x4E69EC) >= 0) fire_sound(e, c, 0x29);
    } else if (t == 0x50) {
        pick(e, c);
        v[0] = 0; v[1] = 0xA0; v[2] = r16(e, st + 0x44) + 8;
        mtv(e, c, v, s + S_POS);
        if (launch(e, 0x414000, c->base, 0x28, 0) >= 0) fire_sound(e, c, 0x2B);
    } else if (t == 0x60) {
        type60(e, c);
    } else if (t == 0x70) {
        uint32_t mdl = c->human ? C1(e, 0x45AE24, (uint32_t)(int32_t)r16(e, car))
                                : C1(e, 0x45AE2C, (uint32_t)(int32_t)r16(e, car + 4));
        int16_t k;
        for (k = 0; k < 2; k++)
            if (r16(e, 0x4FF7B8 + 0x20u * (uint32_t)k) < 1) break;
        if (k == 2) k = -1;
        if (k >= 0) {
            uint32_t r = 0x4FF7B8 + 0x20u * (uint32_t)k;
            w32(e, r + 0x1C, (int32_t)mdl);
            for (int i = 0; i < 3; i++) w32(e, r + 4 + 4 * i, c->w[i]);
            w16(e, r, 0xB4);
            for (int i = 0; i < 3; i++) w32(e, r + 0x10 + 4 * i, add(r32(e, mdl + 0xE8 + 4 * i), c->w[i]));
            if (c->human && c->id == 0) C4(e, 0x46F8B0, 0x2F, 0, 0, 99);
            else snd_play(e, r + 0x10, 0x2F);
            w16(e, r + 2, 0);
        }
    } else if (t == 0x80 || t == 0x90) {
        pick(e, c);
        v[0] = r16(e, st + 0xDC); v[1] = 0; v[2] = r16(e, st + 0x44) + 8;
        mtv(e, c, v, s + S_POS);
        int16_t r = t == 0x80 ? launch(e, 0x414328, c->base, 0xC, 0) : launch(e, 0x413F7C, c->base, 10, c->model);
        if (r >= 0) fire_sound(e, c, t == 0x80 ? 0x31 : 0x33);
    } else if (t == 0xA0) {
        if (r16(e, 0x4FFE20) > 0) {
            w16(e, c->ws + 0x26, 1);
            w16(e, c->ws + 0xE, r16(e, c->ws + 0xE) + 1);
        } else {
            w32(e, c->model + 8, 0);
            if (r8(e, c->ws + 4) == 0) w32(e, (uint32_t)r32(e, 0x4FFE1C) + 8, 1);
            w16(e, 0x4FFE24, 2);
            w32(e, 0x4FFE2C, (int32_t)c->flags);
            w32(e, 0x4FFE30, (int32_t)c->model);
            w32(e, 0x4FFE28, (int32_t)(st + 0xA0));
            uint32_t o = (uint32_t)r32(e, 0x4FFE1C);
            memmove(P(e, o + 0xE8), P(e, c->model + 0xE8), 12);
            w16(e, 0x4FFE20, 300);
            memmove(P(e, o + 0xE0), P(e, c->model + 0xE0), 8);
            C1(e, 0x453C38, 0xA0);
            fire_sound(e, c, 0x35);
            if (!c->human && (int8_t)r8(e, car + 1) == 1) w8(e, car + 0x44C, 1);
        }
    } else if (t == 0xB0) {
        typeB0(e, c);
    } else if (t == 0xC0) {
        uint32_t o = (uint32_t)r32(e, c->ws + 0x48);
        if (o && r8(e, o) == 0) {
            w8(e, o, 0); w8(e, o + 1, 1); w8(e, o + 2, 0); w8(e, o + 7, 0); w8(e, o + 3, 1);
            w16(e, o + 0x10, 0); w8(e, o + 0xB, 0);
            w8(e, o + 0xB, 0);
            w8(e, o, 1);
            if (!c->human) w8(e, car + 0x44C, 1);
        }
    } else if (t == 0xD0) {
        typeD0(e, c);
    } else if (t == 0xE0) {
        pick(e, c);
        v[0] = r16(e, st + 0xDC); v[1] = 0; v[2] = r16(e, st + 0x44) + 8;
        mtv(e, c, v, s + S_POS);
        launch(e, 0x4146F8, c->base, 10, 0x4E69F4);
        v[0] = r16(e, st + 0xD4); v[1] = r16(e, st + 0xD6); v[2] = r16(e, st + 0xD8);
        mtv(e, c, v, s + S_POS);
        if (launch(e, 0x4146F8, c->base, 10, 0x4E69FC) >= 0) v[0] = -r16(e, st + 0xD4);
        v[1] = r16(e, st + 0xD6); v[2] = r16(e, st + 0xD8);
        mtv(e, c, v, s + S_POS);
        if (launch(e, 0x4146F8, c->base, 10, 0x4E69DC) >= 0) w32(e, s + S_POS + 8, add(r32(e, s + S_POS + 8), 200));
        launch(e, 0x414114, r32(e, c->model + 0x158), 7, 0);
        if (c->human && c->id == 0) C4(e, 0x46F8B0, 0xD, 0, 0, 99);
        else if ((!c->human && r8(e, car + 0x32)) || c->id != 0) snd_play(e, s + S_POS, 0xD);
    }
    if (fired) w8(e, c->flags + 9, 1);
}

/* slot 7: the arena special (event types 1 / 8: centre radius; 2: zones) */
static void link_set(E* e, int slot, int32_t id, uint32_t tpos, uint32_t from) {
    uint32_t le = LE(slot);
    w16(e, le + 4, 0x78);
    if (id < 0) { w32(e, le + 0x10, 0); w16(e, le + 8, -1); }
    else { w16(e, le + 8, id); w32(e, le + 0x10, (int32_t)tpos); }
    w32(e, le + 0xC, (int32_t)from);
    w16(e, le + 6, 1);
}
static void link_last(E* e, uint32_t from) {
    int slot = link_find(e, -1);
    if (slot >= 0) link_set(e, slot, -1, 0, from);
}

static int slot7_radius(E* e, int32_t n, int32_t m) {
    int32_t lim;
    if ((uint8_t)C0(e, 0x474508) == 1) {
        w32(e, 0x4FEF5C, -0x1200); w32(e, 0x4FEF60, -0x890); w32(e, 0x4FEF64, 800);
        lim = 0xFA;
    } else {
        w32(e, 0x4FEF5C, 0); w32(e, 0x4FEF60, 0); w32(e, 0x4FEF64, 0xA00);
        lim = 0x15E;
    }
    for (int32_t i = 0; i < n; i++) {
        uint32_t mdl = C1(e, 0x45AE24, (uint32_t)i);
        int32_t id = r32(e, mdl + 0x158);
        w32(e, 0x4FEF58, (int32_t)(C1(e, 0x45AFBC, (uint32_t)i) + 0x6E4));
        if ((dist_to(e, 0x4FEF5C, (uint32_t)r32(e, 0x4FEF58)) >> 3) >= lim) continue;
        int slot = link_find(e, i);
        if (slot < 0) continue;
        link_set(e, slot, id, (uint32_t)r32(e, 0x4FEF58), 0x4FEF5C);
        C2(e, 0x45133C, mdl + 0xE8, 1);
        C4(e, 0x421920, C1(e, 0x45AFBC, (uint32_t)i), 1, 6, 0);
        C2(e, 0x450984, (uint32_t)r32(e, 0x4FEF58), 0);
    }
    for (int32_t i = 0; i < m; i++) {
        if (r32(e, C1(e, 0x45B080, (uint32_t)i) + 0x58C) < 1) continue;
        uint32_t mdl = C1(e, 0x45AE2C, (uint32_t)i);
        int32_t id = r32(e, mdl + 0x158);
        w32(e, 0x4FEF58, (int32_t)(C1(e, 0x45B080, (uint32_t)i) + 0x728));
        if ((dist_to(e, 0x4FEF5C, (uint32_t)r32(e, 0x4FEF58)) >> 3) >= lim) continue;
        int slot = link_find(e, 100 + i);
        if (slot < 0) continue;
        link_set(e, slot, id, (uint32_t)r32(e, 0x4FEF58), 0x4FEF5C);
        C2(e, 0x45133C, mdl + 0xE8, 1);
        C4(e, 0x421920, C1(e, 0x45B080, (uint32_t)i), 0, 6, 0);
        C2(e, 0x450984, (uint32_t)r32(e, 0x4FEF58), 0);
    }
    link_last(e, 0x4FEF5C);
    return 1;
}

static int slot7_zones(E* e, int32_t n, int32_t m) {
    for (int32_t i = 0; i < n; i++) {
        uint32_t mdl = C1(e, 0x45AE24, (uint32_t)i);
        int32_t id = r32(e, mdl + 0x158);
        w32(e, 0x4FEF58, (int32_t)(C1(e, 0x45AFBC, (uint32_t)i) + 0x6E4));
        int32_t z = (int32_t)C3(e, 0x42CBA8, (uint32_t)r32(e, mdl + 0xE8), (uint32_t)r32(e, mdl + 0xEC), (uint32_t)r32(e, mdl + 0xF0));
        for (uint32_t j = 1; j < 5; j++) {
            if (r16(e, C1(e, 0x410878, j) + 0xC) != z) continue;
            int slot = link_find(e, i);
            if (slot < 0) continue;
            link_set(e, slot, id, (uint32_t)r32(e, 0x4FEF58), 0x4E6A04);
            C2(e, 0x45133C, mdl + 0xE8, 1);
        }
    }
    for (int32_t i = 0; i < m; i++) {
        if (r32(e, C1(e, 0x45B080, (uint32_t)i) + 0x58C) < 1) continue;
        uint32_t mdl = C1(e, 0x45AE2C, (uint32_t)i);
        int32_t id = r32(e, mdl + 0x158);
        w32(e, 0x4FEF58, (int32_t)(C1(e, 0x45B080, (uint32_t)i) + 0x728));
        int32_t z = (int32_t)C3(e, 0x42CBA8, (uint32_t)r32(e, mdl + 0xE8), (uint32_t)r32(e, mdl + 0xEC), (uint32_t)r32(e, mdl + 0xF0));
        for (uint32_t j = 1; j < 5; j++) {
            if (r16(e, C1(e, 0x410878, j) + 0xC) != z) continue;
            int slot = link_find(e, 100 + i);
            if (slot < 0) continue;
            link_set(e, slot, id, (uint32_t)r32(e, 0x4FEF58), 0x4E6A04);
            C2(e, 0x45133C, mdl + 0xE8, 1);
            C4(e, 0x421920, C1(e, 0x45B080, (uint32_t)i), 0, 6, 0);
            C2(e, 0x450984, (uint32_t)r32(e, 0x4FEF58), 0);
        }
    }
    link_last(e, 0x4E6A04);
    return 1;
}

/* returns 1 when the original returns at once (no tail) */
static int slot7(E* e, Ctx* c) {
    if ((uint8_t)C0(e, 0x474508) == 2 && C0(e, 0x411DBC) == 1 && c->human) {
        C1(e, 0x46FCA0, 1);
        return 1;
    }
    w8(e, c->ws + 5, 0);
    if ((uint8_t)C0(e, 0x474508) != 1 && (uint8_t)C0(e, 0x474508) != 2 && (uint8_t)C0(e, 0x474508) != 8) {
        if (c->human) { C1(e, 0x46FCA0, 1); return 1; }
        return 0;
    }
    if (c->human) C1(e, 0x45AE24, (uint32_t)(int32_t)r16(e, c->car));
    else C1(e, 0x45AE2C, (uint32_t)(int32_t)r16(e, c->car + 4));
    int32_t n = (int32_t)C0(e, 0x45AFB4);
    int32_t m = (int32_t)C0(e, 0x45B078);
    uint8_t ev = (uint8_t)C0(e, 0x474508);
    int ok;
    if (ev < 2) ok = ev == 1 ? slot7_radius(e, n, m) : 0;
    else if (ev == 2) ok = slot7_zones(e, n, m);
    else ok = ev == 8 ? slot7_radius(e, n, m) : 0;
    if (ok) {
        C4(e, 0x415BC4, 5, 0xFA, 200, 200);
        snd_pos(e, c->body + 0x20);
        uint32_t b = C0(e, 0x46F990);
        C4(e, 0x46F8B0, 0x40, 0, b, 99);
    }
    return 0;
}

/* slot 6: drop a mine (first free of the 10 objects at 0x004ff8a8) */
static void slot6(E* e, Ctx* c, uint32_t model0) {
    int k;
    for (k = 0; k < 10; k++)
        if (r32(e, (uint32_t)r32(e, 0x4FF8A8 + 4u * (uint32_t)k) + 8) == 0) break;
    if (k == 10) return;
    uint32_t rec = 0x4FF8A8 + 0x30u * (uint32_t)k;
    uint32_t obj = (uint32_t)r32(e, 0x4FF8A8 + 4u * (uint32_t)k);
    w16(e, rec + 0x2A, r16(e, model0 + 0x158));
    w32(e, obj + 0x15C, r32(e, c->type == 0xF0 ? 0x4FFAB0 : 0x4FFAB4));
    w32(e, obj + 8, 1);
    w32(e, obj + 0xC, 0);
    uint32_t b = c->body;
    int16_t a0 = r16(e, b + 0x2C), a2 = r16(e, b + 0x34);
    if (r8(e, c->ws + 5)) a2 = (int16_t)(((uint16_t)a2 + 0x800u) & 0xFFFu);
    w16(e, obj + 0xE0, a0);
    w16(e, obj + 0xE2, 0);
    w16(e, obj + 0xE4, a2);
    w16(e, obj + 0xE6, (int16_t)e->stale_e6);
    memcpy(P(e, obj + 0xE8), P(e, b + 0x20), 12);
    w32(e, obj + 0xF0, add(r32(e, obj + 0xF0), 0x28));
    w16(e, rec + 0x2E, 0);
    w32(e, rec + 0x34, 100);
    w32(e, rec + 0x54, (int32_t)obj);
    w16(e, rec + 0x2C, 0x4B0);
    snd_play(e, b + 0x20, 0x12);
}

void gp3_weapon_fire_special(const Gp3WfsEnv* e, uint32_t car, uint8_t human) {
    uint32_t input, ws, flags, body, model0;
    int32_t id, idx, cooldown;
    uint16_t type;
    if (human) {
        body = car + 0x6C4; input = car + 0xCC; ws = car + 0x848; flags = car + 0x9A;
        cooldown = 0x3C;
        id = r16(e, car);
        model0 = C1(e, 0x45AE24, (uint32_t)id);
        idx = r16(e, car + 2);
        type = (uint16_t)r16(e, car + 0x90);
    } else {
        body = car + 0x708; input = car + 0x40; ws = car + 0x834; flags = car + 0x68;
        id = r16(e, car + 4);
        model0 = C1(e, 0x45AE2C, (uint32_t)id);
        type = (uint16_t)r16(e, car + 0x30);
        idx = r16(e, car + 6);
        cooldown = r32(e, 0x4FFED8);
    }
    int32_t slot = (int8_t)r8(e, ws + 0xD);
    if (!human && (uint8_t)C1(e, 0x45B3FC, (uint32_t)(int32_t)(int16_t)type)) {
        uint32_t p = C2(e, 0x45AFC8, (uint32_t)(int32_t)r16(e, car + 4), e->scratch + S_BYTE);
        int32_t other = r8(e, e->scratch + S_BYTE) ? r16(e, p + 2) : r16(e, p + 6);
        if ((int32_t)C2(e, 0x45C20C, (uint32_t)idx, (uint32_t)other) < 400) {
            int32_t a = (int16_t)C2(e, 0x45C1F4, (uint32_t)idx, (uint32_t)other);
            if (iabs(sub(r32(e, body + 0x34), a)) < 0xE3) {
                slot = 0;
                if (r16(e, ws + 0x26) < cooldown - 10) {
                    w16(e, ws + 0x26, 0);
                    w8(e, input + 0x1B, 1);
                    w8(e, input + 0x1E, 1);
                    if (r16(e, ws + 0xE) < 1) w16(e, ws + 0xE, 1);
                }
            }
        }
    }
    if (r16(e, ws + 0x26) > 0) {
        w16(e, ws + 0x26, r16(e, ws + 0x26) - (int16_t)C0(e, 0x4931D4));
        return;
    }
    if (r8(e, input + 0x1B) == 0 || !human) w8(e, input + 0x1E, 1);
    if (human && (slot == 8 || r16(e, ws + 0xE + 2 * slot) <= 0)) {
        C3(e, 0x4469BC, ws, 0xFFFFFFFFu, 8);
        if (slot == 8 && r8(e, input + 0x1E) && r8(e, input + 0x1B) && r8(e, flags + 0xD) == 0) {
            C1(e, 0x46FCA0, 1);
            w8(e, input + 0x1E, 0);
            w8(e, ws + 5, 0);
        }
        return;
    }
    if (r8(e, flags + 0xD) || !r8(e, input + 0x1B) || !r8(e, input + 0x1E)) {
        w8(e, input + 0x1B, 0);
        return;
    }
    w16(e, ws + 0x26, cooldown);
    Ctx c;
    c.car = car; c.ws = ws; c.flags = flags; c.human = human;
    c.body = body;
    switch ((uint32_t)slot) {
    case 0:
        if (human) {
            c.stats = car + 0x138;
            c.base = (int16_t)(r16(e, car) + 1);
            c.model = C1(e, 0x45AE24, (uint32_t)(int32_t)r16(e, car));
            c.type = (uint16_t)r16(e, car + 0x90);
            c.id = r16(e, car);
            c.idx = r16(e, car + 2);
        } else {
            c.stats = car + 0x4EC;
            c.base = (int16_t)(r16(e, car + 4) + 0x32);
            c.model = C1(e, 0x45AE2C, (uint32_t)(int32_t)r16(e, car + 4));
            c.type = (uint16_t)r16(e, car + 0x30);
            c.id = r16(e, car + 4);
            c.idx = r16(e, car + 6);
        }
        special(e, &c);
        break;
    case 1: case 2: case 4: case 5:
        C3(e, 0x444004, car, (uint32_t)slot, human);
        break;
    case 3:
        if (r8(e, ws + 6) == 0) {
            w8(e, ws + 6, 1);
            w16(e, ws + 0x26, 1);
            w16(e, ws + 0xE + 2 * slot, r16(e, ws + 0xE + 2 * slot) + 1);
            w16(e, ws + 0x22, r16(e, ws + 0x22) + 1);
            snd_play(e, body + 0x20, 0x13);
        } else {
            w8(e, ws + 6, 0);
        }
        C3(e, 0x448118, car, (uint32_t)(int32_t)(int8_t)r8(e, ws + 0xD), human);
        w8(e, ws + 5, 0);
        break;
    case 6:
        c.type = type;
        slot6(e, &c, model0);
        break;
    case 7:
        c.id = id;
        if (slot7(e, &c)) return;
        break;
    default:
        break;
    }
    w8(e, input + 0x1B, 0);
    w8(e, input + 0x1E, 0);
    w16(e, ws + 0xE + 2 * slot, r16(e, ws + 0xE + 2 * slot) - 1);
    if (slot != 0) w16(e, ws + 0x22, r16(e, ws + 0x22) - 1);
    if (human && id == 0) {
        if (r8(e, ws + 5)) C2(e, 0x473568, 0xC, r8(e, ws + 0xD));
        C2(e, 0x473568, 8, r8(e, ws + 0xD));
    }
    w8(e, ws + 5, 0);
}
