/*
 * cb_weapon_block.h: weapon block start values and the turbo top speed switch of TM2.EXE.
 *
 *   0x0043807c cb_weapon_block_init (PS1 weaponBlockInit): per car type special weapon values.
 *     type = i32 car +0x2E >> 16 (human car layout, flag 0) or car +0x8E >> 16 (flag != 0), compared
 *     unsigned. Weapon block: +4..+7 u8 = 0, +0x26 / +0x28 / +0x34 / +0x36 i16 = 0, ammo i16
 *     +0x0E..+0x20 = 0 then +0x0E = 2, +0x10 = 2, +0x18 = 1; +0x0D u8 current = 0, +0x2E = 0,
 *     +0x2A = 99, +0x32 = 200, +0x22 = 5, +0x0C u8 = 0x6B, +0x30 = 3000, +0x1E = 0; then by type
 *     (+0x24 i16, +0x0B u8, +0x2C i16):
 *       0x10 24 5 0x438   0x20 24 5 0xE10   0x30 24 3 0xCE4   0x40 24 5 0xE10   0x50 24 3 0x1068
 *       0x60 24 5 0x834   0x70 24 5 0x12C0  0x80 24 4 0x834   0x90 24 5 0x834   0xA0 24 3 0x438
 *       0xB0 24 5 0x834   0xC0 +0x2C 3000, slam_state_init(block) 0x0044c81c, then 24 3
 *       0xD0 24 5 0x4B0   0xE0 24 5 0x438   0xF0 24 5 0x834   other: +0x0B 5, +0x24 10 (+0x2C kept)
 *     finally the objects at +0x44 and +0x40 (game addresses, when set) get i16 +0x0C = 0.
 *     Returns the type (eax), slam_state_init's eax for 0xC0.
 *
 *   0x0043ac5c cb_car_turbo_set: car +0x174 i16 = on; on: +0x1B2 = 100, top speed i16 +0x18A += 60;
 *     off: +0x1B2 and +0x18A by car type (u16 +0x90):
 *       type  10  20  30  40  50  60  70  80  A0  B0  C0  D0  E0  F0  other   (hex)
 *       1B2   1E  23  19  1E  05  0F  19  0D  0E  1E  23  19  3C  64  1C
 *       18A   5C  60  7F  65  7D  79  6F  83  8D  67  5C  71  7F  7F  77
 *     (0x90 is "other"). Then stats (+0x138) +0x54 = top * 3 / 4, +0x5A = top * 2 / 3 (C division)
 *     and car_stats_derive(car + 0x138) (0x0043a944).
 *
 * Verified against the original under emulation (tools/emu/check_combat_wblock.sh).
 */
#ifndef TM2_CB_WEAPON_BLOCK_H
#define TM2_CB_WEAPON_BLOCK_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    int32_t (*slam_state_init)(void* user, uint32_t block);   /* 0x0044c81c, returns its eax */
    void* user;
} CbWeaponBlockEnv;

uint32_t cb_weapon_block_init(const CbWeaponBlockEnv* env, const uint8_t* car, uint32_t block, int ai_layout);

void cb_car_turbo_set(uint8_t* car, int on, void (*stats_derive)(void* user, uint8_t* stats), void* user);

#endif /* TM2_CB_WEAPON_BLOCK_H */
