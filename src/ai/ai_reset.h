/*
 * ai_reset.h: AI car (re)start, ported from TM2.EXE 0x0043b0cc and 0x0043af58 (see ai.h for the
 * block layout). Verified under emulation (tools/emu/check_ai_reset.sh).
 */
#ifndef TM2_AI_RESET_H
#define TM2_AI_RESET_H

#include <stdint.h>

#include "ai.h"
#include "../game/spawn.h"

/* Calls to code that is ported elsewhere but not exported, or not ported. */
typedef struct {
    const int32_t* identity;   /* 12 int32 at TM2.EXE 0x004e69a4 (read from the player's TM2.EXE) */
    /* 0x0043846c car_stats_init(1, stats, type) (src/game/car_stats.c; needs the stats table) */
    void (*stats_init)(void* u, uint8_t* stats, int16_t type);
    /* 0x00438320(ai, 0): collision box of the AI layout */
    void (*collision_box)(void* u, uint8_t* ai);
    /* 0x0043807c(ai, ai + 0x834, 0): weapon block setup */
    void (*weapons_init)(void* u, uint8_t* ai, uint8_t* weapons);
    /* 0x0044c81c(ai + 0x834): car type 0xC0's weapon block extra */
    void (*weapons_c0)(void* u, uint8_t* weapons);
    /* 0x00420be8(ai, 2, 2, 0, 0) */
    void (*hit_state)(void* u, uint8_t* ai);
    SpawnList* spawns;         /* 0x004fe1b4 */
    uint8_t spawn_count;       /* 0x004fe1e1 */
    void* user;
} AiStartOps;

/* 0x0043b0cc: puts the car's body, controls, stats (difficulty percent, the type's stats, top speed),
 * collision box and probes back to their start values; health = maximum (+0x526); range bands
 * +0x16..+0x1E = 320, 160, 600, 1000, 2000. */
void ai_car_reset(uint8_t* ai, const AiEnv* env, const AiStartOps* ops);

/* 0x0043af58: starts AI car `ai` as car type `type`: reset, weapons (reset_weapons: full setup, else
 * +0x842 = 2 and the 0xC0 extra), random mood 0..2, timers, route state cleared, a spawn node picked
 * (spawn_pick, src/game/spawn.c) and the car put on the route there (ai_place_on_route_node), then
 * active (+0x00 = 1). */
void ai_car_start(uint8_t* ai, int16_t type, int reset_weapons, const AiEnv* env, const AiStartOps* ops);

#endif /* TM2_AI_RESET_H */
