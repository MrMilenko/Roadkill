/*
 * cb2_special.c: the special weapon trigger, TM2.EXE 0x0044342c (see cb2_special.h).
 */
#include "cb2_special.h"

#include <string.h>

#include "../math/tm2math.h"

static int16_t rd16(const uint8_t* p) { int16_t v; memcpy(&v, p, 2); return v; }
static int32_t rd32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static void wr16(uint8_t* p, int16_t v) { memcpy(p, &v, 2); }
static void wr32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }

static void sound(const Cb2SpecialEnv* e, int32_t* pos, int32_t snd) {
    e->sound_at(e->user, pos, e->sound_mode(e->user) ? 1 : 0);
    uint32_t c = e->sound_get(e->user, 0);
    uint32_t b = e->sound_get(e->user, 1);
    e->sound_play(e->user, snd, b, c, 99);
}

/* p = body position + M^T v */
static void place(const uint8_t* body, const int32_t v[3], int32_t p[3]) {
    int32_t m[9];
    memcpy(m, body + 0x50, sizeof m);
    tm2_mat_vec_mul_row(m, v, p);
    for (int i = 0; i < 3; i++) p[i] = (int32_t)((uint32_t)p[i] + (uint32_t)rd32(body + 0x20 + 4 * i));
}

void cb2_special_trigger(const Cb2SpecialEnv* e, uint8_t* car, uint8_t human) {
    uint8_t *stats, *body, *input, *flags, *ws;
    uint32_t model;
    int32_t id, cooldown;
    int16_t base;
    if (human) {
        stats = car + 0x138; body = car + 0x6c4; input = car + 0xcc; flags = car + 0x9a; ws = car + 0x848;
        model = e->model(e->user, 1, rd16(car));
        base = (int16_t)(rd16(car) + 1);
        cooldown = 0x3c;
        id = rd16(car);
    } else {
        stats = car + 0x4ec; body = car + 0x708; input = car + 0x40; flags = car + 0x68; ws = car + 0x834;
        model = e->model(e->user, 0, rd16(car + 4));
        base = (int16_t)(rd16(car + 4) + 0x32);
        cooldown = *e->ai_cooldown;
        id = rd16(car + 4);
    }
    int32_t kind = (int8_t)ws[0x0c];
    if (rd16(ws + 0x26) > 0 || kind == 0x6b) return;
    if (flags[0x0d] || !input[0x1d]) { input[0x1d] = 0; return; }
    if (rd16(ws + 0x30) == 0) return;

#define AFFORD(cost) (rd16(ws + 0x30) - (cost) >= 0)
#define BROKE() do { if (human) e->no_energy(e->user, 1); } while (0)
#define NET() do { if (id == 0) e->net_msg(e->user, 0x0d, (uint8_t)kind); } while (0)
#define PAY(cost) wr16(ws + 0x30, (int16_t)(rd16(ws + 0x30) - (cost)))
#define HUD(c) do { if (human && id == 0) e->hud(e->user, (c), 0); } while (0)
    switch (kind) {
    case 100: case 0x66:
        if (!AFFORD(0x4b0)) { BROKE(); break; }
        NET();
        e->launch(e->user, car, kind, human);
        PAY(0x4b0);
        HUD(0x33);
        break;
    case 0x65:
        if (!human && (int8_t)car[1] != 2) break;
        if (!AFFORD(0x168)) { BROKE(); break; }
        NET();
        e->drop_place(e->user, car, 0x65, human);
        PAY(0x168);
        sound(e, (int32_t*)(body + 0x20), 0x14);
        HUD(0x34);
        break;
    case 0x67:
        if (!AFFORD(0x4b0)) { BROKE(); break; }
        NET();
        if (!flags[0x0e]) {
            int i;
            for (i = 0; i < 4; i++)
                if (rd32(e->resolve(e->user, e->beacons[i]) + 8) == 0) break;
            if (i < 4) {
                flags[0x0e] = 1;
                wr32(e->resolve(e->user, e->beacons[i]) + 8, 1);
                uint8_t* r = e->beacon_recs + 12 * i;
                if (human) wr16(r + 2, rd16(car));
                wr32(r + 4, (int32_t)model);
                wr32(r + 8, (int32_t)e->beacons[i]);
                wr16(r, 0x21c);
            }
        }
        PAY(0x4b0);
        HUD(0x36);
        break;
    case 0x68:
        if (!AFFORD(0x168)) { BROKE(); break; }
        NET();
        e->spin_hit(e->user, car, human, 7, 1);
        PAY(0x168);
        sound(e, (int32_t*)(body + 0x20), 0x10);
        HUD(0x37);
        break;
    case 0x69:
        if (e->state(e->user) != 0 || ws[4]) break;
        if (!AFFORD(0x4b0)) { BROKE(); break; }
        NET();
        wr16(ws + 0x34, 0x168);
        ws[4] = 1;
        PAY(0x4b0);
        HUD(0x39);
        break;
    case 0x6a: {
        if (!AFFORD(0xa8c)) { BROKE(); break; }
        NET();
        PAY(0xa8c);
        int32_t target = 0, ang[3], p[3], v[3];
        e->pick_target(e->user, id, &target, human);
        ang[0] = rd32(body + 0x2c); ang[1] = rd32(body + 0x30); ang[2] = rd32(body + 0x34);
        v[0] = rd16(stats + 0xdc); v[1] = 0; v[2] = rd16(stats + 0x44) + 8;
        place(body, v, p);
        e->launch_pair(e->user, base, target, ang, p, 10, 0x004e6a18);
        v[0] = rd16(stats + 0xd4); v[1] = rd16(stats + 0xd6); v[2] = rd16(stats + 0xd8);
        place(body, v, p);
        if ((int16_t)e->launch_pair(e->user, base, target, ang, p, 10, 0x004e6a20) >= 0) v[0] = -rd16(stats + 0xd4);
        v[1] = rd16(stats + 0xd6); v[2] = rd16(stats + 0xd8);
        place(body, v, p);
        if ((int16_t)e->launch_pair(e->user, base, target, ang, p, 10, 0x004e6a10) >= 0)
            p[2] = (int32_t)((uint32_t)p[2] + 200u);
        e->launch_one(e->user, (uint32_t)rd32(e->resolve(e->user, model) + 0x158), target, ang, p, 7);
        if (human && id == 0) e->sound_play(e->user, 0x0d, 0, 0, 99);
        else if ((!human && car[0x32]) || id != 0) sound(e, p, 0x0d);
        break;
    }
    default:
        break;
    }
#undef AFFORD
#undef BROKE
#undef NET
#undef PAY
#undef HUD
    input[0x1d] = 0;
    wr16(ws + 0x26, (int16_t)(human ? cooldown >> 1 : cooldown));
}
