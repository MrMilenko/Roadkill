/* warps.c: see warps.h. */
#include "warps.h"

#include <string.h>

static int32_t rd32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static void wr32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }
static int16_t rd16(const uint8_t* p) { int16_t v; memcpy(&v, p, 2); return v; }
static void wr16(uint8_t* p, int16_t v) { memcpy(p, &v, 2); }
static uint32_t rdu32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }

/* the AI exit per event (1..8) and warp index: {route i16, i32}; -1 = leave as is */
static void ai_exit(uint8_t* car, uint8_t event, int16_t idx, int* place)
{
    static const int16_t tab[9][3][2] = {
        {{0}},
        {{0x35, 0x4A}, {0x35, 0x4A}, {0x35, 0x4A}},
        {{0}},
        {{0x31, 0x56}, {0x58, 0x57}, {0x58, 0x57}},
        {{0x37, 0x52}, {0x3B, 0x3D}, {0x3B, 0x3D}},
        {{0x02, 0x00}, {0x3C, 0x3B}, {0x3C, 0x3B}},
        {{0x45, 0x43}, {0x33, 0x34}, {0x33, 0x34}},
        {{0x09, 0x2E}, {0x26, 0x2F}, {0x26, 0x2F}},
        {{0x51, 0x4F}, {0x0B, 0x00}, {0x86, 0x42}},
    };
    *place = 0;
    if (event < 1 || event > 8 || event == 2)
        return;
    *place = 1;
    int k;
    if (event == 8) {
        if (idx == 0) k = 0;
        else if (idx == 1) k = 1;
        else if (idx == 2) k = 2;
        else return;            /* nothing written, still placed */
    } else {
        k = idx == 0 ? 0 : 1;
    }
    wr16(car + 0x52C, tab[event][k][0]);
    wr32(car + 0x5BC, tab[event][k][1]);
}

void world_warps_update(const WorldWarpsEnv* env)
{
    for (int i = 0; i < WORLD_WARPS; i++) {
        uint8_t* r = env->records + i * WORLD_WARP_REC;
        if (!r[0])
            continue;
        int human = r[1] != 0;
        uint8_t* car = env->resolve(env->user, rdu32(r + 0x20));
        uint8_t* body = car + (human ? 0x6C4 : 0x708);
        uint8_t* probes = car + (human ? 0x58C : 0x5D0);
        uint8_t* stats = car + (human ? 0x138 : 0x4EC);
        wr16(r + 4, (int16_t)(rd16(r + 4) + 1));
        if (rd16(r + 4) < 10) {
            wr16(body + 0x0A, 0);
            wr16(body + 0x0C, 0);
            wr16(body + 0x08, 0);
        }
        if (rd16(r + 4) == 10) {
            wr32(stats + 0x9C, 0);
            wr32(body + 0x20, rd32(r + 0x14));
            wr32(body + 0x24, rd32(r + 0x18));
            wr32(body + 0x28, rd32(r + 0x1C));
            env->probes_clear(env->user, probes);
            int32_t zone = env->find_zone(env->user, rd32(r + 0x14), rd32(r + 0x18), rd32(body + 0x28));
            env->probes_zone(env->user, probes, zone);
            env->probes_floor(env->user, probes, rd32(body + 0x28));
            env->drive(env->user, car, r[1]);
            if (r[1]) {
                wr32(car + 0x14, rd32(body + 0x20));
                wr32(car + 0x18, rd32(body + 0x24));
                wr32(car + 0x1C, rd32(body + 0x28));
                wr16(car + 0x0C, rd16(body + 0x2C));
                wr16(car + 0x0E, rd16(body + 0x30));
                wr16(car + 0x10, rd16(body + 0x34));
            } else {
                int place;
                ai_exit(car, env->event_type(env->user), (int16_t)i, &place);
                if (place)
                    env->ai_place(env->user, car);
            }
            uint8_t* obj = env->resolve(env->user, rdu32(r + 0x24));
            wr32(obj + 0xE8, rd32(r + 0x14));
            obj = env->resolve(env->user, rdu32(r + 0x24));
            wr32(obj + 0xEC, rd32(r + 0x18));
            obj = env->resolve(env->user, rdu32(r + 0x24));
            wr32(obj + 0xF0, rd32(r + 0x1C));
        }
        if (rd16(r + 4) == 11)
            wr32(stats + 0x9C, 1);
        if (rd16(r + 4) == 40) {
            wr32(env->resolve(env->user, rdu32(r + 0x24)) + 8, 0);
            r[0] = 0;
        }
    }
}
