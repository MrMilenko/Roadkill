/* world_misc.c: see world_misc.h. */
#include "world_misc.h"

#include <string.h>

#include "../core/tm2rand.h"
#include "../math/tm2math.h"

static int32_t rd32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static void wr32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }
static int16_t rd16(const uint8_t* p) { int16_t v; memcpy(&v, p, 2); return v; }
static void wr16(uint8_t* p, int16_t v) { memcpy(p, &v, 2); }
static uint32_t rdu32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void wru32(uint8_t* p, uint32_t v) { memcpy(p, &v, 4); }

uint8_t* world_missile_record(uint8_t* records, int32_t i)
{
    if (i < 0 || i >= WORLD_MISSILE_COUNT)
        return 0;
    return records + i * WORLD_MISSILE_SIZE;
}

void world_missiles_create(uint8_t* records, const WorldEntityEnv* env)
{
    for (int i = 0; i < WORLD_MISSILE_COUNT; i++) {
        uint8_t* rec = records + i * WORLD_MISSILE_SIZE;
        uint32_t ent = env->entity_alloc(env->user);
        wru32(rec + 0x34, ent);
        env->entity_insert(env->user, ent);
        wr32(env->resolve(env->user, rdu32(rec + 0x34)) + 0x158, 10);
        wr32(env->resolve(env->user, rdu32(rec + 0x34)) + 8, 0);
        wr16(rec + 4, -15);
        ent = rdu32(rec + 0x34);
        rec[0] = (uint8_t)i;
        wr32(env->resolve(env->user, ent) + 0xC, 0);
    }
}

int32_t world_ai_route_speed_limit(const uint8_t* table, int32_t a, int32_t b)
{
    int32_t v = table[a * 35 + b];
    int32_t t = tm2_frame_ticks60();
    return (int32_t)((uint32_t)(tm2_wmul(v * 19, t) / 100) << 5);
}

void world_ai_route_patch_eiffel(uint8_t* base)
{
    static const int off[7] = {0x30, 0x48, 0x78, 0xF0, 0x150, 0x180, 0x00};
    for (int i = 0; i < 7; i++)
        base[off[i]] = 2;
}

int32_t world_ai_difficulty_speed_scale(uint32_t mode)
{
    if (mode == 1)
        return 0x78;
    if (mode == 2)
        return 0x8C;
    return 0x4B;
}

void world_eiffel_reset(const WorldEiffelEnv* env)
{
    uint8_t* s = env->state;
    s[0x00] = 0;
    s[0x02] = 0;
    wr32(s + 0x1C, 0);
    wr32(s + 0x20, -0x1800);
    wr32(s + 0x24, 0xF0);
    wr16(s + 0x28, 0);
    wr16(s + 0x2A, 0);
    wr16(s + 0x2C, 0);
    wr32(env->tower + 0x30, 0);
    env->set_step(env->user, 0xC80, 0, 0);
    int16_t ang[3];
    int32_t m[9];
    memcpy(ang, s + 0x28, sizeof ang);
    tm2_build_matrix(ang, m);
    memcpy(env->tower + 4, m, sizeof m);
}

void world_section_list_remove(uint8_t* list, int32_t section)
{
    /* the count is the top byte of the i32 at list + 0x2A (sar 24): a signed byte at +0x2D */
    for (int32_t i = 0; i < (int8_t)list[0x2D]; i++) {
        if (rd16(list + 2 * i) != section)
            continue;
        int32_t k = i;
        do {
            k++;
            wr16(list + 2 * (k - 1), rd16(list + 2 * k));
        } while (k < (int8_t)list[0x2D]);
        list[0x2D]--;
    }
}

int world_section_has_moving_surfaces(int32_t s)
{
    return (s > 3 && s < 12) || (s > 13 && s < 16) || (s > 16 && s < 19);
}

void world_fx_sprites_reset(uint8_t* sprites, uint8_t* trails)
{
    for (int i = 0; i < 150; i++) {
        uint8_t* r = sprites + i * 0x30;
        wr32(r + 0x10, 0);
        wr16(r + 0x0C, 0);
        r[7] = (uint8_t)i;
        r[0x0A] = 0xFF;
    }
    for (int i = 0; i < 20; i++)
        wr32(trails + i * 0x24, 0);
}

void world_debris_animation_store(int32_t id, const uint8_t* rec, const WorldDebrisAnimEnv* env)
{
    if (rec[0] != 0) {
        env->error(env->user, id);
        return;
    }
    if (id >= 0x29E && id <= 0x2A1)
        env->parse(env->user, rec, env->slots + (id - 0x29E) * 0x20, 4);
}

void world_fx_random_debris_burst(int32_t* p, const WorldDebrisBurstEnv* env)
{
    env->debris(env->user, tm2_rand() & 3, p);
    uint32_t r = (uint32_t)tm2_rand() & 15;
    if (r == 0)
        env->fx_a(env->user, p, 0);
    else if (r == 1)
        env->fx_b(env->user, p, 0);
    else if (r <= 5)
        env->fx_c(env->user, p, 0);
}

void world_slam_claw_register(uint32_t handle, uint8_t* obj, uint32_t* claw, int32_t* claw_id,
                              void (*rot_x)(void* user, int32_t angle, uint8_t* out), void* user)
{
    if (handle == 0 || obj[0] != 5)
        return;
    *claw = handle;
    *claw_id = 0x2E3;
    rot_x(user, 0x2E3, obj + 4);
}

void world_fx_target_hit_at(uint8_t* target, const int32_t v[3], WorldSetStepFn set_step, void* user)
{
    if (target == 0)
        return;
    wr32(target + 0xE8, v[0]);
    wr32(target + 0xEC, v[1]);
    wr32(target + 0xF0, v[2]);
    set_step(user, 0x29D, 0, 1);
}

void world_track_vehicles_reset(uint8_t* table, uint32_t table_handle, uint32_t* table_ptr)
{
    for (int i = 0; i < 2; i++) {
        wr32(table + i * 0x38 + 0x34, 0);
        wr32(table + i * 0x38 + 0x0C, 0);
    }
    *table_ptr = table_handle;
}

void world_track_vehicle_attach(uint8_t* table, uint32_t handle, uint8_t* obj, int32_t i,
                                void (*start)(void* user, int32_t i), void* user)
{
    if (handle == 0)
        return;
    wru32(table + 0x34 + i * 0x38, handle);
    wr32(obj + 0x158, i + 100);
    start(user, i);
}

int world_track_vehicles_should_stop(int32_t n, uint8_t* (*car)(void* user, int32_t i), void* user)
{
    int r = 0;
    for (int32_t i = 0; i < n; i++)
        if (world_section_has_moving_surfaces(rd32(car(user, i) + 0xC4) >> 16))
            r = 1;
    return r;
}

uint8_t world_scene_choice_state(const uint8_t* obj)
{
    if (obj == 0 || obj[0] != 9)
        return 0xFF;
    return obj[7];
}

void world_scene_choice_step(const uint32_t* rows, int32_t row, int32_t dir,
                             uint8_t* (*resolve)(void* user, uint32_t handle), void* user)
{
    if (row > 15)
        return;
    for (int k = 0; k < 3; k++) {
        uint32_t h = rows[row * 3 + k];
        if (h == 0)
            continue;
        uint8_t* o = resolve(user, h);
        if (dir < 0) {
            uint8_t v = (uint8_t)(o[7] - 1);
            o[7] = v;
            if (v >= o[6])
                o[7] = (uint8_t)(o[6] - 1);
        } else {
            uint8_t v = (uint8_t)(o[7] + 1);
            o[7] = v;
            if (v >= o[6])
                o[7] = 0;
        }
    }
}

uint32_t world_isqrt_inline(int32_t n)
{
    uint32_t root = 0, bit = 0x40000000, rem = (uint32_t)n;
    do {
        if ((int32_t)rem >= (int32_t)bit) {
            uint32_t t = rem - bit;
            if ((int32_t)t >= (int32_t)root) {
                rem = t - root;
                root = (root >> 1) | bit;
                bit >>= 2;
                continue;
            }
        }
        root >>= 1;
        bit >>= 2;
    } while (bit != 0);
    return root;
}

void world_player_car_reset(uint8_t* car, const uint8_t* handicap,
                            void (*setup)(void* user, uint8_t* car, int32_t id, int32_t b),
                            void (*select)(void* user, int32_t id, int32_t z), void* user)
{
    setup(user, car, rd32(car + 0x8E) >> 16, handicap[rd16(car)]);
    select(user, rd32(car + 0x8E) >> 16, 0);
    car[0x13D] = 0;
}

void world_demo_watch_next(const WorldDemoWatchEnv* env)
{
    uint8_t* pl = env->player;
    int32_t next = *env->index + 1;
    *env->cleared = 0;
    if (env->look_mode == 0) {
        *env->index = next;
        if (next >= env->ai_count)
            *env->index = 0;
        const uint8_t* ai = env->ai + *env->index * 0x880;
        wr16(pl + 0x0C, rd16(ai + 0x734));
        wr16(pl + 0x0E, rd16(ai + 0x738));
        wr16(pl + 0x10, rd16(ai + 0x73C));
        wr32(pl + 0x14, rd32(ai + 0x728));
        wr32(pl + 0x1C, rd32(ai + 0x730));
        wr32(pl + 0x18, rd32(ai + 0x72C));
        env->set_state(env->user, pl, 7);
        return;
    }
    pl[0x106] = 5;
    if (env->hold != 0)
        return;
    *env->index = next;
    if (next >= env->ai_count)
        *env->index = env->hold;
    const uint8_t* ai = env->ai + *env->index * 0x880;
    int32_t dx = tm2_wadd(rd32(ai + 0x728), -rd32(pl + 0x6E4));
    int32_t dy = tm2_wadd(rd32(ai + 0x72C), -rd32(pl + 0x6E8));
    int32_t dz = tm2_wadd(rd32(ai + 0x730), -rd32(pl + 0x6EC));
    uint32_t r = world_isqrt_inline(tm2_wadd(tm2_wmul(dy, dy), tm2_wmul(dx, dx)));
    int32_t a = tm2_atan2(dz, (int32_t)r);
    wr16(pl + 0x0C, (int16_t)a);
    wr16(pl + 0x0C, (int16_t)-a);
    wr16(pl + 0x0E, 0);
    wr16(pl + 0x10, (int16_t)tm2_atan2(dx, dy));
}
