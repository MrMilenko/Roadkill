/* cb_track.c: see cb_track.h */
#include "combat/cb_track.h"

static uint8_t* gp(const CbTrackEnv* e, uint32_t a, uint32_t n) { return wmem_ptr(e->mem, a, n); }
static uint32_t rd32(const CbTrackEnv* e, uint32_t a) { return w_u32(gp(e, a, 4), 0); }
static void wr32(const CbTrackEnv* e, uint32_t a, uint32_t v) { w_s32(gp(e, a, 4), 0, v); }

void cb_track_vehicle_destroy(const CbTrackEnv* e, int32_t i) {
    uint32_t slot = rd32(e, 0x00502f18) + (uint32_t)i * 0x38;
    wr32(e, slot + 0x0C, 1);
    wr32(e, rd32(e, slot + 0x34) + 8, 0);
    int32_t tile = e->surface_index(e->user, i);
    int32_t n = e->car_count(e->user);
    for (int32_t c = 0; c < n; c++) {
        uint32_t car = e->car_get(e->user, c);
        const uint8_t* p = gp(e, car, 0x688);
        if (!p[0x5AC] && !p[0x5F4] && !p[0x63C] && !p[0x684])
            continue;
        if ((w_i32(p, 0x5B0) >> 16) != tile)
            continue;
        e->car_hit(e->user, car, 1, 4, 1);
    }
    int32_t pos[3] = {24000, 24000, 0};
    uint32_t ent = rd32(e, rd32(e, 0x00502f18) + (uint32_t)i * 0x38 + 0x34);
    e->surface_set(e->user, i, pos, 0, (int32_t)rd32(e, ent + 0xE2) >> 16);
}
