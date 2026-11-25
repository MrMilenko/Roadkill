/*
 * cb_track.h: a track vehicle (moving platform) is destroyed, TM2.EXE 0x00456fb0 (PS1 trackVehicleDestroy).
 *
 * Platform table: pointer i32 0x00502f18, 0x38 bytes per vehicle: +0x0C i32 destroyed flag, +0x34
 * its entity (+8 i32 visible, +0xE2 i32 whose high half is the deck height).
 *   slot +0x0C = 1, entity +8 = 0; tile = 0x0043b4b0(i) (its deck tile); every human car
 *   (0x0045afb4 count, 0x0045afbc get) with any of its 4 wheel probes on the ground (u8 +0x5AC,
 *   +0x5F4, +0x63C, +0x684) and probe 0's tile (i32 +0x5B0 >> 16) on that tile gets
 *   0x00421920(car, 1, 4, 1); then trackVehicleSurfaceSet 0x0043b440(i, {24000, 24000, 0}, 0,
 *   entity +0xE2 >> 16).
 * Only probe 0's tile is compared, whichever probe touches (as the original).
 *
 * Verified against the original under emulation (tools/emu/check_combat_track.sh).
 */
#ifndef TM2_CB_TRACK_H
#define TM2_CB_TRACK_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    int32_t (*surface_index)(void* user, int32_t i);                  /* 0x0043b4b0 */
    int32_t (*car_count)(void* user);                                 /* 0x0045afb4 */
    uint32_t (*car_get)(void* user, int32_t i);                       /* 0x0045afbc */
    void (*car_hit)(void* user, uint32_t car, int32_t a, int32_t b, int32_t c);   /* 0x00421920 */
    void (*surface_set)(void* user, int32_t i, const int32_t* pos, int32_t a, int32_t h); /* 0x0043b440 */
    void* user;
} CbTrackEnv;

void cb_track_vehicle_destroy(const CbTrackEnv* e, int32_t i);

#endif /* TM2_CB_TRACK_H */
