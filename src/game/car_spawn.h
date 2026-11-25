/*
 * car_spawn.h: player car spawn, ported from TM2.EXE 0x00437ac4.
 *
 * Clears the car's controls and motion, picks a spawn node (spawn.c; in game modes >= 5 it
 * re-picks until no other car is within 400), places the car on the node facing a random
 * neighbor, resets the matrices (identity with 4095 on the diagonal, as the original's constants),
 * runs the stats setup, initializes the four probes on the zone under the car, settles it with
 * one driving model step and writes the car's collision box into its scene entity.
 * Verified against the original under emulation (tools/emu/check_car_spawn.sh).
 *
 * Spawn nodes are the level's PTS nodes (0x004fe1e4, 24 bytes each): i32 x, i32 y, i16 z at +8,
 * i16 neighbors at +0x0A.., u8 neighbor count at +0x16, u8 area at +0x17.
 */
#ifndef TM2_CAR_SPAWN_H
#define TM2_CAR_SPAWN_H

#include <stdint.h>

#include "car.h"
#include "car_stats.h"
#include "spawn.h"

typedef struct {
    const uint8_t* nodes;       /* 0x004fe1e4 */
    int32_t node_count;         /* 0x004fe1b0 */
    SpawnList* spawns;          /* 0x004fe1b4 */
    int32_t spawn_count;        /* byte 0x004fe1e1 */
    int32_t mode;               /* 0x00502fa4 */
    const uint8_t* grp;         /* the level's GRP */
    const uint8_t* surfaces;    /* GRP + 0x125E, for the settling driving model step */
    const CarFx* fx;
    int multiplayer;                /* gameloop_check_multiplayer, for the stats setup */
    CarStatsRotateFn rotate;        /* car 0xC0's scene rotations in the stats setup */
    /* other cars, for the spacing check (game modes >= 5) */
    int32_t car_count;
    uint8_t* (*car_at)(void* user, int32_t index);
    /* the car's scene entity (vehicle_get_model_slot), gets the collision box */
    uint8_t* (*entity)(void* user, uint8_t* car);
    void* user;
} CarSpawnEnv;

/* 0x004300e8: clears a 4 probe block (body + 4 states), current tile -1, +0x40.. = 0xF */
void car_probes_clear(uint8_t* probes);

/* 0x00437ac4 */
/* Exported pieces of the spawn (each its own function in TM2.EXE):
 *   0x00437ff8 car_controls_clear(block)      0x0043a754 car_probe_step_height(probes, id): +8
 *   0x00430368 car_probes_floor(probes, z)     0x0043033c car_probes_zone(probes, zone, GRP)
 *   0x00418b60 car_motion_reset(car, player): controls +1 = 0, body +0x14 = 0, +0x4C = 0,
 *              +0x38..+0x40 = +0x2C..+0x34 */
void car_controls_clear(uint8_t* controls);
void car_probe_step_height(uint8_t* probes, uint16_t id);
void car_probes_floor(uint8_t* probes, int32_t z);
void car_probes_zone(uint8_t* probes, int32_t zone, const uint8_t* grp);
void car_motion_reset(uint8_t* car, int player);

void car_spawn_player(uint8_t* car, const CarSpawnEnv* env);

#endif /* TM2_CAR_SPAWN_H */
