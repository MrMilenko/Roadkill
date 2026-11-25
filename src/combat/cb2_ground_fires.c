/*
 * cb2_ground_fires.c: groundFiresUpdate, TM2.EXE 0x0044a59c (see cb2_ground_fires.h).
 */
#include "cb2_ground_fires.h"

#include <string.h>

static int16_t rd16(const uint8_t* p) { int16_t v; memcpy(&v, p, 2); return v; }
static int32_t rd32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static void wr16(uint8_t* p, int16_t v) { memcpy(p, &v, 2); }
static void wr32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }

/* cdq / xor / sub: |INT_MIN| stays INT_MIN */
static int32_t absd(int32_t a, int32_t b) {
    uint32_t d = (uint32_t)a - (uint32_t)b, s = (uint32_t)((int32_t)d >> 31);
    return (int32_t)((d ^ s) - s);
}

/* fire position f (x, y, z) against an object position o (x, y, z) */
static int32_t box_dist(const uint8_t* f, const uint8_t* o) {
    int32_t ay = absd(rd32(f + 4), rd32(o + 4)), az = absd(rd32(f + 8), rd32(o + 8));
    int32_t m = az > ay ? az : ay;
    int32_t ax = absd(rd32(f), rd32(o));
    return (ax < m ? m : ax) >> 3;
}

static void add_ticks(const Cb2GroundFiresEnv* e, uint8_t* p) {
    wr16(p, (int16_t)(rd16(p) + e->ticks(e->user)));
}

void cb2_ground_fires_update(const Cb2GroundFiresEnv* e) {
    int32_t nh = e->human_count(e->user), na = e->ai_count(e->user);
    for (int k = 0; k < 30; k++) {
        uint8_t* f = e->fires + 0x28 * k;
        if (!f[0]) continue;
        add_ticks(e, f + 4);
        int16_t age = rd16(f + 4);
        if (age >= 300) {
            f[0] = 0;
            for (int j = 0; j < 5; j++) {
                uint32_t h = (uint32_t)rd32(f + 0x14 + 4 * j);
                if (h) wr16(e->resolve(e->user, h) + 0x0c, 0);
            }
            continue;
        }
        if (age <= 0x14) continue;
        const uint8_t* fp = f + 8;
        for (int32_t i = 0; i < nh; i++) {
            const uint8_t* o = e->human_object(e->user, i);
            uint8_t* c = e->human_car(e->user, i);
            if (box_dist(fp, o + 0xe8) < 0x19 && !c[0xb4] && !c[0xa8] && (!c[0xcf] || rd16(c + 0x87a) < 0x32) &&
                !c[0xbb] && !c[0xbd]) {
                c[0xb4] = 1;
                wr16(c + 0x15a, 0);
                wr32(c + 0x88c, (int32_t)e->flame5(e->user, c + 0x6e4, 1));
            }
        }
        for (int32_t i = 0; i < na; i++) {
            const uint8_t* o = e->ai_object(e->user, i);
            uint8_t* c = e->ai_car(e->user, i);
            if (box_dist(fp, o + 0xe8) < 0x23 && !c[0x82] && !c[0x89] && !c[0x8b]) {
                c[0x82] = 1;
                wr16(c + 0x50e, 0);
                wr32(c + 0x878, (int32_t)e->flame5(e->user, c + 0x728, 1));
            }
        }
        uint8_t* a = e->actors(e->user);
        for (int32_t i = 0; i < 6; i++, a += 0x4c) {
            if (rd16(a + 4) == -1) continue;
            int32_t v[3] = { e->hit_vec[0], e->hit_vec[1], e->hit_vec[2] };
            if (box_dist(fp, e->resolve(e->user, (uint32_t)rd32(a + 0x48)) + 0xe8) < 0x23) e->actor_hit(e->user, i, v);
        }
        for (int32_t i = 1; i <= 3; i++) {
            uint8_t* m = e->marker(e->user, i);
            if (box_dist(fp, m + 8) < 0x23 && rd32(m) == 0) {
                wr32(m, 1);
                e->flame_pair(e->user, m + 8, 0);
                e->explosion16(e->user, m + 8, 0);
                e->clut_sweep(e->user, 0x13ec, i, e->event_type(e->user) == 3 ? 0 : 2);
            }
        }
    }
    for (int32_t i = 0; i < nh; i++) {
        e->human_object(e->user, i);
        uint8_t* c = e->human_car(e->user, i);
        if (!c[0xb4]) continue;
        if (rd32(c + 0x1dc) == 1 && (int32_t)e->rand(e->user) % 4 == 0 && rd32(c + 0x1d8) > 5)
            wr32(c + 0x1d8, rd32(c + 0x1d8) - 1);
        add_ticks(e, c + 0x15a);
        if (rd16(c + 0x15a) >= 300) {
            c[0xb4] = 0;
            uint32_t h = (uint32_t)rd32(c + 0x88c);
            if (h) wr16(e->resolve(e->user, h) + 0x0c, 0);
        }
    }
    for (int32_t i = 0; i < na; i++) {
        e->ai_object(e->user, i);
        uint8_t* c = e->ai_car(e->user, i);
        if (!c[0x82]) continue;
        if ((int32_t)e->rand(e->user) % 4 == 0 && rd32(c + 0x58c) > 5 && c[0x32]) wr32(c + 0x58c, rd32(c + 0x58c) - 1);
        add_ticks(e, c + 0x50e);
        if (rd16(c + 0x50e) >= 300) {
            c[0x82] = 0;
            uint32_t h = (uint32_t)rd32(c + 0x878);
            if (h) wr16(e->resolve(e->user, h) + 0x0c, 0);
        }
    }
}
