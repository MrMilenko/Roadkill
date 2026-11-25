/*
 * cb_pickups.h: pickup counter resets of TM2.EXE.
 *
 *   0x0044edb0 cb_pickup_counters_reset: i32 0x004ffedc[31] = 0, then human car 0 (human_car_get
 *              0x0045afbc(0), asked again before every store): +0x1DC i32 = 1, +0x848 i32 = 0,
 *              +0x851 u8 = 0, +0x850 u8 = 0, then the turbo switch 0x0043ac5c(car, 0).
 *   0x0044e060 cb_game_data_init (old name subsys_game_data_init): registers the key sequence table
 *              0x004e6a84 (0x15D) with its handler 0x0044e1cc through 0x0049de80, the reset above,
 *              then i32 0x004ffff4[8] = 0 and the same car 0 steps again (as player_one_reset
 *              0x0044ee0c).
 * human_car_get returns a game address; car fields are written through the WMem.
 *
 * Verified against the original under emulation (tools/emu/check_combat_pickups.sh).
 */
#ifndef TM2_CB_PICKUPS_H
#define TM2_CB_PICKUPS_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    uint32_t (*human_car_get)(void* user, int32_t i);                  /* 0x0045afbc */
    void (*turbo_set)(void* user, uint32_t car, int32_t on);           /* 0x0043ac5c */
    /* 0x0049de80(table, n, handler): the original passes 0x004e6a84, 0x15D, 0x0044e1cc */
    void (*register_codes)(void* user, uint32_t table, int32_t n, uint32_t handler);
    void* user;
} CbPickupsEnv;

void cb_pickup_counters_reset(const CbPickupsEnv* e);
void cb_game_data_init(const CbPickupsEnv* e);

#endif /* TM2_CB_PICKUPS_H */
