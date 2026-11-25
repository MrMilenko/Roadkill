/*
 * weapon_misc.h: small weapon state helpers, ported from TM2.EXE
 * (PS1 decomp: src/semantic/weapon_tables.c, pickup_spawn.c, fx_sprite_spawn.c, fx_wake_object.c,
 * weapon_fire_specials.c).
 *
 *   0x0044c7e4 slam_records_reset    claw (Mr. Slam) record cursor = 0 (0x004ff8a4)
 *   0x0044c81c slam_state_init       gives a car's weapon block (+0x48) a claw record if it has none
 *                                    (5 records of 0x12 at 0x004ff85c, cursor wraps after 4), resets it
 *   0x00470640 blast_anim_step       byte +7 of entry i of the pointer table at 0x00621370
 *   0x0044bb18 shields_update        4 energy shields follow their car, count down, end
 *   0x00415bc4 screen_flash_start    full screen colour flash: two flat quads at 0x004f5dc4 / 0x004f5ddc,
 *                                    frames (doubled in split screen) at 0x004f5df0
 *   0x004498d8 pickup_respawn_tick   every (period + 1) frames brings back one random pickup
 *
 * ORIGINAL BUG: the fifth claw record (0x004ff85c + 4 * 0x12) is the cursor 0x004ff8a4 itself and the
 *   12 bytes after it; slam_cursor must point into the same memory (+0x48 of the records) for the
 *   writes to land as in the original (the cursor store comes last and wins).
 * Claw record: +0 u8 = 0, +1 u8 = 1, +2 u8 = 0, +3 u8 = 1, +7 u8 = 0, +0x0B u8 = 0, +0x10 i16 = 0.
 * Shield record (4 of 0x0C at 0x004ffac8): +0 i16 frames left, +2 i16 car index, +4 u32 car body
 *   object (followed), +8 u32 shield object. Each frame while > 0: minus frame_get_ticks60 (PC; PS1
 *   counts frames), clamped at 0; at 0 the shield object hides (+8 = 0) and the car's shield flag
 *   (car block +0xA8, 0x0045afbc) clears. The shield gets the car's position (z - 64) and angles
 *   (+0xE0..+0xE7) and rebuilds its matrix, also on its last frame.
 * Flash quads: u8 r, g, b, code (0x2A semi transparent, 0x28 opaque), then four i16 x, y corners
 *   (0,0) (320,0) (0,240) (320,240).
 * Pickup record (16 bytes at 0x004fefbc): +0 i16 pickup id, +2 i16 shown flag. Ids 0x1A1 / 0x1A2 come
 *   back only when a second rand() is below 0x4000. Shown with 0x004707b0(id, n, 1), then the
 *   event 0x004735fc(11, id & 0xFF, n & 0xFF).
 *
 * Verified against the original under emulation (tools/emu/check_weapons_misc.sh).
 */
#ifndef TM2_WEAPON_MISC_H
#define TM2_WEAPON_MISC_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    int32_t* slam_cursor;          /* 0x004ff8a4 */
    uint32_t slam_records;         /* 0x004ff85c (game address) */
    const uint32_t* blast_anims;   /* 0x00621370 */
    uint8_t* shields;              /* 0x004ffac8 */
    uint8_t* flash;                /* 0x004f5dc4 .. 0x004f5df3 */
    int32_t split;                 /* 0x00520e90 */
    int32_t* pickup_timer;         /* 0x004fef6c */
    const int32_t* pickup_period;  /* 0x004e6a30 */
    const int32_t* pickup_count;   /* 0x004ff2dc */
    uint8_t* pickups;              /* 0x004fefbc */
    void* user;
    uint32_t (*car_data)(void* user, int32_t i);                       /* 0x0045afbc */
    void (*pickup_show)(void* user, int32_t id, int32_t n, int32_t on); /* 0x004707b0 */
    void (*event)(void* user, int32_t kind, int32_t a, int32_t b);       /* 0x004735fc */
} WMiscEnv;

void slam_records_reset(WMiscEnv* env);
void slam_state_init(WMiscEnv* env, uint8_t* weapons);
uint32_t blast_anim_step(WMiscEnv* env, int32_t i);
void shields_update(WMiscEnv* env);
void screen_flash_start(WMiscEnv* env, int32_t frames, uint8_t r, uint8_t g, uint8_t b);
void pickup_respawn_tick(WMiscEnv* env);

#endif /* TM2_WEAPON_MISC_H */
