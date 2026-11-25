/* gp_cars.c: see gp_cars.h */
#include "gameplay/gp_cars.h"

#include "weapons/wmem.h"

void gp_cars_pick_spawns(GpCarsEnv* env) {
    env->spawn_reset(env->user);
    int32_t n = env->car_count(env->user);
    for (int32_t i = 0; i < n; i++) {
        uint8_t* c = env->car(env->user, i);
        w_s32(c, 0x208, (uint32_t)env->spawn_pick(env->user, *env->spawn_count));
    }
}

void gp_cars_spawn_events(GpCarsEnv* env, uint8_t flag, uint32_t esi) {
    int32_t n = env->player_count(env->user);
    for (int32_t i = 0; i < n; i++) {
        uint8_t* c = env->car(env->user, i);
        env->car_reset(env->user, w_u32(c, 0x894));
        env->event(env->user, 1, w_u16(c, 0x208), esi);
        if (flag != 0)
            env->event_flush(env->user, 0, 0);
    }
}
