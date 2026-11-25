/*
 * sh_registry.c: object registries and the target pool (see sh_registry.h).
 */
#include "sh_registry.h"

#define REG_CHOICE      0x0062119cu
#define REG_SCENE       0x006211dcu
#define REG_ID6         0x0062129cu
#define REG_ID7         0x006212dcu
#define REG_PICKUP      0x00621388u
#define REG_TARGET      0x00621450u
#define REG_CAR_REC     0x00621640u
#define REG_TEMPLATE    0x006226c0u
#define TGT_POOL        0x006226c8u
#define TGT_ACTORS      0x00622b58u

static void step(const ShEnv* e, uint32_t node_slot, uint32_t s) {
    if (e->anim_step) e->anim_step(e->user, sh_r32(e, node_slot), s);
}

void sh_registry_set_step(const ShEnv* e, int32_t id, int32_t index, uint32_t s) {
    uint32_t ix = (uint32_t)index * 4u;          /* [esi*4 + table], 32 bit wrap */
    if (id < 400) {
        uint32_t g = (uint32_t)(id >> 4) * 4u;
        if ((int32_t)((uint32_t)id - 6u) % 16 == 0) step(e, REG_ID6 + g, s);
        else if ((int32_t)((uint32_t)id - 7u) % 16 == 0) step(e, REG_ID7 + g, s);
    }
    uint32_t u = (uint32_t)id;                   /* the id chain compares unsigned */
    if (u < 0x2ae) {
        if (u < 0x202) {
            if (u < 0x1fa) {
                if (u < 0xc) return;
                if (u == 0xc) { step(e, 0x00621634u, s); return; }
                if (u >= 0x1f4) step(e, REG_TARGET + ix, s);
                return;
            }
            if (u <= 0x1fe) step(e, REG_TARGET + ix, s);   /* 0x1fa..0x1fe */
            return;
        }
        if (u == 0x202) { step(e, 0x00621324u + ix, s); return; }
        if (u < 0x208) {
            if (u < 0x204) return;
            if (u <= 0x205) step(e, 0x00621324u + ix, s);
            else if (u == 0x207) step(e, REG_TARGET + ix, s);
            return;
        }
        if (u == 0x208) { step(e, REG_TARGET + ix, s); return; }
        if (u < 0x29d) return;
        if (u == 0x29d) step(e, 0x00621384u, s);
        else if (u == 0x2ac) step(e, 0x00621348u + ix, s);
        return;
    }
    if (u == 0x2ae) { step(e, 0x00621370u + ix, s); return; }
    if (u < 0xbbc) {
        if (u < 0x914) {
            if (u < 0x2b1) return;
            if (u == 0x2b1) step(e, 0x00621340u, s);
            else if (u == 0x2b3) step(e, 0x0062135cu + ix, s);
            return;
        }
        if (u == 0x914) step(e, 0x0062131cu, s);
        else if (u == 0x915) step(e, 0x00621320u, s);
        else if (u == 0xbbb) step(e, 0x00621608u + ix, s);
        return;
    }
    if (u == 0xbbc) { step(e, 0x0062161cu, s); return; }
    if (u < 0xc1f) {
        if (u == 0xbbd) step(e, 0x00621620u, s);
        else if (u == 0xc1d) step(e, 0x00621624u, s);
        return;
    }
    if (u == 0xc1f) { step(e, 0x00621344u, s); return; }
    if (u < 0xfa0) {
        if (u == 0xc80) step(e, 0x00621628u, s);
        return;
    }
    if (u == 0xfa0) step(e, 0x006215e0u + ix, s);
    else if (u == 0xfa5) step(e, 0x006215f4u + ix, s);
}

void sh_registry_choice_register(const ShEnv* e, uint32_t obj, int32_t id) {
    uint32_t n = sh_r32(e, obj + 0x15c);
    if (!n || sh_r8(e, n) != 1) return;
    n = sh_r32(e, n + 0x18);
    if (!n || sh_r8(e, n) != 9) return;
    int32_t g = id >> 4;
    if (g > 15) return;
    sh_w32(e, REG_CHOICE + (uint32_t)g * 4u, n);
    sh_w8(e, n + 7, 0);
}

void sh_registry_choice_state(const ShEnv* e, int32_t id, uint32_t state) {
    if (state == 2) state = 1;
    if (state == 3) state = 2;
    int32_t g = id >> 4;
    if (g > 15) return;
    uint32_t n = sh_r32(e, REG_CHOICE + (uint32_t)g * 4u);
    if (!n) return;
    if (state < sh_r8(e, n + 6)) sh_w8(e, n + 7, state);
}

void sh_registry_scene_choice_add(const ShEnv* e, uint32_t node) {
    if (!node || sh_r8(e, node) != 9) return;
    int32_t g = sh_r16(e, node + 2) >> 4;
    if (g > 15) return;
    int32_t slot = (int32_t)sh_r16(e, node + 4) - 1;
    if (slot < 0 || slot >= 3) return;
    sh_w32(e, REG_SCENE + (uint32_t)(g * 3 + slot) * 4u, node);
}

void sh_registry_pickup_add(const ShEnv* e, uint32_t obj, int32_t id) {
    if (id < 0x191 || id > 0x1ae) return;
    uint32_t k;
    uint8_t t = sh_r8(e, obj);
    if (t == 1) k = sh_r16(e, obj + 0x16);
    else if (t == 4) k = sh_r16(e, obj + 0x10);
    else return;
    if (k < 0x32) sh_w32(e, REG_PICKUP + 4u * k, obj);
}

static void clear(const ShEnv* e, uint32_t a, int n) {
    for (int k = 0; k < n; k++) sh_w32(e, a + 4u * (uint32_t)k, 0);
}

void sh_registries_reset(const ShEnv* e) {
    clear(e, REG_ID6, 16);
    clear(e, REG_ID7, 16);
    clear(e, REG_CHOICE, 16);
    clear(e, REG_SCENE, 48);
    sh_w32(e, REG_TEMPLATE, 0);
    clear(e, REG_PICKUP, 50);
    clear(e, 0x00621518u, 50);
    clear(e, REG_TARGET, 50);
    static const uint32_t five[] = {0x006215f4u, 0x00621608u, 0x00621348u, 0x0062135cu, 0x00621370u, 0x006215e0u};
    for (int k = 0; k < 6; k++) clear(e, five[k], 5);
    sh_w32(e, 0x00621344u, 0);
    clear(e, 0x00621324u, 7);
    static const uint32_t one[] = {0x00621620u, 0x00621340u, 0x00621624u, 0x00621628u, 0x0062162cu,
                                   0x00621630u, 0x00621634u, 0x00621638u, 0x0062163cu, 0x0062161cu};
    for (int k = 0; k < 10; k++) sh_w32(e, one[k], 0);
}

void sh_registry_set_car_template(const ShEnv* e, uint32_t obj) { sh_w32(e, REG_TEMPLATE, obj); }

static void copy_words(const ShEnv* e, uint32_t d, uint32_t s, int n) {   /* 0x0048ce88 */
    for (int k = 0; k < n; k++) sh_w32(e, d + 4u * (uint32_t)k, sh_r32(e, s + 4u * (uint32_t)k));
}

/* the type 3 node under a type 1 node at n (n itself when its +0x18 child is type 3), or 0 */
static uint32_t anim_node(const ShEnv* e, uint32_t n) {
    if (sh_r8(e, n) != 1) return 0;
    uint32_t m = sh_r32(e, n + 0x18);
    if (sh_r8(e, m) == 3) m = n;
    if (sh_r8(e, m) != 1) return 0;
    m = sh_r32(e, m + 0x18);
    return sh_r8(e, m) == 3 ? m : 0;
}

static void copy_anim(const ShEnv* e, uint32_t rec, uint32_t dst, uint32_t src, uint8_t flag, uint32_t slot) {
    copy_words(e, dst, src, 9);
    sh_w8(e, dst + 0x13, flag);
    uint8_t cnt = sh_r8(e, src + 0xb);
    sh_w32(e, dst + 0x1c, rec);
    sh_w8(e, dst + 5, (uint8_t)(cnt - 1));
    for (uint32_t k = 0; k < sh_r8(e, src + 0xb); k++)   /* count reread every step, as the original */
        sh_w32(e, dst + 0x20 + 4 * k, sh_r32(e, src + 0x20 + 4 * k));
    sh_w32(e, rec + slot, dst);
}

void sh_car_choice_build(const ShEnv* e, int32_t id, uint32_t obj) {
    int32_t g = id >> 4;
    if (!obj || g > 15) return;
    uint32_t t = sh_r32(e, REG_TEMPLATE);
    if (!t) return;
    uint32_t rec = REG_CAR_REC + (uint32_t)g * 0x108u;
    copy_words(e, rec, t, 5);
    sh_w32(e, obj + 0x20, rec);
    uint32_t a = anim_node(e, sh_r32(e, t + 0x14));
    if (a) copy_anim(e, rec, rec + 0x20, a, 1, 0x14);
    sh_w32(e, rec + 0x18, sh_r32(e, t + 0x18));
    a = anim_node(e, sh_r32(e, t + 0x1c));
    if (a) copy_anim(e, rec, rec + 0x94, a, 0, 0x1c);
}

uint32_t sh_target_object(const ShEnv* e, int32_t i) { return sh_r32(e, REG_TARGET + (uint32_t)i * 4u); }

void sh_target_actors_reset(const ShEnv* e) {
    for (uint32_t k = 1; k <= 6; k++) {
        sh_w16(e, 0x00622b10u + 0x4cu * k, 0xffff);
        sh_w32(e, 0x00622b54u + 0x4cu * k, 0);
    }
    sh_w32(e, 0x00622d20u, 0);
    sh_w32(e, 0x00622b54u, 0);
}

void sh_target_pool_reset(const ShEnv* e) {
    for (uint32_t k = 0; k < 50; k++) sh_w16(e, TGT_POOL + 4 + 0x14u * k, 0xffff);
    sh_target_actors_reset(e);
}

void sh_target_register(const ShEnv* e, int32_t type, int32_t instance, const int32_t pos[3]) {
    int32_t i;
    for (i = 0; i < 50; i++)
        if ((int16_t)sh_r16(e, TGT_POOL + 0x14u * (uint32_t)i + 4) < 0) break;
    if (i == 50) {
        if (e->target_error) e->target_error(e->user, type, instance);
        return;
    }
    uint32_t t = TGT_POOL + 0x14u * (uint32_t)i;
    sh_w16(e, t, (uint32_t)type);
    sh_w16(e, t + 2, (uint16_t)instance);
    sh_w32(e, t + 8, (uint32_t)pos[0]);
    sh_w32(e, t + 0xc, (uint32_t)pos[1]);
    sh_w8(e, t + 6, 0);
    sh_w32(e, t + 0x10, (uint32_t)pos[2]);
    sh_w16(e, t + 4, 1);
    uint16_t kind;
    switch (type) {
    case 0x1f4: case 0x208: kind = 1; break;
    case 0x1f5: kind = 2; break;
    case 0x1f6: kind = 4; break;
    case 0x1f7: kind = 8; break;
    case 0x1f8: case 0x1fc: case 0x1fd: case 0x207:
        kind = 0xc;
        if (type == 0x1fc) sh_w16(e, 0x00622ab2u, (uint16_t)instance);
        if (type == 0x207) sh_w16(e, 0x00622adeu, (uint16_t)instance);
        if (type == 0x1fd) sh_w16(e, 0x00622ae0u, (uint16_t)instance);
        break;
    case 0x1f9: kind = 0x12; break;
    case 0x1fa: sh_w32(e, 0x00622b4cu, (uint32_t)i); kind = 0x19; break;
    case 0x1fb: sh_w32(e, 0x00622b50u, (uint32_t)i); kind = 0x19; break;
    case 0x1fe: kind = 0xf; break;
    default: return;
    }
    sh_w8(e, t + 6, 1);
    sh_w16(e, t + 4, kind);
}

uint32_t sh_target_pool(void) { return TGT_POOL; }

void sh_target_track_pos(const ShEnv* e, int32_t type, const int32_t pos[3]) {
    for (int w = 0; w < 2; w++) {
        if (type != (w ? 0x1fb : 0x1fa)) continue;
        uint32_t t = 0x006226d0u + 0x14u * sh_r32(e, w ? 0x00622b50u : 0x00622b4cu);
        sh_w32(e, t, (uint32_t)pos[0]);
        sh_w32(e, t + 4, (uint32_t)pos[1]);
        sh_w32(e, t + 8, (uint32_t)pos[2]);
    }
}
