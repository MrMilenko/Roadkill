/*
 * car.h: car block (0x898 bytes) steps, ported from TM2.EXE.
 *
 * Player cars: g_vehicle_count blocks at 0x005030a8, updated by 0x00415f00 after input.
 * AI cars:     g_waypoint_count blocks at 0x0050760c (old names "waypoint", wrong), updated by
 *              0x00422e08 (old notes called it the player driving code, wrong).
 * The same sub blocks sit at different offsets in the two kinds; functions take the kind flag
 * like the original (player = 1, AI = 0). Offsets known so far:
 *
 *                 player   AI      contents (from the functions ported here)
 *   body          +0x6C4  +0x708   +0x08/0A/0C i16 local velocity, +0x10/12/14 i16, +0x20/24/28
 *                                  i32 position, +0x2C..34 i32, +0x30 i32 angle, +0x34 / +0x40
 *                                  i32 angles, +0x38..40 i32 copy, +0x44..4C i32, +0xB0 matrix
 *   controls      +0x09A  +0x068   per frame flag bytes
 *   stats         +0x138  +0x4EC   per car values (+0x42, +0x46, +0x48 i16 used below)
 *   probes        +0x58C  +0x5D0   4 terrain probes (terrain_probe.h), 0x48 apart from +0x18
 *   turn          +0x7EC  +0x830   i16 added to body +0x14 while a probe is latched
 *   flag          +0x09B  +0x446   byte read by car_slide_check
 * Verified against the original under emulation (tools/emu/check_car.sh).
 */
#ifndef TM2_CAR_H
#define TM2_CAR_H

#include <stdint.h>

#include "../physics/terrain_probe.h"

#define CAR_BLOCK_SIZE 0x898

typedef struct {
    uint32_t body, controls, stats, probes, turn, flag;
    uint32_t steer_flags;   /* player +0x0CC, AI +0x040: bytes +3, +0x0F, +0x10 read by car_heading_follow */
    uint32_t steer_halve;   /* player +0x21C, AI +0x0CC: byte +3 halves the follow rate */
} CarLayout;

extern const CarLayout CAR_PLAYER, CAR_AI;
static inline const CarLayout* car_layout(int player) { return player ? &CAR_PLAYER : &CAR_AI; }

/* 0x00418cf0 (body, controls): marks controls +0x19, clears +0x01; unless controls +0x0C is set,
 * copies body +0x10/12/14 (i16) to +0x44/48/4C (i32) and +0x2C..34 to +0x38..40; else clears
 * body +0x4C. */
void car_snapshot(uint8_t* body, uint8_t* controls);

/* 0x00418d38: controls +0x02 = flag && body +0x0A > ticks60 * 0x2F8 / 100 * 32 && the wrapped
 * angle difference body +0x40 - +0x34 exceeds 0x200. */
void car_slide_check(uint8_t* car, int player);

/* 0x0041858c: moves body +0x20/24 by the local velocity / 32 rotated by the body matrix (rows),
 * and body +0x28 by a lift blended from two stats by the angle body +0x30. */
void car_move(uint8_t* car, int player);

/* 0x004220b8: controls +0x0F = any probe latched (clearing latches whose probe left the ground);
 * while latched adds the turn value to body +0x14 and may snapshot (car_snapshot). */
void car_probe_latch(uint8_t* car, int player);

/* 0x00419168: turns the heading body +0x40 toward the travel angle +0x34 at a rate from speed
 * (body +0x0A against stats +0xB0 / +0xBC), steering flags and stats +0x7A; snapshots when slow or
 * when the heading reaches the travel angle; leaves it while sliding fast (difference > 0x58E). */
void car_heading_follow(uint8_t* car, int player);

/* 0x0043a944: derives the per frame handling values of a stats block from its base values,
 * rescaled for the frame rate (frame_get_fps, 60 = no change). Byte +0 nonzero = player car. */
void car_stats_derive(uint8_t* stats);

/* What car_ground_contact needs from outside the car block. */
typedef struct {
    const RmpWorld* world;          /* terrain tiles and zones for the probe updates */
    /* moving platform table (pointer at 0x00502f18, 0x38 bytes per slot): +0x08 i32 angle step,
     * +0x28 i32 x/y/z step; the slot is the probe's extra tile slot */
    const uint8_t* platforms;
    /* x, y of the platform's entity (table +0x34 -> entity +0xE8) */
    const int32_t* (*platform_target)(void* user, int32_t slot);
    /* 0x00420be8(car, 3, 2, 1, player): fired when the car leaves the ground */
    void (*left_ground)(void* user, uint8_t* car, int player);
    void* user;
} CarEnv;

/* 0x0042a020, first part (up to 0x0042a4e7): updates the four probes (rmp_probe_update +
 * rmp_probe_finish), sets controls +0x0B any latched / +0x0C all latched / +0x0A on ground,
 * picks the probe that gives the current tile (probes +0x00) and height (+0x04), rides moving
 * platforms and applies air drag (player cars). The rest of 0x0042a020 (surface effects) is not
 * ported yet. Quirk kept: probe 0 is compared against probe 1's terrain height. */
void car_ground_contact(uint8_t* car, int player, const CarEnv* env);

/* 0x0042d470 (driving model), first part up to 0x0042d915: per probe suspension height
 * (state +0x44 = sum of state +0x16/18/1A/22, plus +0x26 while off terrain), raised to a floor
 * from the front pair (probes 0, 1; stats +0x48) and the whole car (probes 2, 3; stats +0x4A),
 * flagging bottomed probes (state +0x07) and adjusting their spring terms; then nudges the body
 * sideways by which corners bottomed out (unless all four probes are latched). */
void car_suspension_limit(uint8_t* car, int player);

/* Effect and event calls the driving model makes (stubbed in the tests, logged in order).
 * arg is the call's amount where it has one, else 0. */
enum {
    CAR_FX_SKID = 1,        /* 0x004516bc(1, body + 0x20) */
    CAR_FX_SPARKS = 2,      /* 0x004512b4(body + 0x20, 0) */
    CAR_FX_SCREEN = 3,      /* display_transition(6, 0, 0, 99), arg 99 */
    CAR_FX_SPIN_HIT = 4,    /* 0x00421920(car, player, 4, 1) */
    CAR_FX_LAND = 5,        /* 0x004b8010(car + 0x6E4, arg): arg -1 when 0x00520e90 is set, else 0 */
    CAR_FX_SHAKE_HARD = 6,  /* display_transition(5, .., .., arg) on a hard landing */
    CAR_FX_SHAKE = 7,       /* display_transition(3, .., .., arg) on a landing */
    CAR_FX_DUST = 8,        /* 0x00450718(probe position + 16 up, 0): the fx call's car argument is
                             * the probe state (position at +0x3C), not the car */
    CAR_FX_SET_STATE = 9,   /* vehicle_set_state 0x004225a0(car, arg) */
    CAR_FX_TURBO_FLAME = 10,/* 0x00450c08(body + 0x20, 0) */
    CAR_FX_TURBO_SMOKE = 11,/* 0x004512f8(body + 0x20, 0) */
    CAR_FX_RUMBLE_STOP = 12,/* force feedback stop 0x0046f860(0, arg = player) */
    CAR_FX_RUMBLE = 13,     /* force feedback 0x0046f840(0, 0, 0, 99, arg = player) */
    CAR_FX_TURBO_OBJ = 14,  /* the object at turbo block +0x44 gets i16 +0x0C = 0 (arg = its address) */
    CAR_FX_TRANSITION = 15  /* display_transition(arg, 0, 0, 99) from the driving input */
};
typedef struct {
    void (*fx)(void* user, int id, uint8_t* car, int player, int32_t arg);
    void* user;
    int land_flag;          /* 0x00520e90 (read by 0x0046cc54) */
    uint8_t rumble[2];      /* force feedback enabled for player 0 / 1 (0x00621184 / 0x00621185) */
    int32_t ai_turbo_mode;  /* 0x00502f98 (0x0045b3dc): < 1 lets AI cars hold turbo longer */
} CarFx;

/* values part 2 computes before it changes anything, used again by part 3 */
typedef struct {
    int rough;              /* controls / air time / speed / yaw rate over limits: snap, no easing */
    int32_t speed_ref;      /* max(stats +0xB4, |speed|) */
} CarDriveTmp;

/* 0x0042d470, second part (0x0042d915 to 0x0042e0cf): body roll (body +0x30) from the left/right
 * suspension heights, limited by speed; starts a spin out (controls +0x12) when the front pair
 * loses grip at speed; while spinning, turns the car (sin boosted rate), counts spins (stats
 * +0x20), shifts the suspension heights and bleeds speed, and ends the spin when slow and level.
 * Uses tm2_rand (as 0x004899c2) for the skid and spark effects. */
void car_roll_spin(uint8_t* car, int player, const CarFx* fx, CarDriveTmp* tmp);

/* 0x0042f06c (player cars, per probe state): wheel bounce offset (+0x1E / +0x20 / +0x22) from
 * compression speed, capped by cap (car +0x16C). */
void car_wheel_bounce(uint8_t* state, int32_t cap);

/* 0x0042f250 (per probe state): dust (CAR_FX_DUST) on a hard touch down or after > 60 ticks in
 * the air (state +0x02 remembers it). */
void car_wheel_dust(uint8_t* state, int32_t threshold, const CarFx* fx, uint8_t* car, int player);

/* 0x0042d470 from 0x0042e0cf to the end: body pitch (+0x2C) from front/back heights or a flip
 * (controls +0x20, mirrors the spin out), ride height (+0x28), wheel tilt angles, wheel bounce,
 * dust, landing impact (shakes unless the zone surface is 2 or 3) and the landing spin
 * (controls +0x1E). surfaces = GRP + 0x125E (0x004fdc76). */
void car_pitch_land(uint8_t* car, int player, const uint8_t* surfaces, const CarFx* fx, const CarDriveTmp* tmp);

/* 0x00418450: integrates the angular rates into the two angle sets (body +0x2C..34 += i16
 * +0x10..14, +0x38..40 += +0x44..4C), wraps them, and builds the matrices at body +0xB0 (from
 * +0x38..40), +0x50 (from +0x2C..34) and +0x80 (+0x2C..34 plus the wheel tilt angles). */
void car_orient(uint8_t* car, int player);

/* 0x0042ca04: probe state st moves from its zone (+0x10) toward new_zone (GRP zones, solid floor
 * heights, passable table GRP + 0x1218): drops into a lower zone go airborne, raised floors block
 * unless the probe is within climb of it (climb) or another probe already shares the zone. */
void car_zone_cross(uint8_t* st, int32_t new_zone, int32_t climb, int shared, const uint8_t* grp);

/* 0x0042c4d4: places the four wheel probes (offsets from the car stats rotated by body +0x50,
 * plus the velocity look ahead), finds their zones and crosses them; records the car's zone and
 * may call vehicle_set_state(car, 2) (CAR_FX_SET_STATE) for a player car on surface type 1. */
void car_wheels_place(uint8_t* car, int player, const uint8_t* grp, const CarFx* fx);

/* 0x004168e4: driving input: steering into the yaw rate (body +0x00, +0x14), throttle, braking,
 * reverse, coasting drag and turbo (turbo meter at turbo block +0x32: player +0x848, AI +0x834),
 * with screen flashes, rumble and turbo particles as events. */
void car_drive_input(uint8_t* car, int player, const CarFx* fx);

/* The whole driving model 0x0042d470. */
void car_drive(uint8_t* car, int player, const uint8_t* surfaces, const CarFx* fx);

#endif /* TM2_CAR_H */
