/* gp_aifire.c: see gp_aifire.h */
#include "gameplay/gp_aifire.h"

#include "core/tm2rand.h"

static int32_t pick(GpAiFireEnv* env, uint32_t table, int32_t n) {
    int32_t r = tm2_rand() / 0x148;
    const uint8_t* w = wmem_ptr(env->mem, table, (uint32_t)n);
    int32_t sum = 0, i = 0;
    for (; i < n; i++) {
        sum += w[i];
        if (r < sum)
            break;
    }
    return i;
}

void gp_ai_weapon_fire_decide(GpAiFireEnv* env, uint8_t* car) {
    if (car[0x32] == 0 && w_i16(car, 0x30) != 0xF0)
        return;
    if (w_i16(car, 0x85A) <= 1) {
        uint8_t set = car[0xC8];
        if (set == 0) {
            int32_t i = pick(env, w_u32(car, 0x86C), 9);
            if (i < 8) {
                if (w_i16(car, 0x30) == 0xA0 && i == 0 && (int8_t)car[0x841] == 0)
                    i = 1;
                car[0x840] = 0x6B;
                car[0x841] = (uint8_t)i;
            } else {
                car[0x840] = i == 8 ? 0x64 : 0x6B;
                car[0x841] = 8;
            }
        } else if (set == 1) {
            int32_t i = pick(env, w_u32(car, 0x870), 10);
            if (i < 8) {
                car[0x840] = 0x6B;
                car[0x841] = (uint8_t)i;
            } else {
                car[0x841] = 8;
                car[0x840] = i == 8 ? 0x64 : i == 9 ? 0x65 : 0x6B;
            }
        }
    }
    env->act(env->user, car, 0);
}
