/* collapse.c: see collapse.h. */
#include "collapse.h"

#include <string.h>

#include "../core/tm2rand.h"
#include "../math/tm2math.h"

static int32_t rd32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static void wr32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }
static uint32_t rdu32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void wru32(uint8_t* p, uint32_t v) { memcpy(p, &v, 4); }
static void add32(uint8_t* p, int32_t v) { wr32(p, tm2_wadd(rd32(p), v)); }

#define REC(s) (env->table + (s) * WORLD_COLLAPSE_REC)
/* section record fields */
#define R_OBJ 0x00
#define R_DZ 0x04
#define R_TIMER 0x08
#define R_COUNT 0x10
#define R_REROUTE 0x14
#define R_ARMED 0x18
#define R_FALLING 0x19
#define R_SHAKING 0x1A
#define R_LINKS 0x1C
/* state fields */
#define S_BUSY 0x00
#define S_SECTION 0x04
#define S_TOTAL 0x08
#define S_DONE 0x0C
#define S_TIMER 0x10
#define S_COOLDOWN 0x14
#define S_BLASTS 0x18
/* GRP fields */
#define G_X(s) (0x0C + 12 * (s))
#define G_Y(s) (0x10 + 12 * (s))
#define G_Z(s) (0x14 + 12 * (s))
#define G_U(s) (0x1B0 + 4 * (s))
#define G_V(s) (0x23C + 4 * (s))

static int32_t st(const WorldCollapseEnv* env, int off) { return rd32(env->state + off); }
static void st_set(const WorldCollapseEnv* env, int off, int32_t v) { wr32(env->state + off, v); }
static int32_t grp(const WorldCollapseEnv* env, int off) { return rd32(env->grp + off); }

void world_collapse_register(const WorldCollapseEnv* env, uint32_t handle, const uint8_t* obj)
{
    int32_t s;
    uint16_t v;
    if (obj[0] == 4) {
        memcpy(&v, obj + 0x10, 2);
        s = v;
    } else if (obj[0] == 5) {
        memcpy(&v, obj + 0x34, 2);
        s = v;
    } else {
        return;
    }
    wru32(REC(s) + R_OBJ, handle);
    REC(s)[R_ARMED] = 1;
    st_set(env, S_TOTAL, tm2_wadd(st(env, S_TOTAL), rd32(REC(s) + R_COUNT)));
}

void world_collapse_unlink(const WorldCollapseEnv* env, int32_t s)
{
    for (int i = 0; i < WORLD_COLLAPSE_SECTIONS; i++)
        for (int k = 0; k < 6; k++)
            if ((int8_t)REC(i)[R_LINKS + k] == s)
                REC(i)[R_LINKS + k] = 0xFF;
}

void world_collapse_clear_done(const WorldCollapseEnv* env)
{
    for (int32_t i = 0; i < st(env, S_DONE); i++) {
        world_collapse_unlink(env, i);
        REC(i)[R_ARMED] = 0;
        env->section_disable(env->user, i);
    }
}

void world_collapse_move_section(const WorldCollapseEnv* env, int32_t s)
{
    uint8_t* obj = env->resolve(env->user, rdu32(REC(s) + R_OBJ));
    int32_t dz = rd32(REC(s) + R_DZ);
    if (obj[0] == 4)
        add32(obj + 0x0C, dz);
    else if (obj[0] == 5)
        add32(obj + 0x30, dz);
    add32(env->grp + G_Z(s), rd32(REC(s) + R_DZ));
    if (s == 6) {
        add32(env->grp + G_Z(7), rd32(REC(7) + R_DZ));
        add32(env->grp + G_Z(8), rd32(REC(8) + R_DZ));
    } else if (s == 13) {
        add32(env->grp + G_Z(14), rd32(REC(14) + R_DZ));
    } else if (s == 19) {
        add32(env->grp + G_Z(20), rd32(REC(20) + R_DZ));
    } else if (s == 22) {
        add32(env->grp + G_Z(23), rd32(REC(23) + R_DZ));
    } else if (s == 26) {
        add32(env->grp + G_Z(27), rd32(REC(27) + R_DZ));
    }
    int32_t x = tm2_wadd(grp(env, G_X(s)), grp(env, G_U(s)) / 2);
    int32_t y = tm2_wadd(grp(env, G_Y(s)), grp(env, G_V(s)) / 2);
    int32_t z = grp(env, G_Z(s));
    if (env->marker) {
        uint8_t* m = env->resolve(env->user, env->marker);
        wr32(m + 0xE8, x);
        wr32(m + 0xEC, y);
        wr32(m + 0x08, 1);
        wr32(m + 0xF0, z);
        env->set_step(env->user, 0xC1F, 0, 1);
    }
}

void world_collapse_debris(const WorldCollapseEnv* env, int32_t* p)
{
    int done = 0;
    if (st(env, S_TIMER) / 30 + 1 > st(env, S_BLASTS) && (tm2_rand() & 7) == 1) {
        p[2] = tm2_wadd(p[2], -(tm2_rand() % 120));
        if (env->spawn_blast(env->user, p) & 0xFF) {
            st_set(env, S_BLASTS, st(env, S_BLASTS) + 1);
            done = 1;
        }
    }
    int32_t mode = env->game_mode(env->user);
    if (mode == 2 || env->game_mode(env->user) > 3)
        done = 1;
    if (done)
        return;
    if (st(env, S_COOLDOWN) > 0) {
        st_set(env, S_COOLDOWN, tm2_wadd(st(env, S_COOLDOWN), -tm2_frame_ticks60()));
        return;
    }
    int32_t t = st(env, S_TIMER);
    st_set(env, S_COOLDOWN, t < 120 ? 120 : t < 240 ? 60 : 20);
    if ((tm2_rand() & 3) == 0) {
        p[2] = tm2_wadd(p[2], tm2_rand() % 240);
        env->fx(env->user, 0, p, 0);
        return;
    }
    if ((tm2_rand() & 3) == 1) {
        env->fx(env->user, 1, p, 0);
        return;
    }
    p[2] = tm2_wadd(p[2], tm2_rand() % 120);
    if (tm2_rand() & 1)
        env->fx(env->user, 2, p, 0);
    else
        env->fx(env->user, 3, p, 0);
}

static int link_set(const WorldCollapseEnv* env, int32_t s, int k)
{
    return (int8_t)REC(s)[R_LINKS + k] != -1;
}

static void debris_at(const WorldCollapseEnv* env, int32_t x, int32_t y)
{
    int32_t p[3];
    p[0] = x;
    p[1] = y;
    p[2] = tm2_rand() % 120 + 0x12C0;
    world_collapse_debris(env, p);
}

void world_collapse_explosions(const WorldCollapseEnv* env, int32_t s)
{
    int32_t u = grp(env, G_U(s)), v = grp(env, G_V(s));
    int32_t q = u / 4, h = u / 2, hv = v / 2;
    int32_t t = st(env, S_TIMER);
    int32_t rounds = t < 300 ? t / 60 + 1 : 1;
    for (int32_t n = 0; n < rounds; n++) {
        int32_t ox, oy, r, x, y;
        if (link_set(env, s, 0)) {
            r = tm2_rand() % h;
            x = tm2_wadd(tm2_wadd(grp(env, G_X(s)), q), r);
            y = tm2_wadd(grp(env, G_Y(s)), grp(env, G_V(s)));
            debris_at(env, x, y);
        }
        if (link_set(env, s, 1)) {
            r = tm2_rand() % hv;
            int32_t d = tm2_wmul(hv - r, q) / hv;
            ox = grp(env, G_X(s));
            x = tm2_wadd(tm2_wadd(tm2_wadd(ox, h), q), d);
            oy = grp(env, G_Y(s));
            y = tm2_wadd(r, tm2_wadd(oy, hv));
            debris_at(env, x, y);
        }
        if (link_set(env, s, 2)) {
            r = tm2_rand() % hv;
            int32_t d = tm2_wmul(r, q) / hv;
            x = tm2_wadd(tm2_wadd(tm2_wadd(grp(env, G_X(s)), h), q), d);
            y = tm2_wadd(grp(env, G_Y(s)), r);
            debris_at(env, x, y);
        }
        if (link_set(env, s, 3)) {
            r = tm2_rand() % h;
            x = tm2_wadd(tm2_wadd(grp(env, G_X(s)), q), r);
            y = grp(env, G_Y(s));
            debris_at(env, x, y);
        }
        if (link_set(env, s, 4)) {
            r = tm2_rand() % hv;
            int32_t d = tm2_wmul(hv - r, q) / hv;
            x = tm2_wadd(grp(env, G_X(s)), d);
            y = tm2_wadd(grp(env, G_Y(s)), r);
            debris_at(env, x, y);
        }
        if (link_set(env, s, 5)) {
            r = tm2_rand() % hv;
            int32_t d = tm2_wmul(r, q) / hv;
            x = tm2_wadd(grp(env, G_X(s)), d);
            y = tm2_wadd(r, tm2_wadd(grp(env, G_Y(s)), hv));
            debris_at(env, x, y);
        }
    }
}

/* the search for the next section; returns the section that starts, or -1 */
static int32_t collapse_pick(const WorldCollapseEnv* env)
{
    int32_t done = st(env, S_DONE), first, end;
    if (done < 6) { first = 0; end = 6; }
    else if (done < 18) { first = 6; end = 18; }
    else if (done < 25) { first = 18; end = 25; }
    else if (done < 29) { first = 25; end = 29; }
    else { first = 1; end = 0; }
    int32_t picked = -1;
    int hit = 0;
    for (int32_t s = first; s < end && !hit; s++) {
        if (!REC(s)[R_ARMED])
            continue;
        int32_t n = env->car_count(env->user);
        for (int32_t i = 0; i < n && !hit; i++) {
            const uint8_t* car = env->car(env->user, i);
            const uint8_t* body = car + 0x6C4;
            const uint8_t* probes = car + 0x58C;
            if (st(env, S_TIMER) <= 0x30C) {
                if (rd32(probes + 0x26) >> 16 == s && rd32(probes + 0x6E) >> 16 == s &&
                    rd32(probes + 0xB6) >> 16 == s && rd32(probes + 0xFE) >> 16 == s)
                    hit = 1;
            } else {
                int32_t dx = tm2_wadd(tm2_wadd(grp(env, G_U(s)) / 2, grp(env, G_X(s))), -rd32(body + 0x20));
                int32_t dy = tm2_wadd(tm2_wadd(grp(env, G_Y(s)), grp(env, G_V(s)) / 2), -rd32(body + 0x24));
                int32_t a = tm2_wadd(tm2_atan2(dx, dy), -rd32(body + 0x34));
                tm2_angle_wrap32(&a);
                int32_t m = a < 0 ? (int32_t)(0u - (uint32_t)a) : a;
                if (m < 0xE3 || (st(env, S_TIMER) > 0x438 && (tm2_rand() & 3) == 1))
                    hit = 1;
            }
            if (hit) {
                env->state[S_BUSY] = 1;
                st_set(env, S_SECTION, s);
                st_set(env, S_TIMER, 0);
                REC(s)[R_SHAKING] = 1;
                st_set(env, S_BLASTS, 0);
                wr32(REC(s) + R_TIMER, -0x1E0);
                env->section_start(env->user, s);
                env->ai_reroute(env->user, s, rd32(REC(s) + R_REROUTE));
                picked = s;
                world_collapse_unlink(env, s);
            }
        }
    }
    return picked;
}

static void collapse_finish(const WorldCollapseEnv* env, int32_t s)
{
    st_set(env, S_DONE, tm2_wadd(st(env, S_DONE), rd32(REC(s) + R_COUNT)));
    st_set(env, S_TIMER, 0);
    st_set(env, S_BLASTS, 0);
    REC(s)[R_ARMED] = 0;
    env->state[S_BUSY] = 0;
    uint8_t* obj = env->resolve(env->user, rdu32(REC(s) + R_OBJ));
    /* the original also copies a position out of the object here (kind 4: +0x04..+0x0C, kind 5:
     * +0x28, +0x2C, 0) into a stack vector nothing reads */
    if (obj[0] == 4)
        obj[0x12] = 0;
    else if (obj[0] == 5)
        obj[0x36] = 0;
    wr32(env->grp + G_Z(s), -1);
    env->section_sink(env->user, s, -0x50);
    int32_t next = -1;
    if (s == 6) {
        wr32(env->grp + G_Z(7), -1);
        wr32(env->grp + G_Z(8), -1);
        env->section_sink(env->user, 7, -0x50);
        next = 8;
    } else if (s == 13) {
        next = 14;
    } else if (s == 19) {
        next = 20;
    } else if (s == 22) {
        next = 23;
    } else if (s == 26) {
        next = 27;
    }
    if (next >= 0) {
        if (s != 6)
            wr32(env->grp + G_Z(next), -1);
        env->section_sink(env->user, next, -0x50);
    }
    if (env->marker) {
        wr32(env->resolve(env->user, env->marker) + 0x08, 0);
        env->set_step(env->user, 0xC1F, 0, 0);
    }
}

void world_collapse_update(const WorldCollapseEnv* env)
{
    int32_t s = -1;
    st_set(env, S_TIMER, tm2_wadd(st(env, S_TIMER), tm2_frame_ticks60()));
    if (env->state[S_BUSY])
        s = st(env, S_SECTION);
    else if (st(env, S_TIMER) > 0x168)
        s = collapse_pick(env);
    if (s < 0)
        return;
    uint8_t* r = REC(s);
    if (!r[R_FALLING]) {
        if (!r[R_SHAKING])
            return;
        int32_t t = rd32(r + R_TIMER), target;
        if (t < -0x168)
            target = 0x11 - (tm2_rand() & 0x1F);
        else if (t < -0xF0)
            target = 0x1E - (tm2_rand() & 0x3F);
        else if (t < -0x78)
            target = 0x22 - (tm2_rand() & 0x3F);
        else
            target = 0x1F - (tm2_rand() & 0x3F);
        int32_t z = grp(env, G_Z(s));
        if (z > 0x1338)
            target = -0x28;
        else if (z < 0x1290)
            target = 0x28;
        wr32(r + R_DZ, tm2_angle_step(rd32(r + R_DZ), target, 0xB));
        world_collapse_move_section(env, s);
        env->section_move(env->user, s, rd32(r + R_DZ));
        world_collapse_explosions(env, s);
        t = tm2_wadd(rd32(r + R_TIMER), tm2_frame_ticks60());
        wr32(r + R_TIMER, t);
        if (t < 0)
            return;
        r[R_SHAKING] = 0;
        wr32(r + R_DZ, 0);
        r[R_FALLING] = 1;
        return;
    }
    if (st(env, S_TIMER) < 600)
        world_collapse_explosions(env, s);
    int32_t dz;
    if (rd32(r + R_TIMER) > 0) {
        int32_t t = rd32(r + R_TIMER);
        int32_t a = tm2_wmul(t, -2 * tm2_frame_ticks60());
        wr32(r + R_DZ, tm2_wmul(t, a) / 16);
        dz = -1;
    } else {
        wr32(r + R_DZ, 0);
        dz = 0;
    }
    dz = tm2_wadd(dz, rd32(r + R_DZ) / 32);
    if (dz > -32 * tm2_frame_ticks60())
        add32(r + R_TIMER, tm2_frame_ticks60());
    wr32(r + R_DZ, dz);
    world_collapse_move_section(env, s);
    env->section_move(env->user, s, rd32(r + R_DZ));
    if (grp(env, G_Z(s)) >= 0)
        return;
    collapse_finish(env, s);
}
