/* gp_proj.c: see gp_proj.h */
#include "gameplay/gp_proj.h"

#define OBJ_SIZE 0x160u

static uint8_t* slot_at(ProjEnv* env, int32_t i) { return env->slots + i * PROJ_SLOT_SIZE; }
static uint8_t* obj_at(ProjEnv* env, uint32_t a) { return wmem_ptr(env->mem, a, OBJ_SIZE); }

/* first slot whose life (+0x04) is < 1, or -1 */
static int32_t find_free(ProjEnv* env) {
    for (int32_t i = 0; i < PROJ_SLOTS; i++)
        if (w_i16(slot_at(env, i), 0x04) < 1)
            return i;
    return -1;
}

int32_t gp_proj_fire_type0a(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage) {
    int32_t i = (int16_t)proj_fire_homing(env, owner, target, angles, pos, damage);
    if (i >= 0) {
        uint8_t* s = slot_at(env, i);
        w_s16(s, 0x08, 10);
        w_s16(s, 0x02, 10);
    }
    return i;
}

int32_t gp_proj_fire_type33(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage,
                            uint32_t target_obj) {
    int32_t i = (int16_t)proj_fire_homing(env, owner, target, angles, pos, damage);
    if (i < 0)
        return i;
    uint8_t* s = slot_at(env, i);
    if (env->models[6] != 0)
        w_s32(obj_at(env, w_u32(s, 0x34)), 0x15C, env->models[6]);
    w_s16(s, 0x08, 5);
    w_s16(s, 0x06, 0x4B);
    uint32_t life = w_u16(s, 0x04);
    w_s16(s, 0x02, 0x33);
    s[0x44] = 3;
    w_s16(s, 0x04, life << 2);
    w_s32(s, 0x3C, target_obj);
    return i;
}

int32_t gp_proj_fire_type3d(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage) {
    int32_t i = (int16_t)proj_fire_homing(env, owner, target, angles, pos, damage);
    if (i < 0)
        return i;
    uint8_t* s = slot_at(env, i);
    w_s16(s, 0x08, 0x0F);
    w_s16(s, 0x06, 0x25);
    uint32_t life = w_u16(s, 0x04);
    w_s16(s, 0x02, 0x3D);
    uint32_t o = w_u32(s, 0x34);
    w_s16(s, 0x04, life << 2);
    w_s32(obj_at(env, o), 0x15C, env->models[8]);
    return i;
}

int32_t gp_proj_spawn_type0c(ProjEnv* env, int32_t owner, uint32_t angles, uint32_t pos, int32_t damage) {
    int32_t i = find_free(env);
    if (i >= 0) {
        uint8_t* s = slot_at(env, i);
        w_s32(obj_at(env, w_u32(s, 0x34)), 0x15C, env->models[0]);
        w_s16(s, 0x02, 0x0A);
        proj_object_init(env, s, angles, pos);
        w_s16(s, 0x04, 0x32);
        w_s16(s, 0x06, 0x96);
        w_s16(s, 0x0A, (uint32_t)damage);
        w_s16(s, 0x42, (uint32_t)owner);
        w_s32(s, 0x38, 0);
        proj_trail_start(env, i);
        proj_trail_uvs(env, s);
        proj_trail_step(env, s);
    }
    int32_t r = (int16_t)i;
    if (r >= 0)
        w_s16(slot_at(env, r), 0x02, 0x0C);
    return r;
}

static int32_t fire_0f(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage,
                       int variant_b) {
    int32_t i = find_free(env);
    if (i < 0)
        return -1;
    uint8_t* s = slot_at(env, i);
    uint32_t oaddr = w_u32(s, 0x34);
    w_s16(s, 0x02, 0x0F);
    proj_object_init(env, s, angles, pos);
    w_s16(s, 0x04, 0x96);
    if (variant_b) {
        w_s16(s, 0x06, 0x4B);
        w_s16(s, 0x08, 0x32);
        w_s16(s, 0x46, 100);
        w_s32(s, 0x38, target);
        w_s16(s, 0x0A, (uint32_t)damage);
        w_s16(s, 0x42, (uint32_t)owner);
    } else {
        w_s16(s, 0x06, 0x1E);
        w_s32(s, 0x38, target);
        w_s16(s, 0x0A, (uint32_t)damage);
        w_s16(s, 0x42, (uint32_t)owner);
        w_s16(s, 0x08, owner < 0x32 ? 0x0F : 7);
        w_s16(s, 0x46, 100);
    }
    w_s32(obj_at(env, oaddr), 0x15C, env->models[2]);
    proj_trail_start(env, i);
    proj_trail_uvs(env, s);
    proj_trail_step(env, s);
    uint8_t* o = obj_at(env, oaddr);
    w_s32(o, 0x08, w_u32(o, 0x15C) != 0);
    return i;
}

int32_t gp_proj_fire_type0f(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage) {
    return fire_0f(env, owner, target, angles, pos, damage, 0);
}

int32_t gp_proj_fire_type0f_b(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage) {
    return fire_0f(env, owner, target, angles, pos, damage, 1);
}
