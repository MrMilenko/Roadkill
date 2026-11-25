/*
 * gp2_damage.h: damage to a car, ported from TM2.EXE 0x004210e4 (no PS1 pair found; the PS1's
 * carDamage family does the same job). Called by weapons, the claw, crashes and the level.
 *
 * gp2_car_damage(env, obj, amount, pos, kind, attacker):
 *   eax obj: object id of the hit car (0x0045b178 finds its slot: human / AI and index)
 *   edx amount, ebx pos (game address of an i32 x, y, z hit point, or 0), ecx kind (0 normal,
 *   1 explosive, 2 claw ...), stack attacker (i16, < 0 = none; its low byte is stored as the
 *   last attacker). Ghidra drops the stack argument: DECOMPILER TRAP (the function ends ret 4).
 *
 * Car fields (human block / AI block; the block is the second of two identical lookups
 * 0x0045afbc / 0x0045b080; the first one is only passed on to 0x00421800 / 0x00421920):
 *   stats S = +0x138 / +0x4EC: +0x04 i8, +0x05 i8 (3 = destroyed), +0x1A i16 speed,
 *     +0x3C i16 armour (halves), +0x7A i16 mass, +0x8C i16 damage taken (AI), +0xA0 i32 health,
 *     +0xA4 u32 can take damage
 *   flags F = +0x9A / +0x68: +0x00 dead, +0x0D slows, +0x0E frozen, +0x15 no spin, +0x1A freeze
 *     pending, +0x1F
 *   body +0x6C4 / +0x708 (its +0x20 is the position), +0x848 / +0x834 u32 object -> +0x44 u32
 *   (frozen sprite, i16 +0x0C), type u16 +0x90 / +0x30, "visible" u8 1 / +0x32, state i8 +0x13D /
 *   +0x4F1, killer u8 +0x13F / +0x4F3, last attacker u8 +0x140 / +0x4F4, u32 +0xC8 / +0x94
 *   (an object whose +0x08 is cleared on death). Human +0x6E4: the freeze effect position.
 *   World object W (0x0045ae24 / 0x0045ae2c): +0x08 u32, +0x15C u32 model, +0xE8 position.
 *
 * Steps: flash rate (amount > 2 ? 16 : 6) / ticks60, flash color (kind 0: white, else red);
 * an attacker car of slot kind 1 with u16 +0x1B2 == 100 doubles the amount; knock back for kind 1
 * above 10; a frozen car only takes the pending freeze; else (car visible, and type 0xF0 when the
 * 0x00502f94 flag is set) flicker light, rand spin above 7, debris (amount clamped 3..8), armour
 * and AI / no attacker cuts, the last attacker, health (human slot 0 or AI with an attacker, single
 * player only), the low damage grace, death (body hidden, HUD message, killer, objects released),
 * and on death the destruction: explosive (kind != 0 and amount > 5 in modes < 5, or type 0xF0)
 * debris toward the nearest player, else spin, choice state and smoke. Last the speed cut.
 *
 * ORIGINAL BUG kept: the "attacker < 0" test before the debris call is always true (amount > 0).
 *
 * Verified against the original under emulation (tools/emu/check_gp2_damage.sh).
 */
#ifndef TM2_GP2_DAMAGE_H
#define TM2_GP2_DAMAGE_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;     /* the car block and every record reached through it */
    void* user;
    /* addresses are game addresses */
    uint32_t (*attacker_car)(void* user, int32_t slot, uint8_t* kind);          /* 0x0045b028 */
    uint8_t (*find_by_id)(void* user, int32_t id, uint8_t* human, int32_t* idx); /* 0x0045b178 */
    uint32_t (*human_car)(void* user, int32_t i);                               /* 0x0045afbc */
    uint32_t (*ai_car)(void* user, int32_t i);                                  /* 0x0045b080 */
    uint32_t (*human_obj)(void* user, int32_t i);                               /* 0x0045ae24 */
    uint32_t (*ai_obj)(void* user, int32_t i);                                  /* 0x0045ae2c */
    void (*knock_back)(void* user, uint32_t car, uint8_t human, uint32_t pos, int32_t push); /* 0x00421800 */
    void (*freeze_burst)(void* user, uint32_t pos, int32_t z);                  /* 0x00450c08 */
    void (*freeze_glow)(void* user, uint32_t pos, int32_t z);                   /* 0x004512f8 */
    uint8_t (*flag_502f94)(void* user);                                         /* 0x0045bb50 */
    void (*flicker)(void* user, uint32_t obj, int32_t rate, const uint8_t rgb[3], int32_t amount); /* 0x00453b10 */
    int32_t (*rand)(void* user);                                                /* 0x004899c2 */
    void (*spin_hit)(void* user, uint32_t car, uint8_t human, int32_t a, int32_t b); /* 0x00421920 */
    void (*debris)(void* user, int32_t n, uint32_t pos);                        /* 0x004516bc */
    uint8_t (*singleplayer)(void* user);                                        /* 0x0045c348 */
    uint8_t (*multiplayer)(void* user);                                         /* 0x004632b0 */
    void (*body_hide)(void* user, int32_t type, int32_t z);                     /* 0x00453c38 */
    int32_t (*alive_count)(void* user);                                         /* 0x0045bab8 */
    void (*hud_msg)(void* user, int32_t a, int32_t b);                          /* 0x00436040 */
    uint8_t (*event_type)(void* user);                                          /* 0x00474508 */
    int32_t (*game_mode)(void* user);                                           /* 0x0045c338 */
    int32_t (*nearest_player)(void* user, uint32_t pos, int32_t skip);          /* 0x0045acb8 */
    uint8_t (*boss_defeated)(void* user, uint32_t car);                         /* 0x0045a8bc */
    uint32_t (*target_field)(void* user);                                       /* 0x0044df04 */
    uint32_t (*target_obj)(void* user);                                         /* 0x0044df10 */
    void (*burst)(void* user, uint32_t node, uint32_t pos, const int16_t dir[3], const int16_t spin[3],
                  const int32_t drift[3], int32_t hold, int32_t scale, int32_t end, int32_t rnd); /* 0x0048a738 */
    void (*explosion)(void* user, uint32_t pos, int32_t z);                     /* 0x00450aa0 */
    void (*smoke)(void* user, uint32_t pos, int32_t z);                         /* 0x0045109c */
    uint32_t (*car_object)(void* user, int32_t type);                           /* 0x00453f70 */
    void (*break_model)(void* user, uint32_t obj, uint32_t node, const int32_t target[3],
                        const int16_t dir[3], int32_t scale, int32_t life);     /* 0x0048a488 */
    void (*choice_state)(void* user, int32_t type, int32_t state, int32_t b);   /* 0x0047068c */
    void (*smoke_small)(void* user, uint32_t pos, int32_t z);                   /* 0x00451058 */
} Gp2DamageEnv;

void gp2_car_damage(Gp2DamageEnv* env, int32_t obj, int32_t amount, uint32_t pos, int32_t kind,
                    int16_t attacker);

#endif /* TM2_GP2_DAMAGE_H */
