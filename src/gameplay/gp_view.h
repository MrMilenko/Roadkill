/*
 * gp_view.h: player camera mode switch, ported from TM2.EXE 0x004225a0 (PS1 pair: carSetViewMode).
 *
 * Player car block (0x898): +0x00 i16 view index, +0x0C i16[3] camera angles, +0x14 i32[3] camera
 * position, +0x106 u8 camera mode, +0x107 u8 last close mode, +0x6E4 i32[3] car position,
 * +0x6F8 i16 car heading. Watched AI car (0x0045b0a0): +0x728 i32[3] position.
 *
 * gp_car_set_view_mode(car, mode): mode <= 4 (signed) is remembered in +0x107; mode <= 7 first resets
 * the view placement (0x0045b1e8(0, zero position, zero angles)). Then by mode:
 *   2 / 3 / 4: only when the old mode (+0x106) is the same: 2 resets the camera (below), 3 / 4 copy
 *              the car position
 *   7: camera position = the watched AI car's position (0x0045b0a0 read once per coordinate)
 *   any other: reset: angles x, y = 0, z = car heading, position = car position
 * and always hands the camera position (0x00494d34) and angles (0x00494cdc) to view (i16 +0x00),
 * then stores the mode in +0x106.
 *
 * Verified against the original under emulation (tools/emu/check_gameplay_view.sh).
 */
#ifndef TM2_GP_VIEW_H
#define TM2_GP_VIEW_H

#include <stdint.h>

typedef struct {
    void* user;
    void (*place)(void* user, int32_t a, const int32_t pos[3], const int16_t ang[3]);   /* 0x0045b1e8 */
    const uint8_t* (*watched)(void* user);                                              /* 0x0045b0a0 */
    void (*view_pos)(void* user, const uint8_t* pos, int32_t view);                     /* 0x00494d34 */
    void (*view_rot)(void* user, const uint8_t* rot, int32_t view);                     /* 0x00494cdc */
} GpViewEnv;

void gp_car_set_view_mode(GpViewEnv* env, uint8_t* car, int8_t mode);

#endif /* TM2_GP_VIEW_H */
