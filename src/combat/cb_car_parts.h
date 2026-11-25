/*
 * cb_car_parts.h: the spinning car parts of TM2.EXE 0x00453538 (car type, speed).
 *
 * For five car types an angle accumulates speed / 7 (C division) and the part objects get the
 * rotation matrix rot_x(-angle) (0x0048c50c, written over an identity matrix whose diagonal is
 * 0xFFF, not 0x1000) in their +4..+0x24 (9 i32):
 *   type  angle (i32)   objects
 *   0x20  0x00502820    0x00502834[4]
 *   0xB0  0x00502830    0x00502844[2]
 *   0xC0  0x00502828    0x0050284c[4]
 *   0xE0  0x00502824    0x0050285c[6]
 *   0xF0  0x0050282c    0x00502874[4]
 * Any other type (signed type >> 4 < 15): 0x00502884[type >> 4] += |speed| (the full speed;
 * |INT_MIN| stays INT_MIN); above 0x140 it calls 0x0047070c(type >> 4, speed) and restarts at 0.
 * The kind 5 objects in these tables are registered by 0x00453400.. (cb_obj5_register).
 *
 * Verified against the original under emulation (tools/emu/check_combat_parts.sh).
 */
#ifndef TM2_CB_CAR_PARTS_H
#define TM2_CB_CAR_PARTS_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    void (*rot_x)(void* user, int32_t angle, int32_t* m);    /* 0x0048c50c, m: 9 i32 (+ 3 more) */
    void (*step_frames)(void* user, int32_t i, int32_t speed); /* 0x0047070c */
    void* user;
} CbCarPartsEnv;

void cb_car_parts_spin(const CbCarPartsEnv* e, int32_t type, int32_t speed);

#endif /* TM2_CB_CAR_PARTS_H */
