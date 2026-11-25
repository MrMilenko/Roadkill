/*
 * cb_ua.h: game state start values, TM2.EXE 0x00457464 (PS1 uaInit, old name subsys_game_state_init).
 *
 *   i32 0x00502f7c, 0x00502f88, 0x00502f8c, 0x00502f90, 0x00502f94, 0x00502f78 = 0, 0x00502f98 = 1;
 *   for the 8 human cars k (blocks 0x898 apart): u8 0x00502f80[k] = 1, i32 0x00503284 + o = 1,
 *     0x0050393c + o = k, 0x0050378c + o = 0x00503790 + o = 0x00503794 + o = 0, u8 0x00507568[k] = 1,
 *     u8 0x0050313c + o .. + 5 = 0 (o = 0x898 * k), and each round 0x0050e0f4 = 1, 0x0050e0f8 = 0;
 *   car_tables_clear 0x004575c0; i32 0x00507590[8] = 1; 0x0050c2b8 = 0x0050c2bc = 0; 0x00473160;
 *   0x0050e0f4 = 1, 0x0050e0f8 = 0, u8 0x0050e0fc = 1, u8 0x0050e0fd = 0, i32 0x005075e0 = 1,
 *   i16 0x005075b0 = 0x90; fxSpritesReset 0x00451458; srand(0x00493038()) (the timer);
 *   0x00502fa4 = 1, u8 0x00502f9c = 0, 0x00502fa8 = 0x00502fac = 0x00502fb0 = 0x00502fd8 = 0,
 *   i32 0x00502fb4[9] = -1.
 *
 * Verified against the original under emulation (tools/emu/check_combat_ua.sh).
 */
#ifndef TM2_CB_UA_H
#define TM2_CB_UA_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    void (*car_tables_clear)(void* user);   /* 0x004575c0 (cb_car_tables_clear) */
    void (*fn_473160)(void* user);          /* 0x00473160 */
    void (*fx_sprites_reset)(void* user);   /* 0x00451458 */
    int32_t (*timer)(void* user);           /* 0x00493038 */
    void (*srand)(void* user, uint32_t s);  /* 0x004899e6 */
    void* user;
} CbUaEnv;

void cb_ua_init(const CbUaEnv* e);

#endif /* TM2_CB_UA_H */
