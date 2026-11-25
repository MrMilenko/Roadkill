/* weapon_hit.c: see weapon_hit.h */
#include "weapons/weapon_hit.h"

#include "scene/car_collide.h"

static void read_vel(const HitEnv* env, uint32_t vel, int32_t v[3]) {
    const uint8_t* p = wmem_ptr(env->mem, vel, 12);
    for (int i = 0; i < 3; i++)
        v[i] = w_i32(p, 4 * i);
}

uint8_t weapon_apply_hit(const HitEnv* env, int32_t hit, int32_t rtype, int32_t rc, int32_t damage, uint32_t vel,
                         int32_t owner) {
    uint8_t is_car = 0;
    int32_t index = 0, attacker = -1;
    if (env->car_lookup(env->user, owner, &is_car, &index)) {
        if (is_car)
            attacker = w_i16(wmem_ptr(env->mem, env->car_data(env->user, index), 4), 2);
        else
            attacker = w_i16(wmem_ptr(env->mem, env->ai_data(env->user, index), 8), 6);
    }
    if (hit >= 0x1F4 && hit < 0x1770 && hit != 0xDAF && hit != 0xDB0) {
        const uint8_t* e = wmem_ptr(env->mem, env->hit_table(env->user) + (uint32_t)(hit - 0x1F4) * 20, 4);
        rtype = w_i16(e, 0);
        rc = w_i16(e, 2);
        hit = 9;
    }
    if (hit == 0xDAF || hit == 0xDB0)
        rtype = hit;
    int32_t v[3];
    if (hit >= 0x1770) {
        read_vel(env, vel, v);
        env->object_hit(env->user, hit - 0x1770, v);
    }
    if (hit > 0 && hit < 0x64 && hit != 9) {
        int32_t dir[3];
        read_vel(env, vel, v);
        collide_normalize(v, dir);
        env->car_damage(env->user, hit, damage, dir, 1, (int16_t)attacker);
        return 1;
    }
    uint32_t t = (uint32_t)rtype;
    if ((t >= 0x1F4 && t <= 0x1FE) || t == 0x207 || t == 0x208) {
        if (owner < 0x32)
            env->level_damage(env->user, rtype, rc, damage);
        return 1;
    }
    if (t == 0xBBD || t == 0xBBE) {
        env->moscow_dish(env->user, damage);
        return 1;
    }
    if (t == 0xDAF || t == 0xDB0) {
        env->eiffel_hit(env->user, (int16_t)(hit - 0xDAF), (int16_t)damage);
        return 1;
    }
    return 0;
}
