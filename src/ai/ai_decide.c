/*
 * ai_decide.c: AI decisions about the target car, ported from TM2.EXE (see ai.h).
 * Verified against the original under emulation (tools/emu/check_ai_decide.sh).
 */
#include "ai.h"
#include "ai_mem.h"

#include "../math/tm2math.h"

static int32_t iabs(int32_t v) { return v < 0 ? (int32_t)(0u - (uint32_t)v) : v; }

/* Dark Tooth arena: 0 when the target (body b) or, outside the middle band, the car itself is in a
 * part of the arena it must not chase into */
static int boss_area_open(const uint8_t* ai, const uint8_t* b) {
    int32_t x = i32(b, 0x20), y = i32(b, 0x24);
    if (y < -12000) return 0;
    if (y < -0x2670) return x >= -8000 && x <= 0x2580;
    if (y > 0x320) return i16(ai, 0x45e) <= 3;
    const uint8_t* p = b + 0x20;          /* target position */
    if (y <= -0x2260 || y >= -0x15e0) {   /* else the car's own */
        p = ai + 0x728;
        int32_t ay = i32(p, 4);
        if (ay <= -0x2260 || ay >= -0x15e0) return 1;
    }
    if (i32(p, 0) > 0x10e0 && i32(p, 4) < 0x14a0) return 0;
    if (i32(p, 0) <= -0x14a0) return 1;
    return i32(p, 4) >= -0x10e0;
}

int ai_should_chase_direct(uint8_t* ai, int32_t range, const AiEnv* env) {
    int r = 0;
    uint8_t pl;
    const uint8_t* t = ai_target_car(env, i16(ai, 4), &pl);
    const uint8_t* tb = t + (pl ? 0x6c4 : 0x708);
    const uint8_t* ts = t + (pl ? 0x138 : 0x4ec);
    int32_t tz = i16(t, pl ? 0xc6 : 0x45e);
    if (env->event_type == 5) {
        if (!car_has_melee_special((uint16_t)i16(ai, 0x30))) ai[0x39] = 0;
        else if (tz == i16(ai, 0x45e)) ai[0x39] = 1;
        return ai[0x39];
    }
    if (i16(tb, 0xa) > i32(ts, 0xb4) / 2) {
        r = 0;
    } else if (ai[0x449] && i16(ai, 0x45c) > 0) {
        r = 0;
    } else if (i8(ai, 1) == 1 && i16(ai, 0x468) == 0) {
        int32_t dz = wsub(i32(ai, 0x730), i32(tb, 0x28));
        int32_t myz = i16(ai, 0x45e);
        if (env->flag_502f7c) {
            range = 0xc80;
            tz = myz;
            if (i32(ai, 0x28) > 600) dz = 0;
        }
        int ok;
        if (myz == tz) {
            ok = 1;
        } else if (env->event_type == 5) {   /* never true here: event 5 returned above */
            ok = env->grp[0x1218 + myz] == 0;
        } else if (dz > 400) {
            ok = iabs(i16(ai, 0x24)) < 0x31c;
            if (ok) range = 0x640;
        } else {
            ok = i16(ai, 0x46e) > 0;
        }
        if (ok && i16(ai, 0x514) < 6 && dz > -200) {
            if (!env->flag_50c2c4) range = tm2_wmul(range, 2);
            if (range < 0x140) range = 0x140;
            else if (range > 0x12c0) range = 0x12c0;
            if (range > i32(ai, 0x28)) r = 1;
        }
    }
    if (i16(ai, 0x30) == 0xf0 && !boss_area_open(ai, tb)) r = 0;
    return r;
}

/* 0x0045ad38 */
static int32_t nearest_ai(const AiEnv* env, const uint8_t* pos) {
    int32_t best = 0x7fff, pick = 0;
    for (int32_t i = 0; i < env->ai_count; i++) {
        const uint8_t* c = env->ai_cars + AI_BLOCK_SIZE * i;
        if (i32(c, 0x58c) <= 0) continue;
        int32_t d = tm2_wadd(iabs(wsub(i32(pos, 0), i32(c, 0x728))), iabs(wsub(i32(pos, 4), i32(c, 0x72c))));
        if (d < best) {
            best = d;
            pick = i;
        }
    }
    return pick;
}

uint8_t* car_nearest_to_human(int32_t human, uint8_t* is_player, const AiEnv* env) {
    const uint8_t* pos = env->players + PLAYER_BLOCK_SIZE * human + 0x6e4;
    if (env->game_mode < 4) {
        *is_player = 0;
        int32_t i = nearest_ai(env, pos);
        return env->ai_cars + AI_BLOCK_SIZE * (i < 0 || i > 8 ? 0 : i);
    }
    if (env->player_count <= 1) return 0;
    *is_player = 1;
    return env->players + PLAYER_BLOCK_SIZE * ai_nearest_player(env, pos, human);
}
