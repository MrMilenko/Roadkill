/* cb_level.c: see cb_level.h */
#include "combat/cb_level.h"

static uint8_t* gp(const CbLevelEnv* e, uint32_t a, uint32_t n) { return wmem_ptr(e->mem, a, n); }
static uint32_t rd32(const CbLevelEnv* e, uint32_t a) { return w_u32(gp(e, a, 4), 0); }
static void wr32(const CbLevelEnv* e, uint32_t a, uint32_t v) { w_s32(gp(e, a, 4), 0, v); }
static void wr16(const CbLevelEnv* e, uint32_t a, uint32_t v) { w_s16(gp(e, a, 2), 0, v); }
static void wr8(const CbLevelEnv* e, uint32_t a, uint32_t v) { w_s8(gp(e, a, 1), 0, v); }

void cb_stats_preset(uint8_t* s) {
    static const struct { uint8_t off; int16_t v; } p[] = {
        {0x44, 30}, {0x46, 60}, {0x48, 50}, {0x4C, 115}, {0x4E, 40}, {0x50, -40}, {0xD4, -20}, {0xD6, 20},
        {0xD8, 0}, {0xDC, 0}, {0xDE, 0}, {0xE0, 20}, {0x3A, 110}, {0x7A, 50}, {0x42, 50}};
    for (unsigned i = 0; i < sizeof p / sizeof p[0]; i++)
        w_s16(s, p[i].off, (uint16_t)p[i].v);
    w_s32(s, 0xA0, (uint32_t)(w_i32(s, 0x38) >> 16));
}

static int8_t cur(const uint8_t* b) { return w_i8(b, 0x0D); }
static int16_t ammo(const uint8_t* b, int k) { return w_i16(b, 0x0E + 2 * k); }

void cb_weapon_select(uint8_t* b, int32_t dir, int32_t n) {
    if (cur(b) >= 8 || cur(b) < 0)
        b[0x0D] = 0;
    int32_t start = cur(b);
    if (dir == 1) {
        do {
            b[0x0D] = (uint8_t)(b[0x0D] - 1);
            if (cur(b) < 0)
                b[0x0D] = (uint8_t)(n - 1);
        } while (ammo(b, cur(b)) <= 0 && cur(b) != start);
    } else if (dir == -1) {
        do {
            b[0x0D] = (uint8_t)(b[0x0D] + 1);
            if (cur(b) >= n)
                b[0x0D] = 0;
        } while (ammo(b, cur(b)) <= 0 && cur(b) != start);
    } else if (ammo(b, dir - 100) > 0) {
        b[0x0D] = (uint8_t)(dir - 100);
    }
    if (ammo(b, cur(b)) <= 0)
        b[0x0D] = (uint8_t)n;
}

void cb_weapon_objects_reset(const CbLevelEnv* e) {
    for (int k = 0; k < 10; k++)
        wr32(e, 0x004ff8fc + 0x30 * k, 0);
    for (int k = 0; k < 4; k++) {
        wr32(e, 0x004ffad0 + 0xC * k, 0);
        wr32(e, 0x004ffacc + 0xC * k, 0);
    }
    wr32(e, 0x004ffe1c, 0);
    wr32(e, 0x004ffe30, 0);
    wr32(e, 0x004ffed0, 0);
    wr32(e, 0x004ffed4, 0);
    wr32(e, 0x004ffe18, 0);
    for (int k = 0; k < 25; k++)
        wr32(e, 0x004ffb74 + 0x1C * k, 0);
    for (int k = 0; k < 23; k++)
        wr32(e, 0x004ffaf8 + 4 * k, 0);
    for (int k = 0; k < 2; k++)
        wr32(e, 0x004ffb54 + 4 * k, 0);
}

void cb_weapon_objects_detach(const CbLevelEnv* e) {
    for (int k = 0; k < 25; k++) {
        uint32_t r = 0x004ffb5c + 0x1C * k;
        wr16(e, r + 8, 0);
        wr8(e, r, 0);
        wr8(e, r + 1, 0);
        uint32_t o = rd32(e, r + 0x18);
        if (o) {
            wr32(e, o + 8, 0);
            wr32(e, r + 0x18, 0);
        }
    }
    for (int k = 0; k < 23; k++) {
        uint32_t o = rd32(e, 0x004ffaf8 + 4 * k);
        if (o)
            wr32(e, o + 8, 0);
    }
    for (int k = 0; k < 2; k++) {
        uint32_t o = rd32(e, 0x004ffb54 + 4 * k);
        if (o)
            wr32(e, o + 8, 0);
    }
}

void cb_level_entities_init(const CbLevelEnv* e) {
    for (int k = 0; k < 50; k++)
        wr16(e, 0x004fefbe + 0x10 * k, 0);
    wr32(e, 0x004ff2dc, 0);
    for (int k = 0; k < 10; k++)
        wr32(e, 0x004ff2e0 + 4 * k, 2);
    wr32(e, 0x004ff2ec, 1);
}

void cb_level_entity_add(const CbLevelEnv* e, uint32_t pos, int16_t kind, int32_t i) {
    if (i > 50)
        return;
    uint32_t r = 0x004fefbc + (uint32_t)i * 0x10;
    if (w_i16(gp(e, r + 2, 2), 0) != 0)
        return;
    wr16(e, r + 2, 1);
    /* the original's order: pos may lie inside the table */
    wr32(e, r + 4, rd32(e, pos));
    wr32(e, r + 8, rd32(e, pos + 4));
    uint32_t z = rd32(e, pos + 8);
    wr16(e, r, (uint16_t)kind);
    uint32_t c = rd32(e, 0x004ff2dc) + 1;   /* read after the kind store (i = 50 overlaps it) */
    wr32(e, r + 0x0C, z);
    wr32(e, 0x004ff2dc, c);
}

int32_t cb_level_entity_kinds(void) { return 15; }

void cb_level_entity_show(const CbLevelEnv* e, int32_t id, int32_t slot) {
    e->pickup_set_visible(e->user, id, slot, 1);
    wr16(e, 0x004fefbe + ((uint32_t)slot << 4), 1);
}

void cb_level_entity_hide(const CbLevelEnv* e, int32_t id, int32_t slot) {
    e->pickup_set_visible(e->user, id, slot, 0);
    wr16(e, 0x004fefbe + ((uint32_t)slot << 4), 0);
}

uint32_t cb_target_object_field(const CbLevelEnv* e) { return rd32(e, rd32(e, 0x004ffed4) + 0x15C); }
uint32_t cb_target_object(const CbLevelEnv* e) { return rd32(e, 0x004ffed4); }

int32_t cb_pickup_counter_take(const CbLevelEnv* e, uint32_t i) {
    uint32_t a = 0x004ffedc + 4 * i;
    int take = (i >= 0x12 && i <= 0x13) || (i >= 0x16 && i <= 0x1E);
    if (i == 0x22 || i > 0x26)
        return 0;
    int32_t v = (int32_t)rd32(e, a);
    if (take)
        wr32(e, a, 0);
    return v;
}

void cb_combo_block_init(uint8_t* b, uint32_t id) {
    w_s32(b, 0, id);
    for (int k = 0; k < 4; k++) {
        w_s32(b, 0x14 + 4 * k, 0);
        w_s32(b, 0x04 + 4 * k, 0);
        b[0x28 + k] = 0;
    }
    w_s32(b, 0x2C, 0);
    b[0x54] = 0;
    w_s32(b, 0x30 + 4 * (int32_t)w_u32(b, 0x2C), 0xFFFFFFFFu);
}

void cb_fx_counter_clear(const CbLevelEnv* e) { wr32(e, 0x00500014, 0); }

void cb_nothing(void) {}

uint32_t cb_render_mode_set(const CbLevelEnv* e, uint32_t mode) {
    static const uint8_t v[7] = {0xED, 0xF6, 0xF0, 0xF5, 0xF5, 0xF9, 0xEE};
    if (mode <= 6)
        wr8(e, 0x00502818 + mode, v[mode]);
    return mode;
}

void cb_world_objects_clear(const CbLevelEnv* e) {
    static const uint32_t singles[] = {
        0x00502838, 0x0050283c, 0x00502840, 0x0050284c, 0x00502850, 0x00502854, 0x00502858, 0x0050285c,
        0x00502860, 0x00502864, 0x00502868, 0x0050286c, 0x00502870, 0x00502844, 0x00502848, 0x00502874,
        0x00502878, 0x0050287c, 0x00502880, 0x00502834};
    for (unsigned k = 0; k < sizeof singles / sizeof singles[0]; k++)
        wr32(e, singles[k], 0);
    for (int k = 0; k < 15; k++) {
        wr32(e, 0x0050294c + 4 * k, 0);
        wr32(e, 0x00502910 + 4 * k, 0);
    }
    wr32(e, 0x00502e94, 0);
    wr32(e, 0x00502e98, 11);
    wr32(e, 0x00502e9c, 11);
    wr32(e, 0x00502f1c, 0);
    wr32(e, 0x00502f60, 0);
    wr32(e, 0x00502e90, 0);
    for (int k = 0; k < 5; k++) {
        wr32(e, 0x00502f34 + 4 * k, 0);
        wr32(e, 0x00502f48 + 4 * k, 0);
        wr32(e, 0x00502f64 + 4 * k, 0);
        wr32(e, 0x00502f20 + 4 * k, 0);
    }
    wr32(e, 0x004e7418, 1);
    wr32(e, 0x004e741c, 1);
    wr32(e, 0x004e7414, 1);
    for (int k = 0; k < 10; k++) {
        wr32(e, 0x005028c4 + 8 * k, 0);
        wr32(e, 0x005028c0 + 8 * k, 0);
    }
}

void cb_obj5_register(const CbLevelEnv* e, int which, uint32_t obj) {
    static const uint32_t base[CB_OBJ5_COUNT] = {0x00502830, 0x00502858, 0x00502848, 0x00502870, 0x00502840};
    static const uint8_t max[CB_OBJ5_COUNT] = {4, 6, 4, 4, 2};
    if (!obj || *gp(e, obj, 1) != 5)
        return;
    uint32_t i = w_u16(gp(e, obj + 0x34, 2), 0);
    if (i != 0 && i <= max[which])
        wr32(e, base[which] + 4 * i, obj);
}

void cb_light_flicker_set(const CbLevelEnv* e, int32_t key, uint32_t value) {
    for (int k = 0; k < 10; k++) {
        if ((int32_t)rd32(e, 0x005028c4 + 8 * k) == key) {
            wr32(e, 0x005028c0 + 8 * k, value);
            return;
        }
    }
    for (int k = 0; k < 10; k++) {
        if (rd32(e, 0x005028c0 + 8 * k) == 0) {
            wr32(e, 0x005028c0 + 8 * k, value);
            wr32(e, 0x005028c4 + 8 * k, (uint32_t)key);
            return;
        }
    }
}

static uint32_t body_slot(int32_t id) { return 0x00502910 + 4 * (uint32_t)((id >> 4) - 1); }

void cb_car_body_show(const CbLevelEnv* e, int32_t id) {
    uint32_t o = rd32(e, body_slot(id));
    if (o && *gp(e, o, 1) == 5) {
        e->model_remove(e->user, o);
        e->model_add(e->user, rd32(e, body_slot(id)));
    }
}

void cb_car_body_hide(const CbLevelEnv* e, int32_t id) {
    uint32_t o = rd32(e, body_slot(id));
    if (o && *gp(e, o, 1) == 5)
        e->model_remove(e->user, o);
}

void cb_slam_arm_register(const CbLevelEnv* e, uint32_t obj) {
    if (!obj || *gp(e, obj, 1) != 5)
        return;
    wr32(e, 0x00502e90, obj);
    wr32(e, 0x00502e98, 0xFFFFFF8Fu);
    e->rot_x(e->user, -0x71, obj + 4);
}

void cb_car_object_register(const CbLevelEnv* e, int32_t id, uint32_t obj) {
    if (*gp(e, obj, 1) != 1)
        return;
    int32_t i = (id >> 4) - 1;
    if (i < 15)
        wr32(e, 0x0050294c + 4 * (uint32_t)i, obj);
}

uint32_t cb_car_object_get(const CbLevelEnv* e, int32_t id) {
    return rd32(e, 0x0050294c + 4 * (uint32_t)((id >> 4) - 1));
}

int32_t cb_platform_height(const CbLevelEnv* e, int32_t i) {
    uint32_t ent = rd32(e, rd32(e, 0x00502f18) + (uint32_t)i * 0x38 + 0x34);
    return (int32_t)rd32(e, ent + 0xE2) >> 16;
}

void cb_world_set_502f5c(const CbLevelEnv* e, uint32_t v) { wr32(e, 0x00502f5c, v); }

void cb_obj4_register(const CbLevelEnv* e, int which, uint32_t obj) {
    static const uint32_t base[CB_OBJ4_COUNT] = {0x00502f20, 0x00502f34, 0x00502f48};
    uint32_t i = w_u16(gp(e, obj + 0x10, 2), 0);
    if (i <= 4 && *gp(e, obj, 1) == 4)
        wr32(e, base[which] + 4 * i, obj);
}

void cb_target_register(const CbLevelEnv* e, uint32_t obj) {
    wr32(e, 0x00502f60, obj);
    wr32(e, obj + 8, 1);
}

void cb_pool_object_register(const CbLevelEnv* e, uint32_t obj, int32_t i) {
    wr32(e, 0x00502f64 + 4 * (uint32_t)i, obj);
    wr32(e, obj + 8, 0);
}

uint32_t cb_pool_object_free(const CbLevelEnv* e) {
    for (int k = 0; k < 5; k++) {
        uint32_t o = rd32(e, 0x00502f64 + 4 * k);
        if (o && rd32(e, o + 8) == 0)
            return o;
    }
    return 0;
}

void cb_world_set_502f80(const CbLevelEnv* e, uint8_t v, int32_t i) { wr8(e, 0x00502f80 + (uint32_t)i, v); }

static uint32_t plat_slot(const CbLevelEnv* e, int32_t i) { return rd32(e, 0x00502f18) + (uint32_t)i * 0x38; }

uint32_t cb_platform_target_pos(const CbLevelEnv* e, int32_t i) { return rd32(e, plat_slot(e, i) + 0x34) + 0xE8; }
uint32_t cb_platform_step_vec(const CbLevelEnv* e, int32_t i) { return plat_slot(e, i) + 0x28; }
uint32_t cb_platform_step_angle(const CbLevelEnv* e, int32_t i) { return rd32(e, plat_slot(e, i) + 8); }

static void slam_rotate(const CbLevelEnv* e, uint32_t objp, uint32_t anglep, int32_t a) {
    if (rd32(e, objp) == 0)
        return;
    wr32(e, anglep, (uint32_t)a);
    int16_t ang[3] = {(int16_t)w_u16(gp(e, anglep, 2), 0), 0, 0};
    e->build_matrix(e->user, ang, rd32(e, objp) + 4);
}

void cb_slam_arm_rotate(const CbLevelEnv* e, int32_t a) { slam_rotate(e, 0x00502e90, 0x00502e98, a); }
void cb_slam_claw_rotate(const CbLevelEnv* e, int32_t a) { slam_rotate(e, 0x00502e94, 0x00502e9c, a); }

uint32_t cb_combo_taps(uint32_t block) { return block + 0x30; }
uint32_t cb_combo_count(const CbLevelEnv* e, uint32_t block) { return rd32(e, block + 0x2C); }
