/*
 * cb_pickup.h: a car collects a pickup, TM2.EXE 0x004499ac (Ghidra name carParisSinkZonesUpdate:
 * wrong, it is the pickup handler).
 *
 * cb_pickup_collect(car, is_human, id, slot): ids above 0x1B8 are the respawned ones (id - 0x28,
 * the entity slot is then kept). Layout (human / AI): body +0x6C4 / +0x708, stats +0x138 / +0x4EC,
 * ammo block g +0x848 / +0x834 (i16 +0x10..+0x1C special ammo, +0x22 total, +0x24 capacity, u8 +0x0D
 * selected, i16 +0x32 armour). free = capacity - total. p0 = the car is human car 0 (0x0045afbc(0)).
 *   free < 1 (not for ids 0x1AE, 0x1A1, 0x1A2), or id 0x1AE with armour >= 990, and p0: refused.
 *   Otherwise 0x004735fc(10, (u8)(id - 0x90), (u8)slot) and by id: 0x191 / 0x193 / 0x194 / 0x196 /
 *   0x19B / 0x19D / 0x1A3: ammo += min(free, the level's amount 0x004ff2e4 / e8 / f0 / ec / f4 / f8 /
 *   fc) into +0x10 / 12 / 16 / 14 / 18 / 1A / 1C, a selected 8 becomes 1 / 2 / 4 / 3 / 5 / 6 / 7,
 *   p0: 0x00436040(0x0D / 0E / 0F / 10 / 11 / 12 / 14, 10); 0x1A1: health (stats i32 +0xA0, when > 0)
 *   = max (stats +0x38 >> 16); 0x1A2: health += max / 3, at most max; both p0: (0x13, 10); 0x1AE:
 *   armour + 100, at most 999, p0: (0x15, 10). Human or u8 car +0x32: sound 0x18 at the body, prio 99.
 *   Kept slot: 0x004707b0(id, slot, 0), entity slot unused. total = sum of the 7 ammo i16; full
 *   (total >= capacity) after a human ammo pickup: refused too.
 *   refused: 0x00436040(0x23, 3), 0x0046fca0(1).
 *
 * Verified against the original under emulation (tools/emu/check_combat_pickup.sh).
 */
#ifndef TM2_CB_PICKUP_H
#define TM2_CB_PICKUP_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    uint32_t (*human_get)(void* user, int32_t i);                       /* 0x0045afbc */
    void (*hud_pickup)(void* user, int32_t a, int32_t b, int32_t c);    /* 0x004735fc */
    void (*message)(void* user, int32_t id, int32_t t);                 /* 0x00436040 */
    int32_t (*split)(void* user);                                       /* 0x0046cc54 */
    void (*locate)(void* user, uint32_t pos, int32_t view);             /* 0x0046f974 / 0x0046f980 */
    int32_t (*pan)(void* user);                                         /* 0x0046f990 */
    int32_t (*volume)(void* user);                                      /* 0x0046f988 */
    void (*play)(void* user, int32_t id, int32_t vol, int32_t pan, int32_t prio);   /* 0x0046f8b0 */
    void (*pickup_set_visible)(void* user, int32_t id, int32_t slot, int32_t v);    /* 0x004707b0 */
    void (*refuse)(void* user, int32_t a);                              /* 0x0046fca0 */
    void* user;
} CbPickupEnv;

void cb_pickup_collect(const CbPickupEnv* e, uint32_t car, int is_human, uint32_t id, int32_t slot);

#endif /* TM2_CB_PICKUP_H */
