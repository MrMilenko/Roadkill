/*
 * car_input.h: player car input, ported from TM2.EXE 0x00417734 (old name vehicle_read_input).
 *
 * Turns input actions into the player car's control bytes, runs the direction combo detector
 * (special moves), handles the spectator camera while out, and toggles camera tracking.
 * Verified against the original under emulation (tools/emu/check_car_input.sh).
 *
 * Input action ids as this function reads them (meanings from what the car does with them):
 *   0x11 -> +0xD0, 0x12 -> +0xD1, 0x3E -> +0xD3 (0x3C / 0x3D override to 1 / 2), 0x49 -> +0xD5,
 *   0x4A -> +0xD6, 0x4B -> +0xD7, 0x4C -> +0xE2, 0x40 / 0x41 / 0x43 -> +0xD4 = 2 / 3 / 4,
 *   0x22 / 0x23 / 0x24 / 0x25 / 0x26 / 0x27 / 0x28 / 0x29 / 0x2A / 0x2B / 0x2C / 0x2D,
 *   0x2E..0x35 -> +0xE5 = 100 + n, 0x46 / 0x47 / 0x48 spectator reset / next / previous,
 *   0x51..0x54 combo directions (read with the combo block's player slot).
 *
 * Combos (sequence of quick direction taps, see car_combo_update): codes 100..0x6A go to car
 * +0x854; the 8 tap sequence 1 0 3 2 0 0 1 1 converts stored points into health.
 */
#ifndef TM2_CAR_INPUT_H
#define TM2_CAR_INPUT_H

#include <stdint.h>

enum {
    CAR_IN_SET_STATE = 1,   /* vehicle_set_state(car, a); car NULL = player car 0 (0x005030a8) */
    CAR_IN_SOUND = 2,       /* 0x00436040(a, 0) */
    CAR_IN_SCREEN = 3,      /* display_transition(a, 0, 0, b) */
    CAR_IN_CAMERA = 4       /* camera_set_track(a, b, c) */
};

typedef struct {
    /* input_check_ready(id, slot): action state (slot 0 = this machine's player) */
    uint8_t (*input)(void* user, int id, int slot);
    void (*event)(void* user, int id, uint8_t* car, int32_t a, int32_t b, int32_t c);
    void* user;
    /* spectator state the original keeps in globals */
    int32_t watch_player;   /* 0x00502f8c */
    int32_t watch_ai_flag;  /* 0x00502f88 */
    int32_t watch_ai;       /* 0x00502f90 */
    int32_t mode;           /* 0x00502fa4 (game mode, < 5 cycles AI cars) */
    int32_t ai_count;       /* 0x0050c2b0 */
    int32_t player_count;   /* 0x005075e0 */
    int32_t spectate;       /* 0x004ffedc[0x20] */
} CarInputEnv;

/* 0x0044efa0 (old name vehicle_weapon_update): combo block at car +0x7F0: +0 i32 player slot,
 * +4.. hold ticks, +0x14.. release ticks, +0x24.. pressed, +0x28.. latched (per direction 0..3),
 * +0x2C i32 tap count, +0x30.. i32 taps (-1 terminated, 8 max), +0x54 active. */
/* 0x0044f100 car_combo_reset (exported): clears a tap sequence block (+0x2C = 0, +0x54 = 0, +0x30 = -1). */
void car_combo_reset(uint8_t* combo);
/* 0x0045b5a8 spectate_player / 0x0045b564 spectate_ai (exported): the watched player / AI car
 * (wrapped into range; spectate_ai tests the old index, as the original does), the AI watch flag,
 * and the view mode event (CAR_IN_SET_STATE, NULL car, 2 / 7). */
void car_watch_player(CarInputEnv* env, int32_t v);
void car_watch_ai(CarInputEnv* env, int32_t v);

void car_combo_update(uint8_t* combo, CarInputEnv* env);

/* 0x00417734 */
void car_read_input(uint8_t* car, CarInputEnv* env);

#endif /* TM2_CAR_INPUT_H */
