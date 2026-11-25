/*
 * gp5_contacts.c: the car contact loop of 0x0041f40c (see gp5_contacts.h).
 */
#include "gp5_contacts.h"

#include <string.h>

#include "../math/tm2math.h"

static int16_t i16(const uint8_t* b, int o) { return (int16_t)(b[o] | b[o + 1] << 8); }
static int32_t i32(const uint8_t* b, int o) { return (int32_t)(b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24); }
static void w16(uint8_t* b, int o, int32_t v) { b[o] = (uint8_t)v; b[o + 1] = (uint8_t)((uint32_t)v >> 8); }
static void w32(uint8_t* b, int o, int32_t v) { for (int i = 0; i < 4; i++) b[o + i] = (uint8_t)((uint32_t)v >> (8 * i)); }

void gp5_car_contacts(uint8_t* car, int player, int32_t points, const Gp5ContactEnv* env) {
    uint8_t* B = car + (player ? 0x21c : 0xcc);
    uint8_t* stats = car + (player ? 0x138 : 0x4ec);
    uint8_t* body = car + (player ? 0x6c4 : 0x708);
    uint8_t* ctl = car + (player ? 0x9a : 0x68);
    uint8_t* act = B + 0x34c;
    for (int32_t k = 0; k < points; k++) {
        uint8_t* c = B + 0x274 + 0x24 * k;
        if (c[0] == 0) break;
        if (act[0] == 0 || act[2] != 0) {
            memcpy(act, c, 0x24);       /* rep movsd, forward */
            w16(B, 0x12, i16(act, 0x12));
            w16(B, 0x14, i16(act, 0x14));
            uint8_t other_player = 0;
            uint8_t* o = env->car_by_id ? env->car_by_id(env->user, i16(act, 0x14), &other_player) : NULL;
            if (!o) continue;
            const uint8_t* os = o + (other_player ? 0x138 : 0x4ec);
            const uint8_t* ob = o + (other_player ? 0x6c4 : 0x708);
            if (i16(body, 0xa) <= i32(stats, 0xc4)) continue;
            if (i16(stats, 0x7a) >= i16(os, 0x7a)) continue;
            if (i16(stats, 0x7a) >= 0x1e) continue;
            int32_t d = (int32_t)((uint32_t)i32(body, 0x34) - (uint32_t)i32(ob, 0x34));
            tm2_angle_wrap32(&d);
            int32_t ad = d < 0 ? (int32_t)(0u - (uint32_t)d) : d;
            if (ad <= 0x5c7) continue;
            ctl[1] = 1;
            ctl[0x20] = 1;
            w32(body, 0x4c, 0);
            int32_t h = i16(body, 0x40);
            if (i16(act, 0xc) > 0) { w16(act, 0xc, 0xaa); w16(act, 8, h + 0xaa); }
            else { w16(act, 0xc, -0xaa); w16(act, 8, h - 0xaa); }
            w16(act, 6, i16(stats, 0xc4));
            w16(act, 0x12, 0);
            w16(body, 2, i16(body, 0x40));
            w16(stats, 0x20, 0);
            if (d > 0) { w16(body, 0x12, 1); w16(body, 4, i16(body, 0x40) + 0x400); }
            else { w16(body, 0x12, -1); w16(body, 4, i16(body, 0x40) - 0x400); }
            if (env->set_state) env->set_state(env->user, car, player, 0xa, 1);
        } else if (act[1] != 0) {
            if (env->pass_hit) env->pass_hit(env->user, c, act);
        } else if (i16(act, 0xe) < i16(c, 0xe)) {
            memcpy(act, c, 0x24);
        }
    }
}
