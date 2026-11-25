/*
 * cb_assets.h: weapon objects created when a level's assets are bound, TM2.EXE 0x00447dbc
 * (PS1 weaponAssetBind). asset: the asset record (game address) stored in each object's +0x15C.
 * Objects come from entity_alloc 0x00492d90 and go into the entity list (0x00492c4c).
 *
 *   id 0xA0   0x004ffe1c = new(0xA0): +8 = 0, +0x0C = 0
 *   id 0xF9   0x004ffed4 = new(0xF9): +8 = 1
 *   id 0x8A1  2 objects into 0x004ffb54: +8 = 0
 *   id 0x8A2  10 objects into 0x004ff8a8: +8 = 0, +0x158 = 0x8A2, +0x0C = 1, +0xF4 = 1,
 *             +0xF8 / +0xFA i16 = 0, +0xFC i16 = 40; 0x004ffab4 = asset (each round)
 *   id 0x8A4  23 objects into 0x004ffaf8: +8 = 0
 *   id 0x8A6  0x004ffab0 = asset (no object)
 *   id 0x8A9  4 objects into 0x004ffab8: +8 = 0
 *   id 0x915  0x004ffed0 = new(0x915): +8 = 0, +0x158 = 0x8A2, +0x0C = 1, +0xF4 = 1, +0xF8 /
 *             +0xFA i16 = 0, +0xFC i16 = 80; globals 0x004ffe9c, a4, a8, b0, b8, bc, c4, c8 = 0,
 *             0x004ffeac = 0x004ffeb4 = -40, 0x004ffea0 = 0x004ffec0 = 40
 *   other ids: nothing.
 * Every object gets +0x15C = asset. Returns the original's eax: the last object, or id when none.
 *
 * ORIGINAL BUG: in the loops (0x8A1, 0x8A2, 0x8A4, 0x8A9) only the first entity_alloc gets the id;
 * every later one gets the previous object's address in eax as its "id".
 *
 * Verified against the original under emulation (tools/emu/check_combat_assets.sh).
 */
#ifndef TM2_CB_ASSETS_H
#define TM2_CB_ASSETS_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    uint32_t (*entity_alloc)(void* user, uint32_t id);    /* 0x00492d90 */
    void (*entity_list_insert)(void* user, uint32_t obj); /* 0x00492c4c */
    void* user;
} CbAssetsEnv;

uint32_t cb_weapon_asset_bind(const CbAssetsEnv* e, uint32_t asset, uint32_t id);

/* 0x00454aa4 (PS1 worldWarpObjectsCreate): 3 warp objects (records of 0x28 at 0x00502ea0, object at
 * +0x24): entity_alloc, entity_list_insert, +0x15C = asset, +8 = 0. The same ORIGINAL BUG: the second
 * and third alloc get the previous object's address as id. Returns the last object. */
uint32_t cb_warp_objects_create(const CbAssetsEnv* e, uint32_t asset);

#endif /* TM2_CB_ASSETS_H */
