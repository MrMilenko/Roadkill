/* projectile.c: see projectile.h */
#include "weapons/projectile.h"

#include "core/tm2rand.h"
#include "math/tm2math.h"

#define OBJ_SIZE 0x160u

static uint8_t* slot_at(ProjEnv* env, int32_t i) { return env->slots + i * PROJ_SLOT_SIZE; }
static uint32_t slot_addr(ProjEnv* env, const uint8_t* s) {
    return PROJ_SLOTS_ADDR + (uint32_t)(s - env->slots);
}
static uint8_t* obj_of(ProjEnv* env, const uint8_t* s, int off) {
    return wmem_ptr(env->mem, w_u32(s, off), OBJ_SIZE);
}
static int16_t type_of(const uint8_t* s) { return w_i16(s, 0x02); }

static void read3(const uint8_t* p, int off, int32_t v[3]) {
    for (int i = 0; i < 3; i++)
        v[i] = w_i32(p, off + 4 * i);
}

/* the render object's matrix from its angles (+0xE0 -> +0xB0) */
static void obj_build_matrix(uint8_t* o) {
    int16_t a[3] = {w_i16(o, 0xE0), w_i16(o, 0xE2), w_i16(o, 0xE4)};
    int32_t m[9];
    tm2_build_matrix(a, m);
    for (int i = 0; i < 9; i++)
        w_s32(o, 0xB0 + 4 * i, (uint32_t)m[i]);
}

/* M^T * v with the render object's matrix (0x0048cc14) */
static void obj_rotate(const uint8_t* o, int32_t x, int32_t y, int32_t z, int32_t out[3]) {
    int32_t m[9], v[3] = {x, y, z};
    for (int i = 0; i < 9; i++)
        m[i] = w_i32(o, 0xB0 + 4 * i);
    tm2_mat_vec_mul_row(m, v, out);
}

void proj_store_model(ProjEnv* env, uint32_t model, uint32_t id) {
    switch (id) {
    case 0x899: env->models[4] = model; env->models[0] = model; break;
    case 0x89A: env->models[1] = model; break;
    case 0x8A0: env->models[2] = model; break;
    case 0x8FD: env->models[8] = model; break;
    case 0x911: env->models[6] = model; break;
    case 0x912: env->models[7] = model; break;
    case 0x916: env->models[5] = model; break;
    default: break;
    }
}

void proj_object_init(ProjEnv* env, uint8_t* s, uint32_t angles, uint32_t pos) {
    uint8_t* o = obj_of(env, s, 0x34);
    const uint8_t* a = wmem_ptr(env->mem, angles, 10);
    const uint8_t* p = wmem_ptr(env->mem, pos, 12);
    s[0x45] = 1;
    s[0x40] = 0;
    s[0x41] = 1;
    w_s32(s, 0x38, 0);
    s[0x44] = 6;
    /* the angles are read at +0, +4, +8 (low halves of three ints) */
    int16_t ax = w_i16(a, 0);
    if (type_of(s) == 0x0F)
        ax = (int16_t)(ax - 0x200);
    else if (type_of(s) == 0x0E)
        ax = (int16_t)(ax - 0xAA);
    w_s16(o, 0xE0, (uint16_t)ax);
    w_s16(o, 0xE2, w_u16(a, 4));
    w_s16(o, 0xE4, w_u16(a, 8));
    for (int i = 0; i < 3; i++) {
        w_s32(o, 0xE8 + 4 * i, w_u32(p, 4 * i));
        w_s32(s, 0x28 + 4 * i, w_u32(p, 4 * i));
    }
    w_s32(o, 0x08, w_u32(o, 0x15C) != 0);
    obj_build_matrix(o);
    w_s16(s, 0x46, 0);
}

void proj_trail_start(ProjEnv* env, int32_t index) {
    uint8_t* s = slot_at(env, index);
    w_s32(s, 0x104, (uint32_t)(w_i8(s, 0x44) * 3));
    for (int k = 0; k < w_i8(s, 0x44); k++) {
        uint8_t* q = s + 0x160 + k * 0x90;
        w_s16(q, 0x06, 0);
        w_s16(q, 0x60, 0);
        w_s16(q, 0x62, 0);
        w_s16(q, 0x04, 0);
        w_s16(q, 0x00, 1);
        w_s16(q, 0x02, 1);
        w_s16(q, 0x34, 1);
        w_s16(q, 0x36, 1);
        w_s16(q, 0x30, 2);
        w_s16(q, 0x32, 2);
        w_s16(q, 0x64, 2);
        w_s16(q, 0x66, 2);
    }
    int32_t r[3];
    obj_rotate(obj_of(env, s, 0x34), 0, -200, 0, r);
    uint8_t* o = obj_of(env, s, 0x34);
    uint8_t* t = obj_of(env, s, 0xF0);
    for (int i = 0; i < 3; i++)
        w_s32(t, 0xE8 + 4 * i, w_u32(o, 0xE8 + 4 * i));
    for (int k = 0; k < w_i8(s, 0x44) + 1; k++) {
        for (int j = 0; j < 3; j++) {
            uint8_t* v = s + 0x48 + (3 * k + j) * 8;
            w_s16(v, 0, (uint32_t)r[0]);
            w_s16(v, 2, (uint32_t)r[1]);
            w_s16(v, 4, (uint32_t)r[2]);
        }
    }
    w_s32(t, 0x08, 1);
}

void proj_trail_uvs(ProjEnv* env, uint8_t* s) {
    int seg = w_i8(s, 0x40);
    for (int k = 0; k < w_i8(s, 0x44); k++) {
        int n = w_i8(s, 0x44);
        int f = ((n >> 1) + 8 * k) / n;
        const uint8_t* fr = wmem_ptr(env->mem, env->trail_frames + (uint32_t)f * 8, 8);
        uint16_t u = fr[0], v = fr[1], w = fr[2], h = fr[3];
        for (int j = 0; j < 3; j++) {
            uint8_t* q = s + 0x160 + seg * 0x90 + j * 0x30;
            w_s16(q, 0x1C, u);
            w_s16(q, 0x1E, v);
            w_s16(q, 0x18, (uint16_t)(u + w));
            w_s16(q, 0x1A, v);
            w_s16(q, 0x20, u);
            w_s16(q, 0x22, (uint16_t)(v + h));
            w_s16(q, 0x24, (uint16_t)(u + w));
            w_s16(q, 0x26, (uint16_t)(v + h));
            w_s32(q, 0x28, w_u16(fr, 4));
        }
        seg--;
        if (seg < 0)
            seg = w_i8(s, 0x44) - 1;
    }
}

/* the three head vertices of group g: object position + rotated offsets, relative to +0x28 */
static void trail_head(ProjEnv* env, uint8_t* s, int g) {
    static const int32_t offs[3][3] = {{0, 0, 8}, {8, 0, -8}, {-8, 0, -8}};
    for (int j = 0; j < 3; j++) {
        int32_t r[3];
        const uint8_t* o = obj_of(env, s, 0x34);
        obj_rotate(o, offs[j][0], offs[j][1], offs[j][2], r);
        o = obj_of(env, s, 0x34);
        uint8_t* v = s + 0x48 + (g + j) * 8;
        for (int i = 0; i < 3; i++)
            w_s16(v, 2 * i, (uint32_t)r[i] + w_u32(o, 0xE8 + 4 * i) - w_u32(s, 0x28 + 4 * i));
    }
}

void proj_trail_step(ProjEnv* env, uint8_t* s) {
    s[0x45] = (uint8_t)(s[0x45] - 1);
    if (s[0x45] != 0) {
        trail_head(env, s, 3 * w_i8(s, 0x41));
        return;
    }
    int8_t head = (int8_t)(s[0x40] + 1);
    int old = 3 * w_i8(s, 0x41);
    s[0x40] = (uint8_t)head;
    if (head >= w_i8(s, 0x44))
        s[0x40] = 0;
    int seg = w_i8(s, 0x40);
    uint8_t* q0 = s + 0x160 + seg * 0x90;
    /* the back edge of the new segment joins the old head group */
    w_s16(q0, 0x04, (uint32_t)(old + 1));
    w_s16(q0, 0x02, (uint32_t)old);
    w_s16(q0, 0x34, (uint32_t)(old + 2));
    w_s16(q0, 0x32, (uint32_t)(old + 1));
    w_s16(q0, 0x64, (uint32_t)old);
    w_s16(q0, 0x62, (uint32_t)(old + 2));
    s[0x41] = (uint8_t)(s[0x41] + 1);
    if (w_i8(s, 0x41) > 6)
        s[0x41] = 0;
    int g = 3 * w_i8(s, 0x41);
    w_s16(q0, 0x06, (uint32_t)(g + 1));
    w_s16(q0, 0x00, (uint32_t)g);
    w_s16(q0, 0x36, (uint32_t)(g + 2));
    w_s16(q0, 0x30, (uint32_t)(g + 1));
    w_s16(q0, 0x66, (uint32_t)g);
    w_s16(q0, 0x60, (uint32_t)(g + 2));
    trail_head(env, s, g);
    s[0x45] = 2;
    proj_trail_uvs(env, s);
}

static int32_t find_free(ProjEnv* env) {
    for (int i = 0; i < PROJ_SLOTS; i++)
        if (w_i16(slot_at(env, i), 0x04) < 1)
            return i;
    return -1;
}

void proj_spawn_plain(ProjEnv* env, int32_t owner, uint32_t angles, uint32_t pos, int32_t damage) {
    int32_t i = find_free(env);
    if (i >= 0) {
        uint8_t* s = slot_at(env, i);
        w_s32(obj_of(env, s, 0x34), 0x15C, env->models[0]);
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
    int16_t r = (int16_t)i;
    if (r >= 0) {
        uint8_t* s = slot_at(env, r);
        if (env->models[7] != 0)
            w_s32(obj_of(env, s, 0x34), 0x15C, env->models[7]);
        w_s16(s, 0x06, 0x96);
        s[0x44] = 1;
        w_s32(obj_of(env, s, 0xF0), 0x08, 0);
        w_s16(s, 0x02, 0x2B);
    }
}

/* 0x00414458 and 0x004146f8 differ in type, life, mode, model and the trail frames */
static int32_t fire(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage,
                    int colored) {
    int32_t i = find_free(env);
    if (i < 0)
        return -1;
    uint8_t* s = slot_at(env, i);
    uint32_t oaddr = w_u32(s, 0x34);
    w_s16(s, 0x02, colored ? 0x29 : 0x0B);
    proj_object_init(env, s, angles, pos);
    w_s16(s, 0x04, colored ? 100 : 0x32);
    w_s16(s, 0x06, 0x96);
    w_s16(s, 0x08, colored ? 0x23 : 0x32);
    w_s32(s, 0x38, target);
    w_s16(s, 0x0A, (uint32_t)damage);
    w_s16(s, 0x42, (uint32_t)owner);
    uint8_t* o = wmem_ptr(env->mem, oaddr, OBJ_SIZE);
    w_s32(o, 0x15C, colored ? env->models[4] : env->models[0]);
    proj_trail_start(env, i);
    if (!colored)
        proj_trail_uvs(env, s);
    proj_trail_step(env, s);
    o = wmem_ptr(env->mem, oaddr, OBJ_SIZE);
    w_s32(o, 0x08, w_u32(o, 0x15C) != 0);
    return i;
}

int32_t proj_fire_homing(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage) {
    return fire(env, owner, target, angles, pos, damage, 0);
}

int32_t proj_fire_colored(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage) {
    return fire(env, owner, target, angles, pos, damage, 1);
}

void proj_fire_freeze(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage) {
    /* the homing fire gets the owner as a 16 bit value; the test below uses all 32 bits */
    int32_t i = (int16_t)proj_fire_homing(env, (int16_t)owner, target, angles, pos, damage);
    if (i < 0)
        return;
    uint8_t* s = slot_at(env, i);
    w_s16(s, 0x08, owner < 0x32 ? 0x0F : 4);
    w_s16(s, 0x06, 0x32);
    w_s32(obj_of(env, s, 0x34), 0x15C, env->models[1]);
    w_s16(s, 0x02, 0x0E);
}

int32_t proj_fire_dark_tooth(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage) {
    int32_t i = (int16_t)proj_fire_homing(env, owner, target, angles, pos, damage);
    if (i < 0)
        return i;
    uint8_t* s = slot_at(env, i);
    w_s16(s, 0x08, (tm2_rand() & 1) ? 0x63 : 0x14);
    w_s16(s, 0x06, 0x25);
    w_s16(s, 0x02, 0x3D);
    w_s16(s, 0x04, (uint32_t)(w_i16(s, 0x04) * 5));
    w_s32(obj_of(env, s, 0x34), 0x15C, env->models[8]);
    return i;
}

int32_t proj_fire_ghost(ProjEnv* env, int32_t owner, uint32_t target, uint32_t angles, uint32_t pos, int32_t damage) {
    int32_t i = (int16_t)proj_fire_colored(env, owner, target, angles, pos, damage);
    if (i < 0)
        return i;
    uint8_t* s = slot_at(env, i);
    w_s16(s, 0x08, 100);
    w_s16(s, 0x06, 0x4B);
    w_s16(s, 0x02, 0x31);
    w_s16(s, 0x04, 0x50);
    if (env->models[5] != 0)
        w_s32(obj_of(env, s, 0x34), 0x15C, env->models[5]);
    return i;
}

/* ---- flight ---------------------------------------------------------------------------- */

static int32_t iabs(int32_t v) {
    uint32_t m = (uint32_t)(v >> 31);
    return (int32_t)(((uint32_t)v ^ m) - m);
}

/* the freeze missile's hit sound: placed at pos, played without the explosion cooldown */
static void hit_sound(ProjEnv* env, const int32_t pos[3]) {
    int32_t pan;
    if (env->split == 0) {
        env->sound_locate(env->user, pos, 0);
        pan = *env->snd_pan;
    } else {
        env->sound_locate(env->user, pos, -1);
        pan = 0;
    }
    env->sound_play(env->user, 8, *env->snd_volume, pan, 0x4B);
}

static void dark_tooth_burst(ProjEnv* env, const int32_t pos[3]) {
    env->fx(env->user, PROJ_FX_SMOKE_BURST, pos, 0);
    for (int i = 0; i < 3; i++) {
        uint32_t a = env->jitter(env->user, pos, 0, 0xA4, 0xA4, 0xA4);
        env->fx(env->user, PROJ_FX_TRAIL_PUFF, 0, a);
    }
}

/* the render object's position as the original passes it to effects (pointer to +0xE8) */
#define OBJPOS(o, oaddr) (read3((o), 0xE8, op), op), (oaddr) + 0xE8

/* ends the slot named by its own index byte: life -1, render object hidden */
static void end_slot(ProjEnv* env, int32_t idx) {
    uint8_t* e = slot_at(env, (int16_t)idx);
    w_s16(e, 0x04, 0xFFFF);
    w_s32(obj_of(env, e, 0x34), 0x08, 0);
}

void proj_tick(ProjEnv* env, uint8_t* s) {
    int32_t idx = w_i8(s, 0x00);
    uint32_t rec = 0;
    int32_t op[3];
    uint32_t oaddr = w_u32(s, 0x34);
    uint8_t* o = obj_of(env, s, 0x34);

    if (type_of(s) == 0x0F) {
        w_s16(o, 0xE0, (uint32_t)(w_i16(o, 0xE0) + (w_i16(s, 0x42) == 0xDAF ? 0x0C : 0x20)));
        int16_t a = w_i16(o, 0xE0);
        tm2_angle_wrap16(&a);
        w_s16(o, 0xE0, (uint16_t)a);
        if (w_i16(o, 0xE0) > 0x400)
            w_s16(o, 0xE0, 0x400);
        if (w_u32(s, 0x38) == 0)
            obj_build_matrix(o);
    }
    if (type_of(s) == 0x3D) {
        /* the dark tooth missile wobbles: pitch and roll from the remaining life */
        w_s16(o, 0xE2, 0);
        int32_t t300 = tm2_wmul(w_i16(s, 0x04), 300);
        if (w_i16(s, 0x08) == 99) {
            w_s16(o, 0xE0, (uint32_t)(tm2_wmul(tm2_sin(t300), -0x11C) / 0xFFF));
            int32_t roll = w_i16(o, 0xE4);
            w_s16(o, 0xE4, (uint32_t)(tm2_wmul(tm2_sin(tm2_wmul(w_i16(s, 0x04), 300)), -0xAA) / 0xFFF + roll));
        } else {
            w_s16(o, 0xE0, (uint32_t)(tm2_wmul(tm2_cos(0xA58 - t300), 0x11C) / 0xFFF));
            if ((int8_t)env->event_type(env->user) == 8 && env->entity_by_type(env->user, 0xF0) != 0) {
                uint32_t ent = env->entity_by_type(env->user, 0xF0);
                const uint8_t* e = wmem_ptr(env->mem, ent, 0x15C);
                if (w_i16(s, 0x42) == w_i32(e, 0x158)) {
                    int32_t roll = w_i16(o, 0xE4);
                    w_s16(o, 0xE4,
                          (uint32_t)(tm2_wmul(tm2_sin(tm2_wmul(w_i16(s, 0x04), 300)), 0xAA) / 0xFFF + roll));
                }
            }
        }
        obj_build_matrix(o);
        for (int i = 0; i < 2; i++) {
            read3(o, 0xE8, op);
            uint32_t a = env->jitter(env->user, op, oaddr + 0xE8, 0x555, 0x555, 0x555);
            env->fx(env->user, PROJ_FX_TRAIL_PUFF, 0, a);
        }
    }
    if (w_u32(s, 0x38) != 0) {
        env->steer(env->user, slot_addr(env, s));
    } else if (type_of(s) == 0x33) {
        /* a homing missile without a target locks on once it is far enough from its start */
        o = obj_of(env, s, 0x34);
        int32_t dy = iabs(w_i32(s, 0x2C) - w_i32(o, 0xEC)), dz = iabs(w_i32(s, 0x30) - w_i32(o, 0xF0));
        int32_t m = dy < dz ? dz : dy;
        int32_t dx = iabs(w_i32(s, 0x28) - w_i32(o, 0xE8));
        int32_t d = dx < m ? m : dx;
        if ((d >> 3) > 300) {
            w_s16(s, 0x08, 0x1E);
            w_s16(s, 0x06, 0x32);
            w_s32(s, 0x38, w_u32(s, 0x3C) + 0xE8);
            w_s16(s, 0x0A, (uint32_t)(w_u16(s, 0x0A) << 2));
        }
    }

    o = obj_of(env, s, 0x34);
    oaddr = w_u32(s, 0x34);
    if (w_i16(s, 0x04) < 1) {
        w_s16(s, 0x04, (uint32_t)(w_i16(s, 0x04) - 1));
        proj_trail_step(env, s);
        return;
    }

    int32_t speed = w_i16(s, 0x06);
    if (speed < 0x96)
        w_s16(s, 0x06, (uint32_t)(w_i16(s, 0x06) + 5));
    else
        w_s16(s, 0x06, 0x96);
    int32_t vel[3];
    obj_rotate(o, 0, (speed * 20) / tm2_frame_fps(), 0, vel);
    int32_t loc[3];
    read3(o, 0xE8, loc);
    for (int i = 0; i < 3; i++)
        w_s32(o, 0xE8 + 4 * i, w_u32(o, 0xE8 + 4 * i) + (uint32_t)vel[i]);
    w_s16(s, 0x04, (uint32_t)(w_i16(s, 0x04) - 1));
    if (type_of(s) == 0x3D && w_i16(s, 0x06) > 0x4B)
        w_s16(s, 0x06, 0x4B);
    int32_t drop = w_i16(s, 0x46);
    if (drop >= 0x10) {
        w_s32(o, 0xF0, w_u32(o, 0xF0) - 0x10u);
        w_s16(s, 0x46, (uint32_t)(w_i16(s, 0x46) - 0x10));
    } else if (drop > 0) {
        w_s32(o, 0xF0, w_u32(o, 0xF0) - (uint32_t)drop);
        w_s16(s, 0x46, 0);
    }

    /* two hit tests: half way back along this frame's move, then at the new position */
    int counted = 0;
    for (int32_t k = 1; k >= 0; k--) {
        o = obj_of(env, s, 0x34);
        for (int i = 0; i < 3; i++)
            loc[i] = (int32_t)(w_u32(o, 0xE8 + 4 * i) - (uint32_t)(tm2_wmul(vel[i], k) >> 1));
        int32_t owner = w_i16(s, 0x42);
        int32_t hit = env->hit_test(env->user, loc, owner, owner < 0x32 ? 0x0F : 8, type_of(s));
        w_s32(s, 0x0C, (uint32_t)hit);
        if (w_i16(s, 0x42) == w_i32(s, 0x0C) && type_of(s) == 0x33 && w_u32(s, 0x38) != 0) {
            const uint8_t* tp = wmem_ptr(env->mem, w_u32(s, 0x38), 8);
            const uint8_t* to = obj_of(env, s, 0x3C);
            if (w_i32(to, 0xE8) == w_i32(tp, 0) && w_i32(to, 0xEC) == w_i32(tp, 4)) {
                /* a homing missile that reached its own owner's target point: counts for car 0x90 */
                end_slot(env, idx);
                int32_t n = env->car_count(env->user);
                for (int32_t i = 0; i < n; i++) {
                    env->car_model_slot(env->user, i);
                    const uint8_t* c = wmem_ptr(env->mem, env->car_data(env->user, i), 0x858);
                    if (w_i16(c, 0x90) == 0x90 && !counted) {
                        uint8_t* cw = wmem_ptr(env->mem, env->car_data(env->user, i), 0x858);
                        w_s16(cw, 0x856, (uint32_t)(w_i16(cw, 0x856) + 1));
                        counted = 1;
                    }
                }
            }
        }
        if (w_i16(s, 0x42) == w_i32(s, 0x0C))
            w_s32(s, 0x0C, 0);
        if (w_i32(s, 0x0C) < 0)
            w_s32(s, 0x0C, 0);
        if ((k & 1) && w_i32(s, 0x0C) == 0) {
            int32_t h = 0;
            o = obj_of(env, s, 0x34);
            rec = env->level_hit(env->user, w_i16(s, 0x42), w_u32(o, 0x158), loc, &h);
            w_s32(s, 0x0C, (uint32_t)h);
            const uint8_t* r = wmem_ptr(env->mem, rec, 0x10);
            if (w_i32(s, 0x0C) == 9 && w_i16(r, 0x0A) == -w_i16(s, 0x42))
                w_s32(s, 0x0C, 0);
            int32_t rt = w_i16(r, 0x0A);
            if (rt == 0xCE5 || rt == 2 || rt == 0x29B || rt == 0x29C || (rt >= 0x191 && rt <= 0x1B8) ||
                (rt >= 0xDAC && rt < 0xDB6))
                w_s32(s, 0x0C, 0);
            if (type_of(s) == 0x31 && (rt < 0x1F4 || rt > 0x1FE))
                w_s32(s, 0x0C, 0);
            if (w_i32(s, 0x0C) == 9 && type_of(s) == 0x33 && w_u32(s, 0x38) != 0) {
                const uint8_t* tp = wmem_ptr(env->mem, w_u32(s, 0x38), 4);
                if (w_i32(tp, 0) == w_i32(obj_of(env, s, 0x3C), 0xE8))
                    w_s32(s, 0x0C, 0);
            }
        }
        if (w_i32(s, 0x0C) <= 0)
            continue;

        /* a hit */
        loc[2] += 0x18;
        if (w_i32(s, 0x0C) == 9) {
            if (type_of(s) == 0x0E) {
                hit_sound(env, loc);
                env->fx(env->user, PROJ_FX_SPARKS, loc, 0);
                env->fx(env->user, PROJ_FX_FREEZE_HIT, loc, 0);
            } else if (type_of(s) == 0x3D) {
                dark_tooth_burst(env, loc);
            } else {
                env->fx(env->user, PROJ_FX_EXPLOSION, loc, 0);
            }
            if (type_of(s) == 0x0F) {
                o = obj_of(env, s, 0x34);
                oaddr = w_u32(s, 0x34);
                w_s32(o, 0xF0, w_u32(o, 0xF0) + 0x78);
                const uint8_t* r = wmem_ptr(env->mem, rec, 0x10);
                int32_t rt = w_i16(r, 0x0A);
                if (rt >= 0x1F4 && rt <= 0x208 && rt != 0x1FC)
                    w_s32(o, 0xF0, (uint32_t)env->ground(env->user, OBJPOS(o, oaddr)));
                env->fx(env->user, PROJ_FX_BIG_EXPLOSION, OBJPOS(o, oaddr));
                env->fx(env->user, PROJ_FX_FLASH, OBJPOS(o, oaddr));
                rt = w_i16(wmem_ptr(env->mem, rec, 0x10), 0x0A);
                if (rt != 0xBBD && rt != 0x1FC)
                    env->fx(env->user, PROJ_FX_GROUND_FIRE, OBJPOS(o, oaddr));
            }
        } else if (w_i16(s, 0x0A) >= 0) {     /* damage >= 0 */
            if (type_of(s) == 0x0E) {
                hit_sound(env, loc);
                env->fx(env->user, PROJ_FX_SPARKS, loc, 0);
            } else if (type_of(s) == 0x3D) {
                dark_tooth_burst(env, loc);
            } else {
                env->fx(env->user, PROJ_FX_EXPLOSION, loc, 0);
                env->fx(env->user, PROJ_FX_DEBRIS, loc, 0);
            }
            if (type_of(s) == 0x0F) {
                o = obj_of(env, s, 0x34);
                oaddr = w_u32(s, 0x34);
                w_s32(o, 0xF0, w_u32(o, 0xF0) + 0x78);
                if (w_i32(s, 0x0C) >= 0x1F4 && w_i32(s, 0x0C) < 0x1770)
                    w_s32(o, 0xF0, (uint32_t)env->ground(env->user, OBJPOS(o, oaddr)));
                env->fx(env->user, PROJ_FX_BIG_EXPLOSION, OBJPOS(o, oaddr));
                env->fx(env->user, PROJ_FX_FLASH, loc, 0);
                env->fx(env->user, PROJ_FX_GROUND_FIRE, OBJPOS(o, oaddr));
            }
        }
        end_slot(env, idx);
        k = -1;
        if (rec == 0) {
            env->apply_hit(env->user, w_i32(s, 0x0C), 0, 0, w_i16(s, 0x0A), vel, w_i16(s, 0x42));
        } else {
            const uint8_t* r = wmem_ptr(env->mem, rec, 0x10);
            env->apply_hit(env->user, w_i32(s, 0x0C), w_i16(r, 0x0A), w_i16(r, 0x0C), w_i16(s, 0x0A), vel,
                           w_i16(s, 0x42));
        }
        int t = type_of(s);
        if (t == 0x0C || t == 0x0B || t == 0x0F || t == 0x0A)
            env->sound_end(env->user, type_of(s), w_i8(s, 0x00) + 1);
    }

    if (w_i16(s, 0x04) == 0) {
        /* out of life: blows up where it is */
        o = obj_of(env, s, 0x34);
        oaddr = w_u32(s, 0x34);
        if (w_i32(o, 0xF0) <= 0)
            w_s32(o, 0xF0, 0);
        if (type_of(s) == 0x0E) {
            hit_sound(env, loc);
            env->fx(env->user, PROJ_FX_SPARKS, loc, 0);
            env->fx(env->user, PROJ_FX_FREEZE_HIT, loc, 0);
        } else if (type_of(s) == 0x3D) {
            dark_tooth_burst(env, loc);
        } else {
            env->fx(env->user, PROJ_FX_EXPLOSION, OBJPOS(o, oaddr));
        }
        if (type_of(s) == 0x0F) {
            o = obj_of(env, s, 0x34);
            oaddr = w_u32(s, 0x34);
            w_s32(o, 0xF0, w_u32(o, 0xF0) + 0x78);
            env->fx(env->user, PROJ_FX_BIG_EXPLOSION, OBJPOS(o, oaddr));
            w_s32(o, 0xF0, w_u32(o, 0xF0) - 0x78);
            env->fx(env->user, PROJ_FX_GROUND_FIRE, OBJPOS(o, oaddr));
        }
        end_slot(env, idx);
        int t = type_of(s);
        if ((t == 0x0C || t == 0x0B || t == 0x0A) && t != 0x0F)
            env->sound_end(env->user, type_of(s), w_i8(s, 0x00) + 1);
    }
    proj_trail_step(env, s);
}

void proj_update_all(ProjEnv* env) {
    for (int i = 0; i < PROJ_SLOTS; i++) {
        uint8_t* s = slot_at(env, i);
        if (w_i16(s, 0x04) == 0) {
            w_s16(s, 0x04, 0xFFFF);
            w_s32(obj_of(env, s, 0x34), 0x08, 0);
        }
        if (w_i16(s, 0x04) <= -0x0C)
            w_s32(obj_of(env, s, 0xF0), 0x08, 0);
        else
            proj_tick(env, s);
    }
}
