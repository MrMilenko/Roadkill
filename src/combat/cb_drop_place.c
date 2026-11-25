/* cb_drop_place.c: see cb_drop_place.h */
#include "combat/cb_drop_place.h"

#define REC(k) (0x004ffb5c + 0x1C * (uint32_t)(k))

static uint8_t* gp(const CbDropPlaceEnv* e, uint32_t a, uint32_t n) { return wmem_ptr(e->mem, a, n); }
static uint32_t rd32(const CbDropPlaceEnv* e, uint32_t a) { return w_u32(gp(e, a, 4), 0); }
static int16_t rd16(const CbDropPlaceEnv* e, uint32_t a) { return w_i16(gp(e, a, 2), 0); }
static void wr32(const CbDropPlaceEnv* e, uint32_t a, uint32_t v) { w_s32(gp(e, a, 4), 0, v); }
static void wr16(const CbDropPlaceEnv* e, uint32_t a, uint32_t v) { w_s16(gp(e, a, 2), 0, v); }
static void wr8(const CbDropPlaceEnv* e, uint32_t a, uint32_t v) { *gp(e, a, 1) = (uint8_t)v; }

static int free_record(const CbDropPlaceEnv* e) {
    for (int k = 0; k < 25; k++)
        if (rd16(e, REC(k) + 8) < 1)
            return k;
    return -1;
}

static void fill(const CbDropPlaceEnv* e, uint32_t r, uint16_t owner, uint16_t shooter, int32_t ground, const int32_t v[3]) {
    wr16(e, r + 4, owner);
    wr16(e, r + 6, shooter);
    wr16(e, r + 0x0A, (uint32_t)ground);
    wr32(e, r + 0x0C, (uint32_t)v[0]);
    wr32(e, r + 0x10, (uint32_t)v[1]);
    wr32(e, r + 0x14, (uint32_t)v[2] - 0x18);
}

static void show_at(const CbDropPlaceEnv* e, uint32_t r, uint32_t obj) {
    wr32(e, r + 0x18, obj);
    wr32(e, obj + 8, 1);
    uint32_t o = rd32(e, r + 0x18);
    for (int k = 0; k < 3; k++)
        wr32(e, o + 0xE8 + 4 * (uint32_t)k, rd32(e, r + 0x0C + 4 * (uint32_t)k));
}

void cb_drop_place(const CbDropPlaceEnv* e, uint32_t car, uint8_t kind, int is_human) {
    uint32_t posa, block;
    uint16_t owner, shooter;
    if ((uint8_t)is_human) {
        posa = car + 0x6E4;
        block = car + 0x9A;
        owner = (uint16_t)(rd16(e, car) + 1);
        e->human_object(e->user, rd16(e, car));
        shooter = (uint16_t)rd16(e, car + 2);
    } else {
        posa = car + 0x728;
        block = car + 0x68;
        owner = (uint16_t)(rd16(e, car + 4) + 0x32);
        e->ai_object(e->user, rd16(e, car + 4));
        shooter = (uint16_t)rd16(e, car + 6);
    }
    int32_t v[3] = {(int32_t)rd32(e, posa), (int32_t)rd32(e, posa + 4), (int32_t)(rd32(e, posa + 8) + 0x18)};
    int32_t ground = e->ground(e->user, v);
    if (e->split(e->user) == 0)
        e->locate(e->user, v, 0);
    else
        e->locate(e->user, v, -1);
    int k;
    if (kind == 0x65) {
        k = free_record(e);
        if (k < 0)
            return;
        int j;
        for (j = 0; j < 23; j++)
            if (rd32(e, rd32(e, 0x004ffaf8 + 4 * (uint32_t)j) + 8) == 0)
                break;
        if (j == 23)
            return;
        uint32_t r = REC(k);
        wr32(e, r + 0x18, rd32(e, 0x004ffaf8 + 4 * (uint32_t)j));
        fill(e, r, owner, shooter, ground, v);
        show_at(e, r, rd32(e, r + 0x18));
        wr8(e, r + 2, 0x0F);
        wr16(e, r + 8, 0x384);
        wr8(e, r, 0x14);
    } else if (kind == 3) {
        for (k = 0; k < 25; k++) {
            uint32_t r = REC(k);
            if ((int8_t)*gp(e, r, 1) == 0x13 && *gp(e, r + 1, 1) && rd16(e, r + 4) == (int16_t)owner)
                break;
        }
        if (k < 25) {
            uint32_t r = REC(k);
            wr16(e, r + 8, 0);
            wr8(e, r + 1, 0);
            wr16(e, r + 8, 0);
            if ((int8_t)e->event_type(e->user) == 2)
                e->event2(e->user, r + 0x0C);
            if ((int8_t)e->event_type(e->user) == 1)
                e->event1(e->user, r + 0x0C);
            if ((int8_t)e->event_type(e->user) == 3)
                e->event3(e->user, r + 0x0C);
            e->area_damage(e->user, r + 0x0C, rd16(e, r + 6), 1);
            wr32(e, r + 0x14, rd32(e, r + 0x14) + 0xF0);
            e->explosion48(e->user, r + 0x0C, 0);
            wr32(e, rd32(e, r + 0x18) + 8, 0);
        } else {
            k = free_record(e);
            if (k < 0)
                return;
            uint32_t r = REC(k);
            fill(e, r, owner, shooter, ground, v);
            int j;
            for (j = 0; j < 2; j++) {
                uint32_t o = rd32(e, 0x004ffb54 + 4 * (uint32_t)j);
                if (o && rd32(e, o + 8) == 0)
                    break;
            }
            if (j < 2) {
                show_at(e, r, rd32(e, 0x004ffb54 + 4 * (uint32_t)j));
                wr16(e, r + 8, 0x7FFF);
                wr8(e, r + 1, 1);
                wr8(e, r + 2, 0x19);
                wr8(e, r, 0x13);
            }
        }
    } else {
        wr16(e, 0x004ffb48, 0);
        return;
    }
    wr8(e, block + 9, 1);
}
