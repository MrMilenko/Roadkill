/* gp_spinout.c: see gp_spinout.h */
#include "gameplay/gp_spinout.h"

#include "core/tm2rand.h"
#include "weapons/wmem.h"

static void add16(uint8_t* s, int a, int b) { w_s16(s, a, (uint32_t)(w_u16(s, a) + w_u16(s, b))); }

void gp_car_spin_out_by_object(GpSpinEnv* env, int16_t id, int32_t spin, int32_t fx) {
    uint32_t e = env->entity(env->user, id);
    if (e == 0)
        return;
    uint8_t human = 0;
    int32_t idx = 0;
    if (env->entity_car(env->user, e, &human, &idx) == 0)
        return;
    uint8_t* car = human ? env->human_car(env->user, idx) : env->ai_car(env->user, idx);
    uint8_t* st = car + (human ? 0x58C : 0x5D0);
    if (spin != 0) {
        env->spin(env->user, car, human, (int8_t)spin, 0);
        add16(st, 0x2E, 0x32);
        add16(st, 0x76, 0x7A);
        add16(st, 0xBE, 0xC2);
        add16(st, 0x106, 0x10A);
    }
    if (fx == 0)
        return;
    const uint8_t* b = car + (human ? 0x6C4 : 0x708);
    int32_t p[3];
    p[0] = (int32_t)((uint32_t)(tm2_rand() & 0x3F) + w_u32(b, 0x20) - 0x20);
    p[1] = (int32_t)((uint32_t)(tm2_rand() & 0x3F) + w_u32(b, 0x24) - 0x20);
    uint32_t z = w_u32(b, 0x28);
    p[2] = (int32_t)(z - 0x20 - (uint32_t)(tm2_rand() & 0x1F));
    if (p[2] < 1)
        p[2] = 1;
    env->debris(env->user, tm2_rand() & 3, p);
    uint32_t r = (uint32_t)tm2_rand() & 0xF;
    if (r == 0)
        env->fx(env->user, 0x00451014u, p, 0);
    else if (r == 1)
        env->fx(env->user, 0x004512b4u, p, 0);
    else if (r <= 5)
        env->fx(env->user, 0x00451274u, p, 0);
}
