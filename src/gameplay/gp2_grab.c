/* gp2_grab.c: see gp2_grab.h */
#include "gameplay/gp2_grab.h"

#include "math/tm2math.h"

typedef struct {
    Gp2GrabEnv* env;
    uint8_t human;
    int32_t key, owner;
    uint32_t stats, body;
    uint8_t* claw;
} Grab;

static void sound(Grab* g, int32_t id) {
    Gp2GrabEnv* e = g->env;
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

static int32_t held(const Grab* g) { return w_i16(g->claw, 0x0E); }

static int32_t step(int32_t a, int32_t rate) {
    a = (int32_t)((uint32_t)a + (uint32_t)((int32_t)tm2_frame_ticks60() * rate));
    tm2_angle_wrap32(&a);
    return a;
}

/* the arm read and the spin out of the held car above 0x238, shared by all phases but closing */
static int32_t arm_read(Grab* g) {
    Gp2GrabEnv* e = g->env;
    int32_t arm = e->arm_get(e->user);
    if (arm > 0x238 && g->claw[7])
        e->spin(e->user, held(g), 0, 1);
    return arm;
}

static void release(Grab* g, int32_t front) {
    Gp2GrabEnv* e = g->env;
    uint8_t* b = wmem_ptr(e->mem, g->body, 0x38);
    g->claw[8] = 0;
    g->claw[9] = 1;
    e->release(e->user, held(g), w_i16(b, 0x34), front);
    w_s16(g->claw, 0x0E, 0xFFFF);
    g->claw[7] = 0;
}

static void hit(Grab* g) {
    Gp2GrabEnv* e = g->env;
    uint8_t* c = g->claw;
    int32_t amount = c[0x0B] ? 5 : 10;
    uint32_t obj = e->object_from_owner(e->user, held(g));
    uint8_t kind = 0;
    int32_t idx = 0;
    if (obj != 0 && e->find_by_object(e->user, obj, &kind, &idx) != 0) {
        uint32_t body = kind ? e->human_car(e->user, idx) + 0x6C4 : e->ai_car(e->user, idx) + 0x708;
        int32_t id = e->object_id(e->user, idx, kind);
        e->damage(e->user, id, amount, body + 0x38, 2, g->owner);
    }
    w_s16(c, 0x0C, (uint32_t)(w_i16(c, 0x0C) - 1));
    if (w_i16(c, 0x0C) > 0) {
        c[3] = 1;
        return;
    }
    release(g, 0);
    if (e->rand(e->user) & 1)
        c[0x0A] = 1;
}

void gp2_slam_grab_update(Gp2GrabEnv* e, uint32_t car_a, uint8_t human) {
    uint8_t* car = wmem_ptr(e->mem, car_a, 0x898);
    Grab g;
    g.env = e;
    g.human = human;
    if (human) {
        g.stats = car_a + 0x138;
        g.body = car_a + 0x6C4;
        g.key = w_i16(car, 0x00);
        g.owner = w_i16(car, 0x02);
        g.claw = wmem_ptr(e->mem, w_u32(car, 0x890), 0x10);
    } else {
        g.stats = car_a + 0x4EC;
        g.body = car_a + 0x708;
        g.key = w_i16(car, 0x04);
        g.owner = w_i16(car, 0x06);
        g.claw = wmem_ptr(e->mem, w_u32(car, 0x87C), 0x10);
    }
    uint8_t* c = g.claw;
    if (c[2] == 0)
        return;
    sound(&g, 0x39);

    if (c[6]) {                                   /* closing */
        if (c[7])
            e->carry(e->user, held(&g), g.stats, g.body, 0, 0);
        else
            e->find_target(e->user, car_a, human);
        int claw_done, arm_done;
        int32_t a = e->claw_get(e->user);
        if (a <= -0x2AA) {
            claw_done = 1;
        } else {
            a = step(a, -0x44);
            if (a < -0x2AA)
                a = -0x2AA;
            e->claw_set(e->user, a);
            claw_done = 0;
        }
        a = e->arm_get(e->user);
        if (a < 0x38E) {
            a = step(a, 0x38);
            if (a > 0x38E)
                a = 0x38E;
            e->arm_set(e->user, a);
            arm_done = 0;
        } else {
            arm_done = 1;
        }
        if (!claw_done || !arm_done)
            return;
        c[6] = 0;
        if (c[7]) {
            c[3] = 1;
            c[9] = 0;
            c[0x0A] = 0;
            c[8] = 1;
        } else {
            c[9] = 1;
            c[8] = 0;
        }
        return;
    }

    if (c[8] == 0) {                              /* retract */
        int32_t a = e->claw_get(e->user);
        if (a < 0x2E3)
            e->claw_set(e->user, step(a, 0x2D));
        a = arm_read(&g);
        if (a > -0x71)
            e->arm_set(e->user, step(a, -0x22));
        if (e->claw_get(e->user) < 0x2E3)
            return;
        if (e->arm_get(e->user) > -0x71)
            return;
        e->claw_set(e->user, 0x2E3);
        e->arm_set(e->user, -0x71);
        c[2] = 0;
        c[0] = 0;
        return;
    }

    if (c[3]) {                                   /* lowering after a hit */
        int32_t a = arm_read(&g);
        if (a > 0x71) {
            a = step(a, -0x22);
            if (a < 0x71)
                a = 0x71;
            e->arm_set(e->user, a);
            e->carry(e->user, held(&g), g.stats, g.body, 1, 1);
            return;
        }
        c[3] = 0;
        return;
    }

    if (c[0x0A]) {                                /* release swing */
        int32_t a = arm_read(&g);
        if (a <= 0x71)
            return;
        e->carry(e->user, held(&g), g.stats, g.body, 1, 1);
        a = step(a, -0x22);
        if (a < 0x71) {
            a = 0x71;
            release(&g, 1);
        }
        e->arm_set(e->user, a);
        return;
    }

    int32_t a = arm_read(&g);                     /* lift and slam */
    if (a < 0x38E) {
        e->carry(e->user, held(&g), g.stats, g.body, 1, 0);
        a = step(a, 0x44);
        if (a > 0x38E) {
            a = 0x38E;
            sound(&g, 2);
            if (c[7])
                e->spin(e->user, held(&g), 0, 1);
        }
        e->arm_set(e->user, a);
        return;
    }
    hit(&g);
}
