/*
 * sh_target.c: target actors, level / player event records (see sh_target.h).
 */
#include "sh_target.h"
#include "sh_registry.h"
#include "../core/tm2rand.h"
#include "../math/tm2math.h"

void sh_target_actor_spawn(const ShEnv* e, int32_t id, int16_t instance, uint32_t node) {
    int32_t n = (int32_t)sh_r32(e, 0x00622b54u);
    if (n >= 6 || sh_r8(e, node) != 9) return;
    uint32_t a = 0x00622b58u + 0x4cu * (uint32_t)n;
    uint32_t ent = e->entity_alloc ? e->entity_alloc(e->user, n) : 0;
    sh_w32(e, a + 0x48, ent);
    if (!ent) return;
    sh_w16(e, a + 0xa, 0);
    sh_w16(e, a + 4, (uint32_t)id);
    sh_w16(e, a + 6, (uint16_t)instance);
    if (e->entity_init) e->entity_init(e->user, sh_r32(e, a + 0x48), node);
    if (e->entity_insert) e->entity_insert(e->user, sh_r32(e, a + 0x48));
    sh_w32(e, sh_r32(e, a + 0x48) + 8, 1);
    sh_w32(e, sh_r32(e, a + 0x48) + 0xc, 0);
    sh_w32(e, sh_r32(e, a + 0x48) + 0x158, sh_r32(e, 0x00622b54u) + 6000u);
    sh_w8(e, a + 2, 0);
    if (e->actor_place) e->actor_place(e->user, a);
    sh_registry_set_step(e, id, instance, 1);
    sh_w32(e, 0x00622b54u, sh_r32(e, 0x00622b54u) + 1);
}

void sh_target_level_event_init(const ShEnv* e) {
    sh_w8(e, 0x00622addu, 0);
    sh_w16(e, 0x00622ae4u, 0x14);
    sh_w16(e, 0x00622ae6u, 0x50);
    sh_w8(e, 0x00622adcu, 0);
    sh_w16(e, 0x00622ae2u, 0);
    sh_w32(e, 0x00622aecu, 0x4d50);
    sh_w32(e, 0x00622af0u, 0x15e0);
    sh_w32(e, 0x00622ae8u, 0x3ff8);
}

static int16_t drift(void) {
    int32_t r = (tm2_rand() & 1) ? tm2_rand() : -tm2_rand();
    return (int16_t)(r / 8);
}

int32_t sh_target_player_event_init(const ShEnv* e, uint32_t obj, int32_t player) {
    int32_t b = player - 1;
    uint32_t p = 0x00622af4u + 0x2cu * (uint32_t)b;
    sh_w8(e, p, 0);
    sh_w16(e, p + 6, 0x14);
    sh_w16(e, p + 8, 0x32);
    sh_w32(e, p + 0xc, obj);
    sh_w8(e, p + 1, 0);
    sh_w16(e, p + 4, 0);
    sh_w16(e, p + 2, sh_r16(e, obj + 4));
    if (b == 1) {
        sh_w32(e, p + 0x14, 0xee8);
        sh_w32(e, p + 0x18, 0x2a8);
        sh_w32(e, p + 0x10, 0xf40);
    } else {
        sh_w32(e, p + 0x14, 0xfffff008u);
        sh_w32(e, p + 0x18, 0x2a8);
        sh_w32(e, p + 0x10, 0xf70);
    }
    sh_w16(e, p + 0x1c, (uint16_t)drift());
    sh_w16(e, p + 0x1e, (uint16_t)drift());
    sh_w16(e, p + 0x20, 0xfff);
    sh_w16(e, p + 0x24, 0);
    sh_w16(e, p + 0x26, 0);
    sh_w16(e, p + 0x28, 0);
    return (int32_t)((uint32_t)b * 11u);
}

void sh_target_timed_event_init(const ShEnv* e, uint32_t obj) {
    const uint32_t p = 0x00622ab0u;
    sh_w32(e, p + 0xc, obj);
    sh_w8(e, p, 0);
    sh_w16(e, p + 6, 0x14);
    sh_w16(e, p + 8, 0x32);
    sh_w8(e, p + 1, 0);
    sh_w16(e, p + 4, 0);
    sh_w32(e, p + 0x14, 0x32f0);
    sh_w32(e, p + 0x18, 0x730);
    sh_w32(e, p + 0x10, 0xffffb848u);
    sh_w16(e, p + 0x1c, (uint16_t)drift());
    sh_w16(e, p + 0x1e, (uint16_t)drift());
    sh_w16(e, p + 0x24, 0);
    sh_w16(e, p + 0x26, 0);
    sh_w16(e, p + 0x28, 0);
    sh_w16(e, p + 0x20, 0xfff);
}

void sh_target_event_nop(void) {}

static uint32_t gcall(const ShEnv* e, uint32_t fn, int n, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t a[4] = {a0, a1, a2, a3};
    return e->call ? e->call(e->user, fn, n, a) : 0;
}

void sh_target_actor_hit(const ShEnv* e, int32_t i, uint32_t v) {
    uint32_t a = 0x00622b58u + 0x4cu * (uint32_t)i;
    if ((int8_t)sh_r8(e, a + 2) != 0) return;
    if (sh_r32(e, v + 4) == 0) sh_w32(e, v + 4, 1);
    int16_t ang[3] = {0, 0, (int16_t)tm2_atan2((int32_t)sh_r32(e, v), (int32_t)sh_r32(e, v + 4))};
    if ((int16_t)sh_r16(e, a + 4) != 0x205) {
        uint32_t pos = sh_r32(e, a + 0x48) + 0xe8;
        if (gcall(e, 0x0046cc54u, 0, 0, 0, 0, 0) == 0) gcall(e, 0x0046f974u, 1, pos, 0, 0, 0);
        else gcall(e, 0x0046f980u, 2, pos, 0xffffffffu, 0, 0);
        uint32_t b = gcall(e, 0x0046f990u, 0, 0, 0, 0, 0);
        uint32_t r = gcall(e, 0x0046f988u, 0, 0, 0, 0, 0);
        gcall(e, 0x0046f8b0u, 4, 0x19, r * 2, b, 0x32);
    }
    sh_w8(e, a + 2, 1);
    sh_w16(e, a + 8, 0);
    sh_w16(e, a + 0xa, 0x19);
    int32_t m[9];
    tm2_build_matrix(ang, m);
    for (int k = 0; k < 9; k++) sh_w32(e, a + 0x18 + 4 * (uint32_t)k, (uint32_t)m[k]);
    sh_registry_set_step(e, (int16_t)sh_r16(e, a + 4), (int16_t)sh_r16(e, a + 6), 2);
    uint32_t ent = sh_r32(e, a + 0x48);
    sh_w32(e, ent + 0xf0, sh_r32(e, ent + 0xf0) + 0x28);
    gcall(e, 0x00457398u, 1, sh_r32(e, a + 0x48) + 0xe8, 0, 0, 0);
    ent = sh_r32(e, a + 0x48);
    sh_w32(e, ent + 0xf0, sh_r32(e, ent + 0xf0) - 0x28);
}
