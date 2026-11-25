/*
 * gp3_tornado.c: the tornado special weapon update, TM2.EXE 0x0044bd3c (see gp3_tornado.h).
 */
#include "gp3_tornado.h"

#include <string.h>

#include "../math/tm2math.h"

static int16_t rd16(const uint8_t* p) { int16_t v; memcpy(&v, p, 2); return v; }
static int32_t rd32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static uint32_t rdu(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void wr16(uint8_t* p, int16_t v) { memcpy(p, &v, 2); }
static void wr32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }

#define OBJ   0x00
#define TIMER 0x04
#define SPIN  0x08
#define HPP   0x0c
#define BLKP  0x10
#define OWNER 0x14

#define FXH 0x00
#define REL 0x04
#define MAT 0x10

static uint32_t call(const Gp3TornadoEnv* e, uint32_t addr, int n, uint32_t a0, uint32_t a1, uint32_t a2,
                     uint32_t a3, uint32_t a4, uint32_t a5) {
    uint32_t a[6] = {a0, a1, a2, a3, a4, a5};
    return e->call(e->user, addr, n, a, 0);
}

static int32_t iabs(int32_t v) { return (int32_t)(((uint32_t)v ^ (uint32_t)(v >> 31)) - (uint32_t)(v >> 31)); }

/* max(|dx|, |dy|, |dz|) of owner position (+0xe8) minus p, signed compares */
static int32_t cheb(const uint8_t* owner, const uint8_t* p) {
    int32_t ax = iabs((int32_t)((uint32_t)rd32(owner + 0xe8) - (uint32_t)rd32(p)));
    int32_t ay = iabs((int32_t)((uint32_t)rd32(owner + 0xec) - (uint32_t)rd32(p + 4)));
    int32_t az = iabs((int32_t)((uint32_t)rd32(owner + 0xf0) - (uint32_t)rd32(p + 8)));
    int32_t m = ay < az ? az : ay;
    return ax < m ? m : ax;
}

static void catch_car(const Gp3TornadoEnv* e, uint32_t car_h, uint32_t model, int human) {
    uint8_t* g = e->g_obj;
    uint8_t* f = e->g_fx;
    uint8_t* car = e->mem(e->user, car_h);
    uint32_t owner_h = rdu(g + OWNER);
    uint8_t* owner = e->mem(e->user, owner_h);
    int pos = human ? 0x6e4 : 0x728;
    if (human) car[0xb0] = 1;
    for (int i = 0; i < 3; i++)
        wr32(f + REL + 4 * i, (int32_t)((uint32_t)rd32(car + pos + 4 * i) - (uint32_t)rd32(owner + 0xe8 + 4 * i)));
    int flag = human ? 0xa7 : 0x75;
    if (car[flag] == 0) {
        car[flag] = 1;
        wr16(car + (human ? 0x152 : 0x506), 0x3c);
        call(e, 0x421920, 4, car_h, human ? 1 : 0, 3, 1, 0, 0);
        owner = e->mem(e->user, rdu(g + OWNER));
        call(e, 0x415d1c, 6, rdu(e->mem(e->user, model) + 0x158), 0, 0, 4, e->g_fx_addr + REL,
             rdu(owner + 0x158));
    }
    call(e, 0x430b5c, 1, rdu(g + OWNER) + 0xe8, 0, 0, 0, 0, 0);
    wr32(f + FXH, (int32_t)call(e, 0x44a40c, 4, rdu(g + OWNER) + 0xe8, 0x333, 0x333, 0x333, 0, 0));
    if (call(e, 0x4899c2, 0, 0, 0, 0, 0, 0, 0) & 1)
        call(e, 0x451164, 2, rdu(f + FXH), 0, 0, 0, 0, 0);
    int32_t m[9], rel[3], out[3];
    tm2_build_matrix(e->push_angles, m);
    memcpy(f + MAT, m, sizeof m);
    memcpy(rel, f + REL, sizeof rel);
    tm2_mat_vec_mul_row(m, rel, out);
    owner = e->mem(e->user, rdu(g + OWNER));
    car = e->mem(e->user, car_h);
    wr32(car + pos, (int32_t)((uint32_t)rd32(owner + 0xe8) + (uint32_t)out[0]));
    wr32(car + pos + 4, (int32_t)((uint32_t)rd32(owner + 0xec) + (uint32_t)out[1]));
    int timer_off = human ? 0x152 : 0x506;
    if (human) {
        int32_t vz = rd32(car + 0x6f8);
        wr32(car + 0x6f8, (int32_t)((uint32_t)vz - 0x71));
        wr32(car + 0x6ec, (int32_t)((uint32_t)rd32(owner + 0xf0) + (uint32_t)out[2]));
        wr32(car + 0x6f4, (int32_t)((uint32_t)rd32(car + 0x6f4) - 0x71));
        wr32(car + 0x6f0, (int32_t)((uint32_t)rd32(car + 0x6f0) - 0x71));
    } else {
        int32_t vz = rd32(car + 0x73c);
        wr32(car + 0x73c, (int32_t)((uint32_t)vz - 0x155));
        wr32(car + 0x730, (int32_t)((uint32_t)rd32(owner + 0xf0) + (uint32_t)out[2]));
        wr32(car + 0x738, (int32_t)((uint32_t)rd32(car + 0x738) - 0x71));
        wr32(car + 0x734, (int32_t)((uint32_t)rd32(car + 0x734) - 0x71));
    }
    if (rd16(car + timer_off) < 1) {
        if (rd32(f + REL + 4) == 0) wr32(f + REL + 4, 1);
        int32_t a = tm2_atan2(rd32(f + REL), rd32(f + REL + 4));
        call(e, 0x432340, 3, (uint32_t)(int32_t)rd16(car + (human ? 0x90 : 0x30)), (uint32_t)(int32_t)(int16_t)a,
             0, 0, 0, 0);
    }
}

void gp3_tornado_update(const Gp3TornadoEnv* e) {
    uint8_t* g = e->g_obj;
    uint8_t* f = e->g_fx;
    int32_t ncars = (int32_t)call(e, 0x45afb4, 0, 0, 0, 0, 0, 0, 0);
    int32_t nai = (int32_t)call(e, 0x45b078, 0, 0, 0, 0, 0, 0, 0);
    if (rd16(g + TIMER) <= 0) return;
    wr16(g + TIMER, (int16_t)(rd16(g + TIMER) - tm2_frame_ticks60()));
    if (rd16(g + TIMER) < 0) wr16(g + TIMER, 0);

    uint8_t* obj = e->mem(e->user, rdu(g + OBJ));
    uint8_t* owner = e->mem(e->user, rdu(g + OWNER));
    memmove(obj + 0xe8, owner + 0xe8, 12);
    int32_t z = rd32(obj + 0xf0);
    z = tm2_angle_step(z, (int32_t)((uint32_t)z + 0x78), 5);
    obj = e->mem(e->user, rdu(g + OBJ));
    wr32(obj + 0xf0, z);
    wr16(obj + 0xe0, (int16_t)tm2_angle_interp(rd16(obj + 0xe0), 0x31c, 3));
    int32_t s = tm2_angle_interp(rd16(g + SPIN), 0x2aa, 5);
    wr16(g + SPIN, (int16_t)s);
    obj = e->mem(e->user, rdu(g + OBJ));
    wr16(obj + 0xe4, (int16_t)(rd16(obj + 0xe4) - s));
    {
        int16_t ang[3];
        int32_t m[9];
        memcpy(ang, obj + 0xe0, sizeof ang);
        tm2_build_matrix(ang, m);
        memcpy(obj + 0xb0, m, sizeof m);
    }
    if ((int32_t)call(e, 0x4899c2, 0, 0, 0, 0, 0, 0, 0) % 10 == 0) {
        wr32(f + FXH, (int32_t)call(e, 0x44a40c, 4, rdu(g + OWNER) + 0xe8, 0x333, 0x333, 0x333, 0, 0));
        call(e, 0x451274, 2, rdu(f + FXH), 0, 0, 0, 0, 0);
        call(e, 0x450718, 2, rdu(f + FXH), 0, 0, 0, 0, 0);
    }

    for (int32_t i = 0; i < ncars; i++) {
        uint32_t model = call(e, 0x45ae24, 1, (uint32_t)i, 0, 0, 0, 0, 0);
        uint32_t car_h = call(e, 0x45afbc, 1, (uint32_t)i, 0, 0, 0, 0, 0);
        uint8_t* car = e->mem(e->user, car_h);
        owner = e->mem(e->user, rdu(g + OWNER));
        if ((cheb(owner, car + 0x6e4) >> 3) < 0x23 && rdu(g + OWNER) != model)
            catch_car(e, car_h, model, 1);
        else
            car[0xb0] = 0;
    }
    for (int32_t i = 0; i < nai; i++) {
        uint32_t model = call(e, 0x45ae2c, 1, (uint32_t)i, 0, 0, 0, 0, 0);
        uint32_t car_h = call(e, 0x45b080, 1, (uint32_t)i, 0, 0, 0, 0, 0);
        uint8_t* car = e->mem(e->user, car_h);
        owner = e->mem(e->user, rdu(g + OWNER));
        if ((cheb(owner, car + 0x728) >> 3) < 0x23 && rdu(g + OWNER) != model)
            catch_car(e, car_h, model, 0);
    }

    owner = e->mem(e->user, rdu(g + OWNER));
    if (rd32(owner + 0x158) < 0x32) {
        uint32_t r = call(e, 0x470d08, 0, 0, 0, 0, 0, 0, 0);
        for (int k = 0; k < 50; k++, r += 0x14) {
            uint8_t* p = e->mem(e->user, r);
            if (rd16(p + 4) <= 0) continue;
            owner = e->mem(e->user, rdu(g + OWNER));
            if ((cheb(owner, p + 8) >> 3) < 0x23)
                call(e, 0x470d3c, 3, (uint32_t)(int32_t)rd16(p), (uint32_t)(int32_t)rd16(p + 2), 4, 0, 0, 0);
        }
    }
    {
        uint32_t r = call(e, 0x471aec, 0, 0, 0, 0, 0, 0, 0);
        for (int32_t k = 0; k < 6; k++, r += 0x4c) {
            uint8_t* p = e->mem(e->user, r);
            if (rd16(p + 4) == -1) continue;
            int32_t v[3];
            memcpy(v, e->fx_vec, sizeof v);
            owner = e->mem(e->user, rdu(g + OWNER));
            if ((cheb(owner, e->mem(e->user, rdu(p + 0x48)) + 0xe8) >> 3) < 0x23) {
                uint32_t a[2] = {(uint32_t)k, 0};
                e->call(e->user, 0x4723bc, 2, a, v);
            }
        }
    }

    if (rd16(g + TIMER) > 0 && rd32(e->mem(e->user, rdu(g + HPP))) > 0 &&
        e->mem(e->user, rdu(g + BLKP))[0x0d] == 0)
        return;
    wr32(e->mem(e->user, rdu(g + OBJ)) + 8, 0);
    int32_t hp = rd32(e->mem(e->user, rdu(g + HPP)));
    wr16(g + TIMER, 0);
    if (hp > 0) wr32(e->mem(e->user, rdu(g + OWNER)) + 8, 1);
    if (rd32(e->mem(e->user, rdu(g + HPP))) <= 0)
        call(e, 0x453c38, 1, 0xa0, 0, 0, 0, 0, 0);
    else
        call(e, 0x453c00, 1, 0xa0, 0, 0, 0, 0, 0);
    call(e, 0x4511a8, 2, rdu(g + OWNER) + 0xe8, 0, 0, 0, 0, 0);
    for (int32_t i = 0; i < ncars; i++) {
        uint32_t car_h = call(e, 0x45afbc, 1, (uint32_t)i, 0, 0, 0, 0, 0);
        e->mem(e->user, car_h)[0xb0] = 0;
    }
}
