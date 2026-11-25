/*
 * gp7_car.c: see gp7_car.h. The control flow follows the machine code; sar16(a) is the
 * original's "mov r, dword [a]; sar r, 16" (the i16 at a + 2).
 */
#include "gp7_car.h"

#include "camera.h"

#define M(a) e->mem(e->user, (a))
static uint32_t r32(En2Env* e, uint32_t a) { uint32_t v; memcpy(&v, M(a), 4); return v; }
static uint16_t r16(En2Env* e, uint32_t a) { uint16_t v; memcpy(&v, M(a), 2); return v; }
static uint8_t r8(En2Env* e, uint32_t a) { return *M(a); }
static void w32(En2Env* e, uint32_t a, uint32_t v) { memcpy(M(a), &v, 4); }
static void w16(En2Env* e, uint32_t a, uint32_t v) { uint16_t h = (uint16_t)v; memcpy(M(a), &h, 2); }
static void w8(En2Env* e, uint32_t a, uint32_t v) { *M(a) = (uint8_t)v; }

#define R32(a) r32(e, (a))
#define R16(a) r16(e, (a))
#define R8(a) r8(e, (a))
#define W32(a, v) w32(e, (a), (uint32_t)(v))
#define W16(a, v) w16(e, (a), (uint32_t)(v))
#define W8(a, v) w8(e, (a), (uint32_t)(v))
#define S16(a) ((int32_t)(int16_t)r16(e, (a)))
#define SAR16(a) ((int32_t)r32(e, (a)) >> 16)

static uint32_t call(En2Env* e, uint32_t fn, int n, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
    uint32_t v[5] = {a0, a1, a2, a3, a4};
    return e->call(e->user, fn, v, n);
}
#define C0(fn) call(e, (fn), 0, 0, 0, 0, 0, 0)
#define C1(fn, a) call(e, (fn), 1, (a), 0, 0, 0, 0)
#define C2(fn, a, b) call(e, (fn), 2, (a), (b), 0, 0, 0)
#define C3(fn, a, b, c) call(e, (fn), 3, (a), (b), (c), 0, 0)
#define C5(fn, a, b, c, d, f) call(e, (fn), 5, (a), (b), (c), (d), (f))

/* ---- 0x00429fd0 ---- */

void gp7_clear_controls(En2Env* e, uint32_t p)
{
    static const uint8_t zero[] = {0xe, 0x10, 0xf, 0x13, 0x12, 0, 1, 3};
    static const uint8_t zero2[] = {0x17, 0x18, 0x1b, 0x1c, 0x1d, 0x1f, 4, 5, 0xd};
    for (unsigned i = 0; i < sizeof zero; i++) W8(p + zero[i], 0);
    W8(p + 0x1a, 1);
    for (unsigned i = 0; i < sizeof zero2; i++) W8(p + zero2[i], 0);
}

/* ---- 0x00437850 ---- */

static void round_reset(En2Env* e)
{
    C0(0x00448098u);
    C0(0x00447b70u);
    C0(0x00412d58u);
    C0(0x00451458u);
}

uint32_t gp7_player_car_setup(En2Env* e, uint32_t car, uint32_t edx, uint32_t ebx)
{
    W8(car + 0x138, 1);
    W16(car + 0x90, edx);
    C1(0x00437ac4u, car);
    if ((uint8_t)ebx) {
        C3(0x0043807cu, car, car + 0x848, 1);
    } else {
        W16(car + 0x878, 0xbb8);
        W16(car + 0x856, 2);
        W8(car + 0x84e, 0);
        if (R16(car + 0x90) == 0xc0) C1(0x0044c81cu, car + 0x848);
    }
    if (C0(0x0045c338u) == 7) {
        if (R32(0x004fb170u) == 0) {
            W32(0x004fb170u, 1);
            round_reset(e);
        }
        C2(0x00470940u, (uint32_t)SAR16(car + 0x8e), 0);
        C3(0x00470320u, (uint32_t)(SAR16(car + 0x8e) + 6), 0, 0);
    } else {
        W32(0x004fb170u, 0);
        round_reset(e);
    }
    W8(car + 0x84c, 0);
    W16(car + 0x87c, 0);
    C2(0x0044ef60u, car + 0x7f0, (uint32_t)S16(car));
    car_camera_init(M(car));   /* 0x0043793f..0x00437a31, verified on its own */
    W16(car + 0xc6, 0);
    C5(0x00420be8u, car, 2, 2, 0, 1);
    W32(car + 0x8c, 0);
    C1(0x00429fd0u, car + 0xcc);
    uint32_t v = C1(0x0045b3f0u, (uint32_t)S16(car));
    C2(0x0045b3f8u, car, v);   /* a lone ret: (car, value) as the original sets them */
    W8(car + 0x141, 0);
    W8(car + 0x142, 0);
    W8(car + 0x143, 0);
    W8(car + 0x144, 0);
    W8(car + 0x139, 0);
    W8(car + 0x21d, 1);
    uint32_t obj = C1(0x0045ae24u, (uint32_t)S16(car));
    if (obj) {
        uint32_t r = C1(0x00495864u, 0);
        W32(obj + 8, 1);
        W32(obj + 4, r);
    }
    return C0(0x00436010u);
}

/* ---- 0x0042a020 from 0x0042a4e7 ---- */

#define SURFACE_KIND 0x004fdc76u

static void drop_entity(En2Env* e, uint32_t car, int p, uint32_t ent)
{
    W32(ent + 8, 0);
    W32(car + (p ? 0xc8u : 0x94u), 0);
}

void gp7_car_terrain_surface(En2Env* e, uint32_t car, int player)
{
    int p = (uint8_t)player != 0;
    uint32_t body, stats, ctl, probes, turbo, obj, ent;
    uint16_t id;
    uint8_t f2c;
    if (p) {
        body = car + 0x6c4; stats = car + 0x138; ctl = car + 0x9a; probes = car + 0x58c; turbo = car + 0x848;
        id = R16(car + 0x90);
        f2c = R8(stats + 0xa4);
        obj = C1(0x0045ae24u, (uint32_t)S16(car));
        ent = R32(car + 0xc8);
    } else {
        body = car + 0x708; stats = car + 0x4ec; ctl = car + 0x68; probes = car + 0x5d0; turbo = car + 0x834;
        id = R16(car + 0x30);
        obj = C1(0x0045ae2cu, (uint32_t)SAR16(car + 2));
        f2c = id == 0xe0 ? 0 : R8(car + 0x38);
        ent = R32(car + 0x94);
    }
    uint32_t sid = (uint32_t)(int32_t)(int16_t)id;
    if ((uint8_t)C0(0x00474508u) == 5) {
        if (SAR16(probes + 0x26) == 7 || SAR16(probes + 0xfe) == 7) {
            if (R8(ctl + 0x23) == 0) {
                W8(ctl + 0x23, 1);
                C2(0x00457138u, body + 0x20, (uint32_t)SAR16(body + 8));
            }
            uint32_t r = C1(0x00495864u, 2);
            W32(obj + 4, r);
            W16(stats + 0x22, 0x12c);
        } else if (R8(ctl + 0x23) != 0) {
            W8(ctl + 0x23, 0);
            uint32_t r = C1(0x00495864u, 0);
            W32(obj + 4, r);
        }
    }
    if (R8(ctl + 0xa) != 0 || R16(probes + 0x70) != R16(probes + 0x28) || R16(probes + 0xb8) != R16(probes + 0x28) ||
        R16(probes + 0x100) != R16(probes + 0x28)) {
        /* 0x0042aa0e: off the ground or on mixed tiles */
        if (R8(ctl + 0x24)) C1(0x00453c00u, sid);
        W8(ctl + 0x24, 0);
        W8(ctl + 0x22, 0);
        W8(ctl + 0x21, 0);
        if (ent) drop_entity(e, car, p, ent);
        return;
    }
    uint8_t t = R8(SURFACE_KIND + (uint32_t)SAR16(probes + 0x26));
    if (t == 4) {
        uint8_t t1 = R8(SURFACE_KIND + (uint32_t)SAR16(probes + 0x6e));
        if (t1 != 4) {
            t = t1;
        } else {
            uint8_t t2 = R8(SURFACE_KIND + (uint32_t)SAR16(probes + 0xb6));
            if (t2 != 4) {
                t = t2;
            } else {
                uint8_t t3 = R8(SURFACE_KIND + (uint32_t)SAR16(probes + 0xfe));
                if (t3 != 4) t = t3;
            }
        }
    }
    if (t != 3 && t != 4) {
        W8(ctl + 0x21, 0);
        if (ent) {
            drop_entity(e, car, p, ent);
            ent = 0;
        }
    }
    if (t != 4) W8(ctl + 0x24, 0);
    if (t != 2) W8(ctl + 0x22, 0);
    if (t == 2) {
        if (SAR16(probes + 0x3e) <= 7) {
            W8(stats + 6, 0);
            return;
        }
        if (R8(ctl + 0x22) == 0) {
            if (R8(ctl + 0xc) == 0) {
                C1(0x0045723cu, body + 0x20);
                W8(stats + 6, 0);   /* dl, still the 0 just tested at ctl +0x22 */
            }
            W8(ctl + 0x22, 1);
            W16(stats + 0x22, 0);
        } else if (R8(ctl + 0xe) == 0) {
            int32_t a = SAR16(body + 8);
            if (a < 0) a = -a;
            C2(0x0042b02cu, probes, a < (int32_t)R32(stats + 0xb8) ? 1u : 2u);
        }
        uint32_t tk = C0(0x004931d4u);
        W8(stats + 6, R8(stats + 6) + (uint8_t)tk);
        if ((int8_t)R8(stats + 6) > 0x3c) {
            W8(stats + 6, 0);
            if (f2c && R8(ctl + 0xe) == 0) W32(stats + 0xa0, R32(stats + 0xa0) - 3);
        }
        return;
    }
    if (t != 3 && t != 4) return;
    if (R8(ctl + 0x21) == 0) {
        if (R8(ctl + 0xc) == 0) {
            C2(0x00457138u, body + 0x20, (uint32_t)SAR16(body + 8));
            W8(ctl + 0x21, 1);
            W8(stats + 6, 0);
            uint32_t r = C0(0x004573f8u);
            if (p) {
                if (r) W32(car + 0xc8, r);
            } else {
                W32(car + 0x94, r);
            }
        }
    } else if (ent) {
        W32(ent + 0xe8, R32(body + 0x20));
        W32(ent + 0xec, R32(body + 0x24));
        uint32_t z = R32(body + 0x28);
        W16(ent + 0xe0, 0);
        W16(ent + 0xe2, 0);
        W32(ent + 0xf0, z - 0x10);
        W16(ent + 0xe4, R16(body + 0x34));
        C2(0x0048c360u, ent + 0xe0, ent + 0xb0);
        W32(ent + 8, 1);
        if ((int32_t)R32(stats + 0xa0) <= 0) drop_entity(e, car, p, ent);
    }
    /* 0x0042a8b5 */
    if ((R8(ctl + 0x1a) != 0 || SAR16(stats + 0x20) > 0) && SAR16(probes + 0x3e) > 0 && SAR16(probes + 0x116) > 0) {
        W8(ctl + 0x1a, 0);
        uint32_t o = R32(turbo + 0x44);
        if (o) W16(o + 0xc, 0);
    }
    if (t == 4 && R8(ctl + 0x24) == 0) {
        W8(ctl + 0x24, 1);
        W8(ctl + 0x1f, 1);
        if (C0(0x004899c2u) & 1) {
            W16(probes + 0x76, 0);
            W16(probes + 0x2e, 0xffff);
        } else {
            W16(probes + 0x76, 0xffff);
            W16(probes + 0x2e, 0);
        }
        W16(probes + 0xbe, 0);
        W16(probes + 0x106, 0);
        C1(0x00453c38u, sid);
    }
    if (R8(ctl + 0x24) == 0) return;
    W8(ctl + 0xd, 1);
    int32_t tk = (int16_t)C0(0x004931d4u);
    W32(stats + 0xa0, R32(stats + 0xa0) - (uint32_t)tk);
    W16(body + 0xa, (int16_t)(S16(body + 0xa) / 2));
    W16(body, (int16_t)(S16(body) / 2));
    W16(probes + 0x2e, R16(probes + 0x2e) - 4);
    W16(probes + 0x76, R16(probes + 0x76) - 4);
    W16(probes + 0x106, R16(probes + 0x106) - 2);
    W16(probes + 0xbe, R16(probes + 0xbe) - 2);
    int16_t a = (int16_t)R16(probes + 0x2e), b = (int16_t)R16(probes + 0x76);
    if (a < b)
        W16(probes + 0x2e, a - 1);
    else
        W16(probes + 0x76, b - 1);
    C2(0x0042b02cu, probes, (C0(0x004899c2u) & 1) ? 2u : 0u);
}
