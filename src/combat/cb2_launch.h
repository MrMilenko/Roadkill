/*
 * cb2_launch.h: weaponLaunchProjectile, TM2.EXE 0x00444004 (PS1 decomp: weaponLaunchProjectile).
 * eax = car block, edx = projectile kind (unsigned), bl = 1 for a human car (0: AI car).
 *
 * Car block fields (human / AI):
 *   id: i16 +0x00 / i16 +0x04;   model record: 0x0045ae24(id) / 0x0045ae2c(id), its +0x158 u32
 *   stats +0x138 / +0x4EC: +0x42 i16 (launch delay base), +0x44 i16 (launch height)
 *   body +0x6C4 / +0x708: +0x20 i32[3] position, +0x2C i32[3] angles
 *   weapon state +0x848 / +0x834: +0x05 u8 (aim straight: angle x kept, y 0, z turned 0x800),
 *                                  +0x09 u8 (kinds 5 / 0x66: the 0x1E variant)
 *   flags +0x9A / +0x68: +0x09 u8 set to 1 when a projectile was made
 *   +0x32 u8 (sound for AI cars too)
 *
 * pos = body position with z + height + 8. angles = body angles, or with +0x05 set (x, 0,
 * (z + 0x800) & 0xFFF). Except for kind 4, a target is picked first: 0x00448de4(id, &target, human)
 * or, with +0x05 set, 0x00449048(...). Kinds:
 *   1: 0x00413f4c(model+0x158, target, &angles, &pos; stack 7), sound 0x0A
 *   2: 0x00414458(...; stack 10), sound 0x0B
 *   4: 0x00414178(model+0x158, &angles, &pos, 0x0F) (no target), sound 0x0C
 *   5 and 0x66: +0x09 ? 0x00414618(...; stack 0x1E) : 0x00414530(...; stack 10), sound 0x0F;
 *      0x66 also, for a human car with id 0, 0x00436040(0x35, 0) after the sound
 *   0x64: 0x00414114(...; stack 7), sound 0x0E through 0x0046f8b0 (no index argument)
 *   anything else: nothing.
 * The launcher's result is used as a 16 bit index (movsx). Sound, when (human or +0x32) and
 * index >= 0: 0x0046cc54() ? 0x0046f980(&pos, -1) : 0x0046f974(&pos); then 0x0046f8cc(sound,
 * 0x0046f988(), 0x0046f990(), 99; stack index + 1) (0x0046f990 is called first).
 * Index >= 0: flags +0x09 = 1, projectile 0x00415b3c(index) +0x46 i16 = max(0, i16 stats+0x42 - 0x18).
 *
 * Verified against the original under emulation (tools/emu/check_cb2_launch.sh).
 */
#ifndef TM2_CB2_LAUNCH_H
#define TM2_CB2_LAUNCH_H

#include <stdint.h>

typedef struct {
    const uint8_t* (*model)(void* user, int human, int32_t id);          /* 0x0045ae24 / 0x0045ae2c */
    /* which 0: 0x00448de4, 1: 0x00449048 */
    void (*pick_target)(void* user, int which, int32_t id, int32_t* target, uint32_t human);
    /* addr: the launcher; n = its last argument; target unused for 0x00414178 */
    int32_t (*launch)(void* user, uint32_t addr, uint32_t model_158, int32_t target, int32_t* angles,
                      int32_t* pos, int32_t n);
    int32_t (*sound_mode)(void* user);                                   /* 0x0046cc54 */
    void (*sound_at)(void* user, int32_t* pos, int mode);                /* 0: 0x0046f974(pos), 1: 0x0046f980(pos, -1) */
    uint32_t (*sound_get)(void* user, int which);                        /* 0: 0x0046f990, 1: 0x0046f988 */
    /* 0x0046f8cc (has_index) or 0x0046f8b0 */
    void (*sound_play)(void* user, int has_index, int32_t snd, uint32_t b, uint32_t c, int32_t d, int32_t index);
    void (*hud)(void* user, int32_t a, int32_t b);                       /* 0x00436040 */
    uint8_t* (*projectile)(void* user, int32_t index);                   /* 0x00415b3c */
    void* user;
} Cb2LaunchEnv;

void cb2_weapon_launch(const Cb2LaunchEnv* env, uint8_t* car, uint32_t kind, uint8_t human);

#endif /* TM2_CB2_LAUNCH_H */
