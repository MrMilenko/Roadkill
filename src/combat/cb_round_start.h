/*
 * cb_round_start.h: start of a round, TM2.EXE 0x00457c80 (PS1 roundStart).
 *
 *   0x00502f94 = 0x00502f78 = 0; srand(0x00493038()); 0x00470900(); 0x00489a70();
 *   event type (0x00474508, al) 8 and the first AI car's type (i16 0x0050763c) 0xF0: 0x004325f8();
 *   mode i32 0x00502fa4 >= 5: u8 0x0050e0fc = 1, 0x0050e0f4 = 1, u8 0x0050e0fd = 0, u8 0x0050f8f0 = 0,
 *     0x0050e0f8 = 0; 0x00463514(13, 13) and 0x004632f4(13) polled until it returns nonzero;
 *     0x004632d4() ? (0x004327fc(), u8 0x0050f8f0 = 1) : 0x0046500c(); 0x00463514(14, 14), 0x004632f4(14)
 *     polled the same way;
 *   for every human car c (count 0x005075e0, blocks 0x898 from 0x005030a8): 0x004632b0() (result
 *     unused), 0x00437850(car, i16 car +0x90, u8 0x00502f80[c]), i32 0x005075c0[(i16)c] = 0, and
 *     outside split screen (0x0046cc54 == 0) 0x004225a0(car, i8 car +0x107);
 *   weaponsRoundReset 0x00447b70; u8 0x00502f80 = 0x00502f81 = 0;
 *   demo (u8 0x00502f9c): 0x00502fa8 / ac / b0 / d8 = 0, 0x00502f88 = 1, 0x00502f90 = rand() % AI
 *     count (0x0050c2b0; C remainder), 0x004225a0(0x005030a8, 7), u8 0x00502fa0[4] = 0, 0x0045c56c(),
 *     0x0045c350(), u8 +0x0D of every AI car block (0x00507600 + 0x880 * k) = 1.
 * ORIGINAL BUG: a demo round with no AI cars divides by zero.
 *
 * Verified against the original under emulation (tools/emu/check_combat_rstart.sh).
 */
#ifndef TM2_CB_ROUND_START_H
#define TM2_CB_ROUND_START_H

#include <stdint.h>

#include "weapons/wmem.h"

enum {   /* the plain calls of cb_round_start, by original address */
    CB_RS_470900, CB_RS_489A70, CB_RS_4325F8, CB_RS_4327FC, CB_RS_46500C, CB_RS_4632B0,
    CB_RS_ROUND_RESET, CB_RS_45C56C, CB_RS_45C350, CB_RS_COUNT
};

typedef struct {
    const WMem* mem;
    int32_t (*timer)(void* user);                          /* 0x00493038 */
    void (*srand)(void* user, uint32_t seed);              /* 0x004899e6 */
    int32_t (*rand)(void* user);                           /* 0x004899c2 */
    int32_t (*event_type)(void* user);                     /* 0x00474508 (al) */
    /* the plain calls: 0x00470900, 0x00489a70, 0x004325f8, 0x004327fc, 0x0046500c, 0x004632b0,
     * 0x00447b70, 0x0045c56c, 0x0045c350 */
    void (*call)(void* user, int which);
    void (*key_setup)(void* user, int32_t a, int32_t b);   /* 0x00463514 */
    int32_t (*key_poll)(void* user, int32_t a);            /* 0x004632f4 */
    int32_t (*key_check)(void* user);                      /* 0x004632d4 */
    void (*car_start)(void* user, uint32_t car, int32_t type, int32_t handicap);   /* 0x00437850 */
    int32_t (*split)(void* user);                          /* 0x0046cc54 */
    void (*car_view)(void* user, uint32_t car, int32_t view);                      /* 0x004225a0 */
    void* user;
} CbRoundStartEnv;

void cb_round_start(const CbRoundStartEnv* e);

#endif /* TM2_CB_ROUND_START_H */
