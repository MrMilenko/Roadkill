/*
 * cb_drop_place.h: a car drops a weapon, TM2.EXE 0x00448118 (records: see cb_drops.h).
 *
 * cb_drop_place(car, kind, is_human): human car (index i16 +0, shooter id i16 +2, position +0x6E4,
 * weapon block +0x9A, owner = index + 1) or AI car (index i16 +4, shooter i16 +6, position +0x728,
 * block +0x68, owner = index + 50). The car's world object is fetched (0x0045ae24 / 0x0045ae2c,
 * result unused). v = position with z + 24; ground = 0x0042c2d4(v); the sound is placed at v
 * (0x0046f974, or 0x0046f980(v, -1) in split screen; nothing is played).
 *   kind 0x65: the first record with timer < 1 and the first of the 23 objects 0x004ffaf8 with
 *     +8 == 0 (none: return): record +0x18 = object, +4 owner, +6 shooter, +0x0A ground, +0x0C v with
 *     z - 24; object +8 = 1, object +0xE8 = the record's position; +2 = 15, timer = 900, kind = 0x14.
 *   kind 3: a live mine of this owner (kind 0x13, armed, +4 == owner) is set off: timer = 0, armed
 *     = 0, the event calls (0x00474508: 2 -> 0x00410890, 1 -> 0x00411334, 3 -> 0x00411e28 with the
 *     position), areaDamage(pos, +6, 1), z += 240, fx_explosion48(pos, 0) (0x00450aa0), object
 *     +8 = 0. Otherwise the first record with timer < 1 (none: return) takes owner, shooter, ground
 *     and position; when one of the 2 objects 0x004ffb54 is set with +8 == 0 it becomes a mine:
 *     +0x18 = object, object +8 = 1, object +0xE8 = position, timer = 0x7FFF, armed, +2 = 25, kind 0x13.
 *   any other kind: i16 0x004ffb48 = 0 (the timer of "record -1": ORIGINAL BUG, a stray store).
 *   After kind 0x65 / 3 with a record found: weapon block u8 +9 = 1.
 * ORIGINAL BUG: the objects of 0x004ffaf8 and the mine's object are used without a 0 test.
 *
 * Verified against the original under emulation (tools/emu/check_combat_dropplace.sh).
 */
#ifndef TM2_CB_DROP_PLACE_H
#define TM2_CB_DROP_PLACE_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    uint32_t (*human_object)(void* user, int32_t i);                     /* 0x0045ae24 */
    uint32_t (*ai_object)(void* user, int32_t i);                        /* 0x0045ae2c */
    int32_t (*ground)(void* user, const int32_t v[3]);                   /* 0x0042c2d4 */
    int32_t (*split)(void* user);                                        /* 0x0046cc54 */
    void (*locate)(void* user, const int32_t v[3], int32_t view);        /* 0x0046f974 / 0x0046f980 */
    int32_t (*event_type)(void* user);                                   /* 0x00474508 (al) */
    void (*event2)(void* user, uint32_t pos);                            /* 0x00410890 */
    void (*event1)(void* user, uint32_t pos);                            /* 0x00411334 */
    void (*event3)(void* user, uint32_t pos);                            /* 0x00411e28 */
    void (*area_damage)(void* user, uint32_t pos, int32_t shooter, int32_t big);   /* 0x00449f10 */
    void (*explosion48)(void* user, uint32_t pos, int32_t flag);         /* 0x00450aa0 */
    void* user;
} CbDropPlaceEnv;

void cb_drop_place(const CbDropPlaceEnv* e, uint32_t car, uint8_t kind, int is_human);

#endif /* TM2_CB_DROP_PLACE_H */
