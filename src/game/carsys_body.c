/*
 * carsys_body.c: wheel effects and the car body pose (see carsys_body.h).
 */
#include "carsys_body.h"

#include <string.h>

#include "../core/tm2rand.h"
#include "../math/tm2math.h"

static int32_t rd32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static int16_t rd16(const uint8_t* p) { int16_t v; memcpy(&v, p, 2); return v; }
static void wr32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }
static int32_t wsub(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }

void carsys_wheels_spawn_fx(uint8_t* probes, uint8_t kind, const CarsysWheelFx* fx) {
    if (tm2_rand() % 5 != 0) return;
    int k = kind < 3 ? kind : 3;
    static const int wheel[4] = {0x54, 0x9C, 0xE4, 0x12C};
    for (int i = 0; i < 4; i++) fx->fx(fx->user, k, fx->jitter(fx->user, probes + wheel[i], 0x555));
}

void carsys_body_pose_update(uint8_t* car, int player, uint8_t* const* body_objects) {
    int32_t key = rd16(car + (player ? 0x90 : 0x30));
    const uint8_t* body = car + (player ? 0x6C4 : 0x708);
    const uint8_t* p = car + (player ? 0x58C : 0x5D0);
    int32_t slot = (key >> 4) - 1;
    if (slot < 0 || slot >= 15) return;
    uint8_t* obj = body_objects[slot];
    if (!obj) return;
    int32_t h0 = rd16(p + 0x3C), h1 = rd16(p + 0x84), h2 = rd16(p + 0xCC), h3 = rd16(p + 0x114);
    int32_t height = (((h0 + h2) >> 1) + ((h3 + h1) >> 1)) >> 1;
    int32_t axle[3] = {wsub(rd32(p + 0x12C), rd32(p + 0xE4)), wsub(rd32(p + 0x130), rd32(p + 0xE8)),
                       wsub(wsub(rd32(p + 0x134), h3), wsub(rd32(p + 0xEC), h2))};
    int32_t side[3] = {wsub(rd32(p + 0x54), rd32(p + 0xE4)), wsub(rd32(p + 0x58), rd32(p + 0xE8)),
                       wsub(wsub(rd32(p + 0x5C), h0), wsub(rd32(p + 0xEC), h2))};
    int32_t m[9], a[3], s[3];
    memcpy(m, body + 0x50, sizeof m);
    tm2_mat_vec_mul_col(m, axle, a);
    tm2_mat_vec_mul_col(m, side, s);
    int16_t ang[3] = {(int16_t)tm2_atan2(s[2], s[1]), (int16_t)-tm2_atan2(a[2], a[0]), rd16(body + 0x34)};
    int32_t out[9];
    tm2_build_matrix(ang, out);
    memcpy(obj + 4, out, sizeof out);
    wr32(obj + 0x28, rd32(body + 0x20));
    wr32(obj + 0x2C, rd32(body + 0x24));
    if (key == 0xA0 || key == 0xB0 || key == 0x40 || key == 0xC0 || key == 0x60) height += 3;
    wr32(obj + 0x30, height);
}
