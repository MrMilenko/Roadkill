/*
 * cb_world.h: warp start, a pooled object placed with a sound, and the car table reset of TM2.EXE.
 *
 *   0x004543f8 cb_warp_start (PS1 pair worldWarpStart): warp record i (0x28 bytes at 0x00502ea0:
 *     +0 u8 active, +1 u8 AI flag, +2 i8 the paired record, +4 i16 timer, +8 i32[3] exit position,
 *     +0x20 car, +0x24 warp object) starts for car unless the car is type 0xF0 and i is 0
 *     (type = car +0x2E >> 16, or +0x8E >> 16 for an AI car), the record is active or its pair is.
 *     Then: +4 = 0, +0 = 1, +1 = ai, +0x20 = car, object +8 = 1, object +0xE8..+0xF0 = exit position;
 *     for an AI car or a car whose u8 +0x32 is set: sound 0x17 at the object (+0xE8), prio 99.
 *   0x0045723c cb_obj4b_place: next object of the 0x00502f34 table (cursor i32 0x004e7418, back to 1
 *     above 4, signed): when set, its +4..+0x0C = pos, 0x00470320(0x2B3, cursor, 1), cursor + 1,
 *     and sound 0x22 at pos, prio 99, when |volume| < 1500.
 *   0x004575c0 cb_car_tables_clear (PS1 carTablesClear): u8 0x00502fdc[15] = 2, i32 0x00502fec[15]
 *     = -1, i32 0x0050c28c[9] = -1, i32 0x00503938 + 0x898 * k (k < 8) and 0x00507e88 + 0x880 * k
 *     (k < 9) = 0, slam_records_reset 0x0044c7e4, i32 0x0050c2b0, 0x00503084,
 *     0x0050c2b8, 0x0050c2bc = 0, 0x00470b20, then by event type (0x00474508, al): 6 -> the collapse
 *     reset 0x00454ae0, else 8 -> the track reset 0x00455e94 (the type is asked again); finally
 *     0x005075e4 = -1.
 *
 * Sound at a point: split = 0x0046cc54; split 0: 0x0046f974(pos), else 0x0046f980(pos, -1); then
 * pan 0x0046f990 and volume 0x0046f988 (in that order; 0x0045723c reads the volume once more before)
 * and 0x0046f8b0(id, volume, pan, 99). PC: the pan is used in split screen too.
 *
 * Verified against the original under emulation (tools/emu/check_combat_world.sh).
 */
#ifndef TM2_CB_WORLD_H
#define TM2_CB_WORLD_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    int32_t (*split)(void* user);                                 /* 0x0046cc54 */
    void (*locate)(void* user, uint32_t pos, int32_t view);       /* 0x0046f974 (view 0) / 0x0046f980 */
    int32_t (*volume)(void* user);                                /* 0x0046f988 */
    int32_t (*pan)(void* user);                                   /* 0x0046f990 */
    void (*play)(void* user, int32_t id, int32_t vol, int32_t pan, int32_t prio);   /* 0x0046f8b0 */
    void (*set_step)(void* user, int32_t id, int32_t i, int32_t v);                 /* 0x00470320 */
    void (*slam_records_reset)(void* user);                       /* 0x0044c7e4 */
    void (*collision_init)(void* user);                           /* 0x00470b20 */
    int32_t (*event_type)(void* user);                            /* 0x00474508, al */
    void (*collapse_reset)(void* user);                           /* 0x00454ae0 */
    void (*track_reset)(void* user);                              /* 0x00455e94 */
    void* user;
} CbWorldEnv;

void cb_warp_start(const CbWorldEnv* e, int32_t i, uint32_t car, int ai);
void cb_obj4b_place(const CbWorldEnv* e, uint32_t pos);
void cb_car_tables_clear(const CbWorldEnv* e);

#endif /* TM2_CB_WORLD_H */
