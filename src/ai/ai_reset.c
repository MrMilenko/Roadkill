/*
 * ai_reset.c: AI car (re)start, ported from TM2.EXE (see ai_reset.h).
 *
 * The small clears below (0x00437ff8, 0x00418b60, 0x00429fd0, 0x0043a754) are also ported, as static
 * functions, in src/game/car_spawn.c and src/game/car_input.c; they are repeated here for the AI
 * layout until those are exported.
 */
#include "ai_reset.h"
#include "ai_mem.h"

#include "../core/tm2rand.h"
#include "../game/car.h"

/* 0x00437ff8: control block clear */
static void controls_clear(uint8_t* c) {
    static const uint8_t z[] = { 1, 2, 3, 4, 6, 7, 8, 9, 10, 0xb, 0xd, 0xe, 0x1a, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14,
                                 0x17, 0x1c, 0x1d, 0x15, 0x1e, 0x1f, 0x21, 0x24, 0x22, 0x25, 5, 0x26, 0x27, 0 };
    for (unsigned i = 0; i < sizeof z; i++) c[z[i]] = 0;
}

/* 0x00429fd0: input flag clear (here on the block at +0x40) */
static void input_clear(uint8_t* c) {
    static const uint8_t z[] = { 0xe, 0x10, 0xf, 0x13, 0x12, 0, 1, 3, 0x17, 0x18, 0x1b, 0x1c, 0x1d, 0x1f, 4, 5, 0xd };
    for (unsigned i = 0; i < sizeof z; i++) c[z[i]] = 0;
    c[0x1a] = 1;
}

/* 0x0043a754: probe step height (probes +0x08) by car type */
static int32_t step_height(uint16_t t) {
    switch (t) {
    case 0x10: case 0x40: case 0xb0: return 0x40;
    case 0x20: return 0x50;
    case 0x50: return 0x28;
    case 0x90: case 0xc0: return 0x38;
    case 0xe0: return 0x58;
    case 0xf0: return 0xa0;
    default: return 0x30;
    }
}

void ai_car_reset(uint8_t* ai, const AiEnv* env, const AiStartOps* ops) {
    w32(ai, 0x72c, 0);
    w32(ai, 0x728, 0);
    for (int o = 0x710; o <= 0x71c; o += 2)
        if (o != 0x716) w16(ai, o, 0);
    for (int o = 0x734; o <= 0x754; o += 4) w32(ai, o, 0);
    w32(ai, 0x730, i16(ai, 0x52e));
    for (int k = 0; k < 12; k++) {
        w32(ai, 0x758 + 4 * k, ops->identity[k]);
        w32(ai, 0x7b8 + 4 * k, ops->identity[k]);
    }
    ai[0x43c] = 0;
    controls_clear(ai + 0x68);
    /* 0x00418b60(ai, 0): motion reset */
    ai[0x69] = 0;
    w16(ai, 0x71c, 0);
    w32(ai, 0x754, 0);
    w32(ai, 0x740, i32(ai, 0x734));
    w32(ai, 0x744, i32(ai, 0x738));
    w32(ai, 0x748, i32(ai, 0x73c));
    w32(ai, 0x588, 1);
    /* 0x00420bac(ai + 0xCC) */
    ai[0xcf] = 0;
    ai[0xd0] = 0;
    w16(ai, 0xd2, 0);
    ai[0xce] = 0;
    uint8_t* st = ai + 0x4ec;
    int32_t pct = env->difficulty == 1 ? 0x78 : env->difficulty == 2 ? 0x8c : 0x4b;   /* 0x0045b488 */
    w16(st, 0x76, pct);
    w16(st, 0x78, pct);
    car_stats_derive(st);
    ops->stats_init(ops->user, st, i16(ai, 0x30));
    car_stats_set_top_speed(st, i16(st, 0x52));
    ops->collision_box(ops->user, ai);
    w32(ai + 0x5d0, 8, step_height((uint16_t)i16(ai, 0x30)));
    w16(ai, 0x16, 0x140);
    w16(ai, 0x18, 0xa0);
    w16(ai, 0x1a, 600);
    w16(ai, 0x1c, 1000);
    w16(ai, 0x1e, 2000);
    w32(ai, 0x58c, i16(ai, 0x526));
}

void ai_car_start(uint8_t* ai, int16_t type, int reset_weapons, const AiEnv* env, const AiStartOps* ops) {
    ai[0x4ec] = 0;
    w16(ai, 0x30, type);
    ai_car_reset(ai, env, ops);
    if ((uint8_t)reset_weapons) {
        ops->weapons_init(ops->user, ai, ai + 0x834);
    } else {
        w16(ai, 0x842, 2);
        if (i16(ai, 0x30) == 0xc0) ops->weapons_c0(ops->user, ai + 0x834);
    }
    w16(ai, 0x85c, 0x78);
    w16(ai, 0x85a, 0x78);
    int32_t mood = tm2_rand() % 3;
    w16(ai, 0xc, 0);
    w16(ai, 0x12, (i16(ai, 4) + 1) * 0x3c);
    ai[1] = (uint8_t)mood;
    for (int o = 0x32; o <= 0x3b; o++) ai[o] = 0;
    static const uint16_t words[] = { 0x456, 0x458, 0x45a, 0x45e, 0x460, 0x462, 0x464, 0x46c, 0x466, 0x46a };
    for (unsigned k = 0; k < sizeof words / sizeof words[0]; k++) w16(ai, words[k], 0);
    static const uint16_t bytes[] = { 0x43c, 0x43d, 0x43e, 0x43f, 0x440, 0x444, 0x445, 0x446, 0x447, 0x448, 0x449,
                                      0x454, 0x44c, 0x48a, 0x44e, 0x44f, 0x451, 0x450, 0x452, 0x453 };
    for (unsigned k = 0; k < sizeof bytes / sizeof bytes[0]; k++) ai[bytes[k]] = 0;
    w32(ai, 0x5bc, spawn_pick(ops->spawns, i8(&ops->spawn_count, 0)));
    ai_place_on_route_node(ai, env);
    input_clear(ai + 0x40);
    ops->hit_state(ops->user, ai);
    ai[0] = 1;
}
