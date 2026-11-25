/*
 * camera.h: the player's chase camera, ported from TM2.EXE 0x0043cdc0 (PS1 decomp: viewChaseCamera,
 * src/semantic/camera.c; the PC build differs in its easing and adds the fixed views 5 and 6).
 *
 * Camera fields in the player car block (0x898 bytes):
 *   +0x004 i16 ease weight of the old position (0..100)
 *   +0x00C i16 pitch, +0x00E i16 roll (always 0), +0x010 i16 yaw, +0x012 i16 (copied out)
 *   +0x014 i32[3] position          +0x02C i32[3] goal          +0x038 i32[3] look point
 *   +0x044 i32[3] offset, views 2 / 7   +0x050 views 3   +0x05C other views
 *   +0x068 split screen mode 1   +0x074 mode 2   +0x080 i32[3] look offset (+0x84 eased to 40)
 *   +0x106 u8 view: 1, 2, 3, 7 chase (7 = watching an AI car), 4 chase unless multiplayer,
 *          5 / 6 fixed point looking at the watched AI car / the player, others: nothing moves
 * It reads the body (+0x6C4: speed i16 +0x6CE, pos +0x6E4, pitch i16 +0x6F0, heading i16 +0x6F8,
 * matrix +0x714), the control flags (+0x9A: +0x06 steering, +0x0C all in air, +0x0D, +0x1D,
 * +0x1F falling, +0x20) and stats (+0x138: +0x64 steering limit).
 *
 * Verified against the original under emulation (tools/emu/check_camera.sh).
 */
#ifndef TM2_CAMERA_H
#define TM2_CAMERA_H

#include <stdint.h>

typedef struct {
    const uint8_t* watched;     /* vehicle_get_entity() 0x0045b0a0: the AI car block views 5 and 7 follow */
    int split;                  /* split screen mode, 0x00520e90 */
    int multiplayer;            /* gameloop_check_multiplayer() 0x004632b0 */
    int16_t* lock_frames;       /* 0x004fef24: frames since the camera was last locked */
    int32_t* prev_pos;          /* 0x004fef18: the position before this update (written) */
    const int32_t* fixed_pos;   /* 0x004fef04: fixed view offset (car space) */
    const int16_t* fixed_ang;   /* 0x004fef10 */
    /* 0x0045b1e8(0, fixed_pos, fixed_ang): places view 0 */
    void (*view_set)(void* user, const int32_t pos[3], const int16_t ang[3]);
    /* 0x00494cdc + 0x00494d34 (view 0): the camera's +0x0C..+0x13 and position */
    void (*view_out)(void* user, const uint8_t rot[8], const int32_t pos[3]);
    void* user;
} CamEnv;

void car_chase_camera(uint8_t* car, const CamEnv* env);

/* The camera part of the player car setup 0x00437850: look offset {0, 40, 0}, the view offsets
 * (+0x44 {0, -280, 112}, +0x50 {0, -480, 168}, +0x5C {0, -1200, 800}, +0x68 {0, -176, 80} and
 * +0x74 {0, -240, 80}) with per car changes for 0x20 / 0x60 / 0xC0 and 0xB0 / 0xE0, from the car id
 * at +0x90. The rest of that setup (spawn, weapons, model slot) is not here. */
void car_camera_init(uint8_t* car);

#endif /* TM2_CAMERA_H */
