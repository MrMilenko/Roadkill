/*
 * mt_destroyed.c: a car is destroyed (see mt_destroyed.h).
 */
#include "mt_destroyed.h"

#include <string.h>

static int16_t r16(const uint8_t* p) { int16_t v; memcpy(&v, p, 2); return v; }
static int32_t r32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static void w16(uint8_t* p, int16_t v) { memcpy(p, &v, 2); }
static void w32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }

#define CALL(f, a, b, c, d) e->f(e->user, (uintptr_t)(a), (uintptr_t)(b), (uintptr_t)(c), (uintptr_t)(d), 0, 0)
#define U(v) ((uint32_t)(int32_t)(v))

void mt_car_destroyed(uint8_t* car, uint8_t human, const MtDestroyed* e) {
    uint8_t *body, *st, *tm;
    int16_t type;
    int32_t idx, slot;
    uint32_t attached, pickup;
    if (human) {
        body = car + 0x6c4;
        st = car + 0x9a;
        tm = car + 0x138;
        st[0x11] = 1;
        type = r16(car + 0x90);
        idx = r16(car);
        slot = e->human_slot[idx];
        attached = (uint32_t)r32(car + 0xc8);
        pickup = (uint32_t)r32(car + 0x890);
        if (*e->flag_2f94 != 0) return;
    } else {
        body = car + 0x708;
        tm = car + 0x4ec;
        type = r16(car + 0x30);
        idx = r16(car + 4);
        slot = e->ai_slot[idx];
        attached = (uint32_t)r32(car + 0x94);
        st = car + 0x68;
        pickup = (uint32_t)r32(car + 0x87c);
        st[0x11] = r16(car + 0x20) > 0 ? 0 : 1;
    }
    if (*e->game_mode >= 5 && idx == 0) {
        e->kills[0] = (int32_t)((uint32_t)e->kills[0] + 1);
        uint32_t id = CALL(player_id, 0, 0, 0, 0);
        CALL(net_send3, 5, (uint32_t)e->kills[0] & 0xffff, id, 0);
        int32_t killer = (int8_t)car[0x13f];
        if ((int8_t)car[0x13f] < 0) killer = (int8_t)car[0x140];
        if (killer >= 0) {
            id = CALL(player_id, U(killer), 0, 0, 0);
            CALL(net_send2, 3, id, 0, 0);
        }
    }
    if (type == 0xc0) {
        if (pickup) {
            uint8_t* p = e->resolve(e->user, pickup);
            CALL(pickup_drop, U(r16(p + 0xe)), U(r16(body + 0x34)), 0, 0);
            p[0] = 0;
            w16(p + 0xe, -1);
            p[7] = 0;
        }
    } else if (st[0x1d]) {
        CALL(pickup_drop, U(type), U(r16(body + 0x34)), 0, 0);
    }
    if (CALL(sound_mode, 0, 0, 0, 0) == 0)
        CALL(music_call0, body + 0x20, 0, 0, 0);
    else
        CALL(music_call, body + 0x20, 0xffffffffu, 0, 0);
    if (type == 0x60 || type == 0x30 || type == 0xa0) {
        uint32_t b = CALL(get_990, 0, 0, 0, 0);
        uint32_t a = CALL(get_988, 0, 0, 0, 0) >> 1;
        CALL(sound_play, 0x1a, a, b, 99);
    } else {
        uint32_t b = CALL(get_990, 0, 0, 0, 0);
        uint32_t a = CALL(get_988, 0, 0, 0, 0) / 3u;
        CALL(sound_play, 0x19, a, b, 99);
    }
    CALL(loop_stop, 9, 0xffffffffu, 0, 0);
    if (type == 0xa0) CALL(sound_stop, 0x35, 0, 0, 0);
    uint32_t obj = e->slot_objects[slot];
    if ((uint8_t)CALL(event_type, 0, 0, 0, 0) == 6 && r32(body + 0x28) < 0xf0) {
        CALL(object_activate, obj, 0, 0, 0);
        CALL(fx_457138, body + 0x20, 2000, 0, 0);
        w16(tm + 0x26, 0);
    } else if ((uint8_t)CALL(event_type, 0, 0, 0, 0) == 8 && r32(body + 0x28) < -0x28) {
        CALL(object_activate, obj, 0, 0, 0);
        w16(tm + 0x26, 0);
    } else {
        int32_t v[3] = {r32(body + 0x20), r32(body + 0x24), 0};
        CALL(fx_451058, v, 0, 0, 0);
        CALL(car_explode, car, human, 4, 0);
        CALL(fx_450984, body + 0x20, 0, 0, 0);
        st[0x13] = 1;
        if (type == 0xf0) {
            w16(tm + 0x26, 0x168);
            uint32_t stk[1] = {0};
            int32_t id = r32(e->resolve(e->user, e->slot_objects[slot]) + 0x158);
            e->damage(e->user, U(id), 0xf, 0, 2, stk, 1);
        } else {
            w16(tm + 0x26, 0x78);
        }
    }
    if (attached) {
        w32(e->resolve(e->user, attached) + 8, 0);
        w32(car + (human ? 0xc8 : 0x94), 0);
    }
    st[0x10] = 1;
    st[0x0d] = 1;
    w16(tm + 0x1a, 0x21c);
    tm[2] = 0;
    uint32_t stk[1] = {human};
    e->car_state(e->user, (uintptr_t)car, 0, 2, 0, stk, 1);
}
