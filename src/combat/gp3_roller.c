/*
 * gp3_roller.c: the rolling ground weapon slots update, TM2.EXE 0x0044ad14 (see gp3_roller.h).
 */
#include "gp3_roller.h"

#include <string.h>

#include "../math/tm2math.h"

static int16_t rd16(const uint8_t* p) { int16_t v; memcpy(&v, p, 2); return v; }
static int32_t rd32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static uint32_t rdu(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void wr16(uint8_t* p, int16_t v) { memcpy(p, &v, 2); }
static void wr32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }
static void add32(uint8_t* p, int32_t v) { wr32(p, (int32_t)((uint32_t)rd32(p) + (uint32_t)v)); }

static uint32_t call(const Gp3RollerEnv* e, uint32_t addr, int n, uint32_t a0, uint32_t a1, uint32_t a2,
                     uint32_t a3, uint32_t a4, uint32_t a5, void* local) {
    uint32_t a[6] = {a0, a1, a2, a3, a4, a5};
    return e->call(e->user, addr, n, a, local);
}

static int32_t iabs(int32_t v) { return (int32_t)(((uint32_t)v ^ (uint32_t)(v >> 31)) - (uint32_t)(v >> 31)); }

/* max(|dx|, |dy|, |dz|) between positions a and b, signed compares */
static int32_t cheb(const uint8_t* a, const uint8_t* b) {
    int32_t ax = iabs((int32_t)((uint32_t)rd32(a) - (uint32_t)rd32(b)));
    int32_t ay = iabs((int32_t)((uint32_t)rd32(a + 4) - (uint32_t)rd32(b + 4)));
    int32_t az = iabs((int32_t)((uint32_t)rd32(a + 8) - (uint32_t)rd32(b + 8)));
    int32_t m = ay < az ? az : ay;
    return ax < m ? m : ax;
}

#define OBJH() rdu(s + 0x2c)
#define OBJ() e->mem(e->user, OBJH())

static void kill(const Gp3RollerEnv* e, uint8_t* s) {
    wr32(OBJ() + 8, 0);
    add32(OBJ() + 0xf0, 0x50);
    call(e, 0x4508f4, 2, OBJH() + 0xe8, 0, 0, 0, 0, 0, 0);
}

static void query(const Gp3RollerEnv* e, uint8_t* s, uint32_t sa, uint8_t* res) {
    call(e, 0x49cec0, 5, OBJH(), 0, 1, OBJH() + 0xb0, sa + 0x20, 0, res);
}

/* a car within reach: the hit */
static void hit_car(const Gp3RollerEnv* e, uint8_t* s, uint32_t sa, uint32_t car_h, uint32_t model, int human) {
    wr16(s + 4, 0);
    kill(e, s);
    call(e, 0x415d1c, 6, rdu(e->mem(e->user, model) + 0x158), 0, 0, (uint32_t)(int32_t)(int8_t)s[0], sa + 0x14,
         (uint32_t)(int32_t)rd16(s + 2), 0);
    call(e, 0x421920, 4, car_h, human ? 1 : 0, 4, 1, 0, 0, 0);
    uint8_t* car = e->mem(e->user, car_h);
    int16_t a = (int16_t)(rd16(OBJ() + 0xe4) - rd16(car + (human ? 0x6f8 : 0x73c)));
    tm2_angle_wrap16(&a);
    int32_t t = human ? a : iabs(a);
    if (t > 0x200 && t < 0x600) car[human ? 0xb8 : 0x86] = 1;
    int32_t v[3];
    call(e, 0x48b5a0, 2, sa + 0x14, 0, 0, 0, 0, 0, v);
    call(e, 0x421800, 4, car_h, human ? 1 : 0, 0, 800, 0, 0, v);
}

static void slot_update(const Gp3RollerEnv* e, uint8_t* s, uint32_t sa, int32_t ncars, int32_t nai) {
    wr16(s + 4, (int16_t)(rd16(s + 4) - tm2_frame_ticks60()));
    s[0] = (uint8_t)((0x4b0 - rd16(s + 4)) * 0x1e / 0x4b0);
    if ((int8_t)s[0] <= 0xf) s[0] = 0xf;
    {
        int16_t ang[3] = {0, 0, rd16(OBJ() + 0xe4)};
        int32_t m[9], v[3], w[3];
        tm2_build_matrix(ang, m);
        memcpy(OBJ() + 0xb0, m, sizeof m);
        memcpy(m, OBJ() + 0xb0, sizeof m);
        memcpy(v, s + 8, sizeof v);
        tm2_mat_vec_mul_row(m, v, w);
        memcpy(s + 0x14, w, sizeof w);
    }
    memmove(s + 0x20, OBJ() + 0xe8, 12);
    add32(OBJ() + 0xe8, rd32(s + 0x14));
    add32(OBJ() + 0xec, rd32(s + 0x18));
    add32(OBJ() + 0xf0, rd32(s + 0x1c));

    uint8_t res[0x24];
    memset(res, 0, sizeof res);
    query(e, s, sa, res);
    int32_t type = rd16(res + 0x22);
    if (type == 0x29b || type == 0x29c || (type >= 0x191 && type <= 0x1b8)) wr32(res + 4, 0);
    if (rd32(res + 4) != 0 && rd16(s + 2) < 0x32) {
        type = rd16(res + 0x22);
        if (type >= 0x1f4 && type <= 0x208) {
            call(e, 0x470d3c, 3, (uint32_t)type, (uint32_t)(int32_t)rd16(res + 0x20), (uint32_t)(int32_t)(int8_t)s[0],
                 0, 0, 0, 0);
            wr16(s + 4, 0);
            kill(e, s);
        }
    }
    if (rd32(res + 4) != 0) {
        int32_t nx = rd32(res + 8) / 0x29, ny = rd32(res + 0xc) / 0x29, nz = rd32(res + 0x10) / 0x29;
        int32_t vx = rd32(s + 0x14), vy = rd32(s + 0x18), vz = rd32(s + 0x1c);
        int32_t dot = tm2_wadd(tm2_wadd(tm2_wmul(vy, ny), tm2_wmul(vx, nx)), tm2_wmul(vz, nz));
        int32_t rx = tm2_wmul(nx, dot) / 10000, ry = tm2_wmul(ny, dot) / 10000;
        int32_t fx = (int32_t)((uint32_t)vx - 2u * (uint32_t)rx);
        int32_t fy = (int32_t)((uint32_t)vy - 2u * (uint32_t)ry);
        (void)nz;
        uint8_t* obj = OBJ();
        wr16(obj + 0xe4, (int16_t)tm2_atan2(fx, fy ? fy : 1));
        wr16(s + 6, (int16_t)(rd16(s + 6) + 1));
        if (rd16(s + 6) == 1) {
            s[0] = (uint8_t)(s[0] + s[0]);
            call(e, 0x453b10, 3, OBJH(), 100, e->sound_addr, 0, 0, 0, 0);
        }
        int32_t depth = (int32_t)((uint32_t)rd32(res + 0x14) + 0x14);
        wr32(res + 0x14, depth);
        add32(OBJ() + 0xe8, tm2_wmul(-rd32(res + 8), depth) / 0xfff);
        add32(OBJ() + 0xec, tm2_wmul(-rd32(res + 0xc), depth) / 0xfff);
        add32(OBJ() + 0xf0, tm2_wmul(-rd32(res + 0x10), depth) / 0xfff);
        query(e, s, sa, res);
        type = rd16(res + 0x22);
        if (type == 0x29b || type == 0x29c) wr32(res + 4, 0);
        if (rd32(res + 4) != 0) {
            add32(OBJ() + 0xe8, -rd32(s + 0x14));
            add32(OBJ() + 0xec, -rd32(s + 0x18));
            add32(OBJ() + 0xf0, -rd32(s + 0x1c));
            OBJ()[0xe5] = (uint8_t)(OBJ()[0xe5] + 8);
        }
    }
    {
        int32_t g = (int32_t)(call(e, 0x42c2d4, 1, OBJH() + 0xe8, 0, 0, 0, 0, 0, 0) + 0x10);
        int32_t z = rd32(OBJ() + 0xf0);
        if (g <= z) g = tm2_angle_step(z, g, 0xd);
        wr32(OBJ() + 0xf0, g);
    }
    for (int32_t i = 0; i < ncars; i++) {
        uint32_t model = call(e, 0x45ae24, 1, (uint32_t)i, 0, 0, 0, 0, 0, 0);
        uint32_t car_h = call(e, 0x45afbc, 1, (uint32_t)i, 0, 0, 0, 0, 0, 0);
        uint8_t* car = e->mem(e->user, car_h);
        if (rd32(car + 0x1d8) <= 0) continue;
        if (rd16(s + 2) == rd32(e->mem(e->user, model) + 0x158) && rd16(s + 4) > 0x3fc) continue;
        if ((cheb(OBJ() + 0xe8, car + 0x6e4) >> 3) < 0x14) hit_car(e, s, sa, car_h, model, 1);
    }
    for (int32_t i = 0; i < nai; i++) {
        uint32_t model = call(e, 0x45ae2c, 1, (uint32_t)i, 0, 0, 0, 0, 0, 0);
        uint32_t car_h = call(e, 0x45b080, 1, (uint32_t)i, 0, 0, 0, 0, 0, 0);
        uint8_t* car = e->mem(e->user, car_h);
        if (rd32(car + 0x58c) <= 0) continue;
        if (rd16(s + 2) == rd32(e->mem(e->user, model) + 0x158) && rd16(s + 4) > 0x4ab) continue;
        if ((cheb(OBJ() + 0xe8, car + 0x728) >> 3) < 0x14) hit_car(e, s, sa, car_h, model, 0);
    }
    {
        uint32_t r = call(e, 0x471aec, 0, 0, 0, 0, 0, 0, 0, 0);
        for (int32_t k = 0; k < 6; k++, r += 0x4c) {
            uint8_t* p = e->mem(e->user, r);
            if (rd16(p + 4) == -1) continue;
            int32_t v[3];
            memcpy(v, e->rec_vec, sizeof v);
            if ((cheb(OBJ() + 0xe8, e->mem(e->user, rdu(p + 0x48)) + 0xe8) >> 3) < 10)
                call(e, 0x4723bc, 2, (uint32_t)k, 0, 0, 0, 0, 0, v);
        }
    }
    if (rd16(s + 4) == 100) call(e, 0x453b10, 3, OBJH(), 100, e->sound_addr, 0, 0, 0, 0);
    if (rd16(s + 4) <= 0 && rd32(OBJ() + 8) != 0) kill(e, s);
}

void gp3_roller_update(const Gp3RollerEnv* e) {
    int32_t ncars = (int32_t)call(e, 0x45afb4, 0, 0, 0, 0, 0, 0, 0, 0);
    int32_t nai = (int32_t)call(e, 0x45b078, 0, 0, 0, 0, 0, 0, 0, 0);
    for (uint32_t off = 0; off != 0x1e0; off += 0x30) {
        uint8_t* s = e->slots + off;
        if (rd16(s + 4) > 0) slot_update(e, s, e->slots_addr + off, ncars, nai);
    }
}
