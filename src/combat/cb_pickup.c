/* cb_pickup.c: see cb_pickup.h */
#include "combat/cb_pickup.h"

static uint8_t* gp(const CbPickupEnv* e, uint32_t a, uint32_t n) { return wmem_ptr(e->mem, a, n); }
static uint32_t rd32(const CbPickupEnv* e, uint32_t a) { return w_u32(gp(e, a, 4), 0); }
static int16_t rd16(const CbPickupEnv* e, uint32_t a) { return w_i16(gp(e, a, 2), 0); }
static void wr16(const CbPickupEnv* e, uint32_t a, uint32_t v) { w_s16(gp(e, a, 2), 0, v); }
static void wr32(const CbPickupEnv* e, uint32_t a, uint32_t v) { w_s32(gp(e, a, 4), 0, v); }

static void refuse(const CbPickupEnv* e) {
    e->message(e->user, 0x23, 3);
    e->refuse(e->user, 1);
}

void cb_pickup_collect(const CbPickupEnv* e, uint32_t car, int is_human, uint32_t id, int32_t slot) {
    uint8_t human = (uint8_t)is_human;
    int p0 = e->human_get(e->user, 0) == car;
    int keep = 0;
    if ((int32_t)id > 0x1B8) {
        id -= 0x28;
        keep = 1;
    }
    uint32_t body = car + (human ? 0x6C4 : 0x708);
    uint32_t stats = car + (human ? 0x138 : 0x4EC);
    uint32_t g = car + (human ? 0x848 : 0x834);
    int32_t free = rd16(e, g + 0x24) - rd16(e, g + 0x22);
    if (free < 1 && id != 0x1AE && id != 0x1A1 && id != 0x1A2 && p0) {
        refuse(e);
        return;
    }
    if (id == 0x1AE && rd16(e, g + 0x32) >= 0x3DE && p0) {
        refuse(e);
        return;
    }
    e->hud_pickup(e->user, 10, (uint8_t)(id - 0x90), (uint8_t)slot);
    static const struct { uint16_t id; uint32_t amount; uint8_t ammo, sel, msg; } A[] = {
        {0x191, 0x004ff2e4, 0x10, 1, 0x0D}, {0x193, 0x004ff2e8, 0x12, 2, 0x0E}, {0x194, 0x004ff2f0, 0x16, 4, 0x0F},
        {0x196, 0x004ff2ec, 0x14, 3, 0x10}, {0x19B, 0x004ff2f4, 0x18, 5, 0x11}, {0x19D, 0x004ff2f8, 0x1A, 6, 0x12},
        {0x1A3, 0x004ff2fc, 0x1C, 7, 0x14}};
    int done = 0;
    for (unsigned k = 0; k < sizeof A / sizeof A[0]; k++) {
        if (id != A[k].id)
            continue;
        int32_t amt = (int32_t)rd32(e, A[k].amount);
        int32_t n = free <= amt ? free : amt;
        wr16(e, g + A[k].ammo, (uint32_t)(rd16(e, g + A[k].ammo) + n));
        if ((int8_t)*gp(e, g + 0x0D, 1) == 8)
            *gp(e, g + 0x0D, 1) = A[k].sel;
        if (p0)
            e->message(e->user, A[k].msg, 10);
        done = 1;
    }
    if (!done) {
        if (id == 0x1A1) {
            if ((int32_t)rd32(e, stats + 0xA0) > 0) {
                wr32(e, stats + 0xA0, (uint32_t)((int32_t)rd32(e, stats + 0x38) >> 16));
                if (p0)
                    e->message(e->user, 0x13, 10);
            }
        } else if (id == 0x1A2) {
            if ((int32_t)rd32(e, stats + 0xA0) > 0) {
                int32_t mx = (int32_t)rd32(e, stats + 0x38) >> 16;
                int32_t a = (int32_t)(rd32(e, stats + 0xA0) + (uint32_t)(mx / 3));
                wr32(e, stats + 0xA0, (uint32_t)a);
                mx = (int32_t)rd32(e, stats + 0x38) >> 16;
                wr32(e, stats + 0xA0, (uint32_t)(mx <= a ? mx : a));
                if (p0)
                    e->message(e->user, 0x13, 10);
            }
        } else if (id == 0x1AE) {
            wr16(e, g + 0x32, (uint32_t)(rd16(e, g + 0x32) + 100));
            if (rd16(e, g + 0x32) > 0x3E7)
                wr16(e, g + 0x32, 0x3E7);
            if (p0)
                e->message(e->user, 0x15, 10);
        }
    }
    if (human || *gp(e, car + 0x32, 1)) {
        e->locate(e->user, body + 0x20, e->split(e->user) == 0 ? 0 : -1);
        int32_t pan = e->pan(e->user);
        int32_t vol = e->volume(e->user);
        e->play(e->user, 0x18, vol, pan, 0x63);
    }
    if (!keep) {
        e->pickup_set_visible(e->user, (int32_t)id, slot, 0);
        wr16(e, 0x004fefbe + ((uint32_t)slot << 4), 0);
    }
    uint16_t total = 0;
    wr16(e, g + 0x22, 0);
    for (int k = 0; k < 7; k++) {
        total = (uint16_t)(w_u16(gp(e, g + 0x22, 2), 0) + w_u16(gp(e, g + 0x10 + 2 * (uint32_t)k, 2), 0));
        wr16(e, g + 0x22, total);
    }
    if ((int16_t)total < rd16(e, g + 0x24))
        return;
    if (id == 0x1AE || id == 0x1A1 || id == 0x1A2 || !human)
        return;
    refuse(e);
}
