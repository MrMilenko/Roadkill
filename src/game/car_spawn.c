/*
 * car_spawn.c: player car spawn, ported from TM2.EXE 0x00437ac4 (see car_spawn.h).
 */
#include "car_spawn.h"

#include "car_stats.h"
#include "../core/tm2rand.h"
#include "../math/tm2math.h"
#include "../physics/grp.h"
#include "../physics/terrain_tile.h"

#include <string.h>

static int16_t i16(const uint8_t* b, int o) { return (int16_t)(b[o] | b[o + 1] << 8); }
static int32_t i32(const uint8_t* b, int o) { return (int32_t)(b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24); }
static void w16(uint8_t* b, int o, int32_t v) { b[o] = (uint8_t)v; b[o + 1] = (uint8_t)(v >> 8); }
static void w32(uint8_t* b, int o, int32_t v) {
    uint32_t u = (uint32_t)v;
    b[o] = (uint8_t)u; b[o + 1] = (uint8_t)(u >> 8); b[o + 2] = (uint8_t)(u >> 16); b[o + 3] = (uint8_t)(u >> 24);
}

void car_probes_clear(uint8_t* p) {
    static const uint16_t bytes[] = { 0x18, 0x20, 0x1b, 0x1a, 0x60, 0x68, 0x63, 0x62, 0xa8, 0xb0, 0xab, 0xaa,
                                      0xf0, 0xf8, 0xf3, 0xf2, 0x19, 0x61, 0xa9, 0xf1 };
    static const uint16_t shorts[] = { 0x3c, 0x84, 0xcc, 0x114, 0x3e, 0x86, 0xce, 0x116, 0x28, 0x70, 0xb8, 0x100,
                                       0x2e, 0x32, 0x30, 0x3a, 0x36, 0x76, 0x78, 0x82, 0x7e, 0xbe, 0xc2, 0xc0, 0xca,
                                       0xc6, 0x106, 0x10a, 0x108, 0x112, 0x10e, 0x22, 0x44, 0x2a, 0x2c, 0x6a, 0x8c,
                                       0x72, 0x74, 0xb2, 0xd4, 0xba, 0xbc, 0xfa, 0x11c, 0x102, 0x104, 0x7a };
    static const uint16_t ints[] = { 0x5c, 0xa4, 0xec, 0x134 };
    static const uint16_t fifteen[] = { 0x40, 0x88, 0xd0, 0x118 };
    for (unsigned i = 0; i < sizeof bytes / 2; i++) p[bytes[i]] = 0;
    for (unsigned i = 0; i < sizeof shorts / 2; i++) w16(p, shorts[i], 0);
    for (unsigned i = 0; i < sizeof ints / 2; i++) w32(p, ints[i], 0);
    for (unsigned i = 0; i < sizeof fifteen / 2; i++) w16(p, fifteen[i], 0xf);
    w16(p, 0, -1);
}

/* 0x00437ff8: clears the control block */
void car_controls_clear(uint8_t* c) {
    static const uint8_t z[] = { 1, 2, 3, 4, 6, 7, 8, 9, 10, 0xb, 0xd, 0xe, 0x1a, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14,
                                 0x17, 0x1c, 0x1d, 0x15, 0x1e, 0x1f, 0x21, 0x24, 0x22, 0x25, 5, 0x26, 0x27, 0 };
    for (unsigned i = 0; i < sizeof z; i++) c[z[i]] = 0;
}

/* 0x0043a754: probe step height by car id */
static int32_t step_height(uint16_t id) {
    switch (id) {
    case 0x10: case 0x40: case 0xb0: return 0x40;
    case 0x20: return 0x50;
    case 0x50: return 0x28;
    case 0x90: case 0xc0: return 0x38;
    case 0xe0: return 0x58;
    case 0xf0: return 0xa0;
    default: return 0x30;
    }
}

/* 0x00430368 */
void car_probes_floor(uint8_t* p, int32_t z) {
    for (int k = 0; k < 4; k++) w32(p, 0x5c + 0x48 * k, z);
    for (int k = 0; k < 4; k++) {
        int s = 0x18 + 0x48 * k;
        int16_t h = i16(p, s + 0x44);
        w16(p, s + 0x1c, h);
        w16(p, s + 0x24, h);
        w16(p, s + 0x26, h);
    }
}

/* the exported 0x0043a754 (EAX probes, DX car id) */
void car_probe_step_height(uint8_t* probes, uint16_t id) { w32(probes, 8, step_height(id)); }

/* the exported 0x0043033c (EAX probes, EDX zone): the zone of all four probes and its floor
 * height from the GRP zone table (grp +0x14 + 12 * zone, 0x004fca2c) */
void car_probes_zone(uint8_t* p, int32_t zone, const uint8_t* grp)
{
    for (int q = 0; q < 4; q++) w16(p, 0x28 + 0x48 * q, zone);
    car_probes_floor(p, i32(grp, 0x14 + 12 * (int64_t)zone));
}

/* the exported 0x00418b60 (EAX car, DL player): what car_spawn_player inlines, for both layouts */
void car_motion_reset(uint8_t* car, int player)
{
    uint8_t* c = car + (player ? 0x9a : 0x68);
    uint8_t* b = car + (player ? 0x6c4 : 0x708);
    c[1] = 0;
    w16(b, 0x14, 0);
    w32(b, 0x4c, 0);
    w32(b, 0x38, i32(b, 0x2c));
    w32(b, 0x3c, i32(b, 0x30));
    w32(b, 0x40, i32(b, 0x34));
}

static const uint8_t* node(const CarSpawnEnv* e, int32_t i) { return e->nodes + (int64_t)i * 24; }

/* 0x0042c3e0 (player): nearest node of the car's area (Manhattan distance) */
static int32_t nearest_node(const CarSpawnEnv* e, const uint8_t* car) {
    const uint8_t* b = car + 0x6c4;
    int32_t area = i16(car, 0xc6), best = 0x7fffffff, idx = 0;
    for (int32_t i = 0; i < e->node_count; i++) {
        const uint8_t* n = node(e, i);
        if ((int8_t)n[0x17] != area) continue;
        int32_t dx = i32(b, 0x20) - i32(n, 0), dy = i32(b, 0x24) - i32(n, 4), dz = i32(b, 0x28) - i16(n, 8);
        int32_t d = (dz < 0 ? -dz : dz) + (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
        if (d < best) { idx = i; best = d; }
    }
    return idx;
}

/* 0x00438320 (player): collision box corners into the scene entity */
static void collision_box(uint8_t* ent, const uint8_t* s) {
    int32_t w = i16(s, 0x48), front = i16(s, 0x4e), rear = i16(s, 0x50), h = 0x18 - i16(s, 0x42);
    w32(ent, 0xf4, 6);
    w16(ent, 0xf8, -w / 2); w16(ent, 0xfa, front); w16(ent, 0xfc, h);
    w16(ent, 0x100, w / 2); w16(ent, 0x102, front); w16(ent, 0x104, h);
    w16(ent, 0x108, w / 2); w16(ent, 0x10a, rear); w16(ent, 0x10c, h);
    w16(ent, 0x110, -w / 2); w16(ent, 0x112, rear);
    w16(ent, 0x118, 0); w16(ent, 0x114, h);
    w16(ent, 0x11a, front);
    w16(ent, 0x120, 0); w16(ent, 0x11c, h);
    w16(ent, 0x122, front);
    w16(ent, 0x124, i16(s, 0x46));
}

void car_spawn_player(uint8_t* car, const CarSpawnEnv* e) {
    car_controls_clear(car + 0x9a);
    car[0x92] = 0;
    car[0x93] = 0;
    static const uint16_t zero16a[] = { 0x6cc, 0x6ce, 0x6d0, 0x6e4, 0x6e6, 0x6e8, 0x6ea, 0x6f0, 0x6f2, 0x6f4, 0x6f6, 0x6f8, 0x6fa };
    for (unsigned i = 0; i < sizeof zero16a / 2; i++) w16(car, zero16a[i], 0);
    w16(car, 0x1d4, 1);
    w16(car, 0x1d6, 0);
    uint8_t* r = car + 0x21c;                  /* 0x00420bac */
    r[3] = 0; r[4] = 0; w16(r, 6, 0); r[2] = 0;

    int32_t n;
    if (e->mode < 5) {
        n = spawn_pick(e->spawns, e->spawn_count);
    } else {
        n = i32(car, 0x208);
        int retry = i16(car, 0) == 0;
        while (retry) {
            w32(car, 0x6e4, i32(node(e, n), 0));
            w32(car, 0x6e8, i32(node(e, n), 4));
            int ok = 1;
            for (int32_t i = 0; i < e->car_count && ok; i++) {
                if (i == i16(car, 0)) continue;
                const uint8_t* o = e->car_at(e->user, i);
                int32_t dx = i32(car, 0x6e4) - i32(o, 0x6e4), dy = i32(car, 0x6e8) - i32(o, 0x6e8);
                if (rmp_isqrt(tm2_wadd(tm2_wmul(dx, dx), tm2_wmul(dy, dy))) < 400) {
                    ok = 0;
                    n = spawn_pick(e->spawns, e->spawn_count);
                    w32(car, 0x208, n);
                }
            }
            retry = !ok;
        }
    }
    const uint8_t* nd = node(e, n);
    w32(car, 0x6e4, i32(nd, 0));
    w32(car, 0x6e8, i32(nd, 4));
    int32_t nz = i16(nd, 8);
    w32(car, 0x6ec, i16(car, 0x17a) + nz);
    int32_t cnt = (int8_t)nd[0x16], k = 0;
    if (cnt >= 1) k = tm2_rand() % cnt;
    const uint8_t* nb = node(e, i16(nd, 0xa + 2 * k));
    int32_t dy = i32(nb, 4) - i32(nd, 4);
    if (dy == 0) dy = 1;
    int32_t heading = tm2_atan2(i32(nb, 0) - i32(nd, 0), dy);
    static const uint16_t zero16b[] = { 0x6d4, 0x6d6, 0x6d8, 0x708, 0x70a, 0x70c, 0x70e, 0x710, 0x712,
                                        0x6fc, 0x6fe, 0x700, 0x702, 0x704, 0x706 };
    for (unsigned i = 0; i < sizeof zero16b / 2; i++) w16(car, zero16b[i], 0);
    w32(car, 0x6f8, heading);
    static const int32_t ident[12] = { 4095, 0, 0, 0, 4095, 0, 0, 0, 4095, 0, 0, 0 };
    for (int i = 0; i < 12; i++) {
        w32(car, 0x714 + 4 * i, ident[i]);
        w32(car, 0x774 + 4 * i, ident[i]);
        w32(car, 0x7bc + 4 * i, ident[i]);
    }
    w32(car, 0x7a4, i32(car, 0x6e4));
    w32(car, 0x7a8, i32(car, 0x6e8));
    w32(car, 0x7ac, i32(car, 0x6ec));
    w16(car, 0x1ae, 100);
    w16(car, 0x1b0, 100);
    uint8_t* s = car + 0x138;
    car_stats_derive(s);
    car_stats_init(s, i16(car, 0), (uint16_t)i16(car, 0x90), e->multiplayer, e->rotate, e->user);
    w16(car, 0x172, i16(car, 0x172) + 0x14);
    w32(car, 0x1d8, i16(car, 0x172));
    uint8_t* p = car + 0x58c;
    w32(p, 8, step_height((uint16_t)i16(car, 0x90)));
    car_probes_clear(p);
    int32_t zone = grp_find_zone(e->grp, i32(car, 0x6e4), i32(car, 0x6e8), i32(car, 0x6ec));
    for (int q = 0; q < 4; q++) w16(p, 0x28 + 0x48 * q, zone);
    car_probes_floor(p, i32(e->grp, 0x14 + 12 * zone));
    car_probes_floor(p, nz);
    car_drive(car, 1, e->surfaces, e->fx);
    /* 0x00418b60 */
    uint8_t* b = car + 0x6c4;
    car[0x9b] = 0;
    w16(b, 0x14, 0);
    w32(b, 0x4c, 0);
    w32(b, 0x38, i32(b, 0x2c));
    w32(b, 0x3c, i32(b, 0x30));
    w32(b, 0x40, i32(b, 0x34));
    if (e->entity) collision_box(e->entity(e->user, car), s);
    int32_t near = nearest_node(e, car);
    w16(car, 0xc, 0);
    w16(car, 0xe, 0);
    w16(car, 0xc4, near);
    w32(car, 0x14, i32(car, 0x6e4));
    w32(car, 0x18, i32(car, 0x6e8));
    w32(car, 0x1c, i32(car, 0x6ec));
    w16(car, 0x10, i16(car, 0x6f8));
}
