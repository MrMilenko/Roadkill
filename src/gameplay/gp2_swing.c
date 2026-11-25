/* gp2_swing.c: see gp2_swing.h */
#include "gameplay/gp2_swing.h"

#include "math/tm2math.h"

typedef struct {
    Gp2SwingEnv* env;
    uint8_t human;
    int32_t key, owner;
    uint32_t car, stats, body;
    uint8_t* claw;
} Swing;

static void sound(Swing* g, int32_t id) {
    Gp2SwingEnv* e = g->env;
    int32_t pan = 0, vol = 0;
    if (!g->human || g->key != 0) {
        if (e->sound_mode(e->user) == 0)
            e->sound_locate0(e->user, g->body + 0x20);
        else
            e->sound_locate1(e->user, g->body + 0x20, -1);
        vol = e->sound_vol(e->user);
        pan = e->sound_pan(e->user);
    }
    e->sound_play(e->user, id, pan, vol, 0x63);
}

static int32_t held(const Swing* g) { return w_i16(g->claw, 0x0E); }

static int32_t step(int32_t a, int32_t rate) {
    a = (int32_t)((uint32_t)a + (uint32_t)((int32_t)tm2_frame_ticks60() * rate));
    tm2_angle_wrap32(&a);
    return a;
}

/* the held car takes amount (owner as the stack argument) */
static void hit_held(Swing* g, int32_t amount) {
    Gp2SwingEnv* e = g->env;
    uint32_t obj = e->object_from_owner(e->user, held(g));
    uint8_t kind = 0;
    int32_t idx = 0;
    if (obj == 0 || e->find_by_object(e->user, obj, &kind, &idx) == 0)
        return;
    uint32_t body = kind ? e->human_car(e->user, idx) + 0x6C4 : e->ai_car(e->user, idx) + 0x708;
    int32_t id = e->object_id(e->user, idx, kind);
    e->damage(e->user, id, amount, body + 0x38, 2, g->owner);
}

static int32_t iabs(int32_t v) {
    uint32_t m = (uint32_t)(v >> 31);
    return (int32_t)(((uint32_t)v ^ m) - m);
}

/* largest axis distance (32 bit wrap, |INT_MIN| stays negative) >> 3 under 15 */
static int near_tip(const int32_t t[3], const uint8_t* p) {
    int32_t ax = iabs((int32_t)((uint32_t)t[0] - w_u32(p, 0)));
    int32_t ay = iabs((int32_t)((uint32_t)t[1] - w_u32(p, 4)));
    int32_t az = iabs((int32_t)((uint32_t)t[2] - w_u32(p, 8)));
    int32_t m = ay >= az ? ay : az;
    int32_t d = ax >= m ? ax : m;
    return (d >> 3) < 0xF;
}

static void tip_hits(Swing* g) {
    Gp2SwingEnv* e = g->env;
    const uint8_t* st = wmem_ptr(e->mem, g->stats, 0x50);
    const uint8_t* b = wmem_ptr(e->mem, g->body, 0x74);
    int32_t m[9], v[3] = {0, 0, 0}, t[3];
    for (int i = 0; i < 9; i++)
        m[i] = w_i32(b, 0x50 + 4 * i);
    v[1] = w_i16(st, 0x4E) + 0x78;
    tm2_mat_vec_mul_row(m, v, t);
    for (int i = 0; i < 3; i++)
        t[i] = (int32_t)((uint32_t)t[i] + w_u32(b, 0x20 + 4 * i));
    uint32_t pool = e->pool_get(e->user);
    for (int i = 0; i < 50; i++) {
        const uint8_t* p = wmem_ptr(e->mem, pool + 0x14 * i, 0x14);
        if (w_i16(p, 4) <= 0)
            continue;
        if (near_tip(t, p + 8))
            e->pool_hit(e->user, w_i16(p, 0), w_i16(p, 2), 0x19);
    }
    uint32_t act = e->actors_get(e->user);
    for (int i = 0; i < 6; i++) {
        const uint8_t* a = wmem_ptr(e->mem, act + 0x4C * i, 0x4C);
        if (w_i16(a, 4) == -1)
            continue;
        const uint8_t* o = wmem_ptr(e->mem, w_u32(a, 0x48) + 0xE8, 12);
        if (near_tip(t, o)) {
            static const int32_t dir[3] = {0, 0, 0xFFF};   /* 0x004e66e8 */
            int32_t d[3] = {dir[0], dir[1], dir[2]};
            e->actor_hit(e->user, i, d);
        }
    }
}

void gp2_slam_swing_update(Gp2SwingEnv* e, uint32_t car_a, uint8_t human) {
    uint8_t* car = wmem_ptr(e->mem, car_a, 0x898);
    Swing g;
    const uint8_t *flags, *misc;
    g.env = e;
    g.human = human;
    g.car = car_a;
    if (human) {
        g.stats = car_a + 0x138;
        g.body = car_a + 0x6C4;
        flags = car + 0x9A;
        misc = car + 0xCC;
        g.key = w_i16(car, 0x00);
        g.owner = w_i16(car, 0x02);
        g.claw = wmem_ptr(e->mem, w_u32(car, 0x890), 0x12);
    } else {
        g.stats = car_a + 0x4EC;
        g.body = car_a + 0x708;
        flags = car + 0x68;
        misc = car + 0x40;
        g.key = w_i16(car, 0x04);
        g.owner = w_i16(car, 0x06);
        g.claw = wmem_ptr(e->mem, w_u32(car, 0x87C), 0x12);
    }
    uint8_t* c = g.claw;
    if (c[1] == 0)
        return;
    if (flags[0x0D]) {                                 /* stunned */
        if (c[7])
            e->carry(e->user, held(&g), g.stats, g.body, 0, 0);
        return;
    }
    if (c[3]) {                                        /* lowering */
        sound(&g, 0x39);
        int32_t a = e->claw_get(e->user);
        if (a <= -0x2AA) {
            c[5] = 1;
            c[3] = 0;
            return;
        }
        a = step(a, -0x22);
        if (a < -0x2AA)
            a = -0x2AA;
        e->claw_set(e->user, a);
        e->arm_set(e->user, step(e->arm_get(e->user), -5));
        return;
    }
    if (c[5]) {                                        /* holding down */
        if (misc[0x1F] == 0) {
            c[4] = 1;
            c[5] = 0;
            return;
        }
        w_s16(c, 0x10, (uint32_t)(w_u16(c, 0x10) + (uint16_t)tm2_frame_ticks60()));
        if (w_i16(c, 0x10) > 0x258) {
            c[4] = 1;
            c[5] = 0;
        }
        return;
    }
    if (c[4]) {                                        /* swinging up */
        int32_t a = step(e->arm_get(e->user), 0x4F);
        if (a > 0x38E) {
            a = 0x38E;
            c[4] = 0;
            if (c[7]) {
                e->spin(e->user, held(&g), 1, 1);
                hit_held(&g, c[0x0B] ? 6 : 0x0C);
                e->spin(e->user, 0xC0, 1, 1);
            } else {
                sound(&g, 5);
                e->spin(e->user, 0xC0, 1, 0);
            }
            tip_hits(&g);
        } else if (a > 0x238) {
            if (c[7]) {
                e->carry(e->user, held(&g), g.stats, g.body, 0, 0);
            } else if (e->find_target(e->user, car_a, human)) {
                c[4] = 0;
                e->spin(e->user, held(&g), 1, 1);
                hit_held(&g, c[0x0B] ? 0x0C : 0x19);
                e->spin(e->user, 0xC0, 1, 1);
            }
        }
        if (c[7])
            e->spin(e->user, held(&g), 0, 1);
        e->arm_set(e->user, a);
        return;
    }

    /* retract */
    int32_t a = e->claw_get(e->user);
    if (a < 0x2E3)
        a = step(a, 0x2D);
    e->claw_set(e->user, a);
    a = e->arm_get(e->user);
    if (a > -0x71) {
        a = step(a, -0x22);
    } else if (!c[7]) {
        if (e->find_target(e->user, car_a, human))
            e->spin(e->user, held(&g), 0, 1);
    }
    e->arm_set(e->user, a);
    if (e->claw_get(e->user) < 0x2E3)
        return;
    if (e->arm_get(e->user) > -0x71)
        return;
    e->claw_set(e->user, 0x2E3);
    e->arm_set(e->user, -0x71);
    uint8_t* k = wmem_ptr(e->mem, w_u32(car, human ? 0x890 : 0x87C), 0x12);
    k[1] = 0;
    if (k[7]) {
        k[6] = 1;
        k[2] = 1;
        return;
    }
    k[0] = 0;
    e->release(e->user, w_i16(k, 0x0E), w_i16(wmem_ptr(e->mem, g.body, 0x38), 0x34), 0);
    w_s16(k, 0x0E, 0xFFFF);
}
