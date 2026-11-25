/*
 * cb2_special.h: the special weapon trigger, TM2.EXE 0x0044342c (no PS1 pair found; Ghidra
 * FUN_0044342c). eax = car block, dl = 1 for a human car (0: AI car).
 *
 * Car block (human / AI): id i16 +0x00 / i16 +0x04 (car +0x01 i8: an AI "kind" 2 also may use
 * 0x65), stats +0x138 / +0x4EC, body +0x6C4 / +0x708 (position +0x20 i32[3], angles +0x2C i32[3],
 * matrix +0x50 i32[9]), input block +0xCC / +0x40 (+0x1D u8 special pressed), flags +0x9A / +0x68
 * (+0x0D u8 blocked, +0x0E u8 beacon out), weapon state +0x848 / +0x834:
 *   +0x04 u8 (0x69: active)  +0x0C i8 special kind  +0x26 i16 cooldown  +0x30 i16 energy
 *   +0x34 i16 (0x69: timer)
 * Model record: 0x0045ae24(id) / 0x0045ae2c(id). Projectile id base: id + 1 (human) / i16 +0x04 +
 * 0x32 (AI). Cooldown after a trigger: human 0x3C >> 1 = 0x1E, AI the i32 at 0x004ffed8 (16 bit).
 *
 * Nothing when cooldown > 0 or kind == 0x6B. If flags +0x0D is set or the special is not pressed:
 * pressed = 0, done. If energy == 0: done (pressed stays). Else by kind (jump table 0x0044340c,
 * kind - 100 in 0..7; else just the tail):
 *   cost check: energy - cost < 0 -> human: 0x0046fca0(1) (no energy sound); else id == 0 ->
 *   0x00473568(0x0D, kind) first.
 *   100, 0x66 (cost 0x4B0): weaponLaunchProjectile 0x00444004(car, kind, human), energy -= cost,
 *     human id 0: 0x00436040(0x33, 0).
 *   0x65 (human, or AI with car +0x01 == 2; cost 0x168): weaponDropPlace 0x00448118(car, 0x65,
 *     human), energy -= cost, sound at the body position (0x0046cc54 ? 0x0046f980(p, -1) :
 *     0x0046f974(p)), 0x0046f8b0(0x14, 0x0046f988(), 0x0046f990(), 99), human id 0: hud 0x34.
 *   0x67 (cost 0x4B0): if flags +0x0E is clear, the first of the 4 beacons (pointers at
 *     0x004ffab8, object +0x08 == 0) is taken: flags +0x0E = 1, object +0x08 = 1, record i at
 *     0x004ffac8 (12 bytes): +0x02 i16 = car id (human only), +0x04 = model, +0x08 = object,
 *     +0x00 i16 = 0x21C. energy -= cost; human id 0: hud 0x36.
 *   0x68 (cost 0x168): car_spin_hit 0x00421920(car, human, 7, 1), energy -= cost, sound as 0x65
 *     with 0x10, human id 0: hud 0x37.
 *   0x69: only when 0x0045c340() == 0 and +0x04 == 0 (cost 0x4B0): +0x34 = 0x168, +0x04 = 1,
 *     energy -= cost, human id 0: hud 0x39.
 *   0x6A (cost 0xA8C): energy -= cost; target 0x00448de4(id, &target, human); angles = body
 *     angles; three launches 0x004146f8(base, target, &angles, &p; stack 10, a constant vector
 *     0x004e6a18 / 0x004e6a20 / 0x004e6a10) from p = body + M^T (i16 stats +0xDC, 0, i16 stats
 *     +0x44 + 8), then (x, y, z) = i16 stats +0xD4 / +0xD6 / +0xD8, then with x negated when the
 *     second launch returned >= 0 (16 bit); a 4th 0x00414114(model +0x158, target, &angles, &p;
 *     7) with p.z + 200 when the third returned >= 0. DECOMPILER TRAP: Ghidra drops the second
 *     stack argument of 0x004146f8. Dead code kept out of the port: two more M^T products and
 *     sin / cos of the body angles whose results are never read. Sound: human id 0:
 *     0x0046f8b0(0x0D, 0, 0, 99); else when (AI and +0x32) or id != 0: sound at p and
 *     0x0046f8b0(0x0D, ...).
 * Tail (every kind but the early returns): pressed = 0, cooldown set.
 *
 * Verified against the original under emulation (tools/emu/check_cb2_special.sh).
 */
#ifndef TM2_CB2_SPECIAL_H
#define TM2_CB2_SPECIAL_H

#include <stdint.h>

typedef struct {
    const int32_t* ai_cooldown;                 /* 0x004ffed8 */
    const uint32_t* beacons;                    /* 0x004ffab8, 4 object handles */
    uint8_t* beacon_recs;                       /* 0x004ffac8, 4 x 12 */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    uint32_t (*model)(void* user, int human, int32_t id);                 /* 0x0045ae24 / 0x0045ae2c */
    void (*net_msg)(void* user, int32_t a, uint32_t b);                   /* 0x00473568 */
    void (*launch)(void* user, uint8_t* car, int32_t kind, uint32_t human);   /* 0x00444004 */
    void (*hud)(void* user, int32_t a, int32_t b);                        /* 0x00436040 */
    void (*no_energy)(void* user, int32_t a);                             /* 0x0046fca0 */
    void (*drop_place)(void* user, uint8_t* car, int32_t kind, uint32_t human); /* 0x00448118 */
    int32_t (*sound_mode)(void* user);                                    /* 0x0046cc54 */
    void (*sound_at)(void* user, int32_t* pos, int mode);                 /* 0: 0x0046f974, 1: 0x0046f980(pos, -1) */
    uint32_t (*sound_get)(void* user, int which);                         /* 0: 0x0046f990, 1: 0x0046f988 */
    void (*sound_play)(void* user, int32_t snd, uint32_t b, uint32_t c, int32_t d);  /* 0x0046f8b0 */
    void (*spin_hit)(void* user, uint8_t* car, uint32_t human, int32_t a, int32_t b); /* 0x00421920 */
    int32_t (*state)(void* user);                                         /* 0x0045c340 */
    void (*pick_target)(void* user, int32_t id, int32_t* target, uint32_t human);    /* 0x00448de4 */
    int32_t (*launch_pair)(void* user, int32_t base, int32_t target, int32_t* ang, int32_t* pos,
                           int32_t n, uint32_t vec);                      /* 0x004146f8 */
    int32_t (*launch_one)(void* user, uint32_t model_158, int32_t target, int32_t* ang, int32_t* pos,
                          int32_t n);                                     /* 0x00414114 */
    void* user;
} Cb2SpecialEnv;

void cb2_special_trigger(const Cb2SpecialEnv* env, uint8_t* car, uint8_t human);

#endif /* TM2_CB2_SPECIAL_H */
