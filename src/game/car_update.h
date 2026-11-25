/*
 * car_update.h: per player car update, ported from TM2.EXE 0x00415f00.
 *
 * Runs the car steps in the original's order. Steps that are ported and checked on their own run
 * directly (car.c); steps that reach outside the car (terrain, driving model, weapons, collisions,
 * effects, models) go through CarUpdateOps so the engine can plug in real ones and the test can
 * stub them. Verified against the original under emulation (tools/emu/check_car_update.sh), with
 * every op stubbed and its calls compared.
 */
#ifndef TM2_CAR_UPDATE_H
#define TM2_CAR_UPDATE_H

#include <stdint.h>

typedef struct {
    /* steps called with (car, player = 1) */
    void (*drive_input)(void* u, uint8_t* car);     /* 0x004168e4 car_drive_input */
    void (*wheels_place)(void* u, uint8_t* car);    /* 0x0042c4d4 car_wheels_place */
    void (*terrain)(void* u, uint8_t* car);         /* 0x0042a020 car_ground_contact + surface effects */
    void (*step_4305fc)(void* u, uint8_t* car);     /* when car +0xAE is set */
    void (*step_420e48)(void* u, uint8_t* car);
    void (*step_430bb4)(void* u, uint8_t* car);
    void (*step_4316b4)(void* u, uint8_t* car);
    void (*drive)(void* u, uint8_t* car);           /* 0x0042d470 car_drive */
    void (*step_422704)(void* u, uint8_t* car);
    void (*step_453c78)(void* u, uint8_t* car);
    void (*step_43e6e0)(void* u, uint8_t* car);
    void (*step_417640)(void* u, uint8_t* car);
    /* other calls */
    void (*pickup_432340)(void* u, int32_t a, int32_t b);    /* 0x00432340(a, b, 1) */
    void (*step_453538)(void* u, int32_t model, int32_t speed);
    void (*init_model)(void* u, uint8_t* car, int32_t model); /* vehicle_init_model 0x00437850(car, model, 1) */
    void (*sound)(void* u, int32_t id);                       /* 0x00436040(id, 0) */
    void (*respawn_obj)(void* u, int32_t model, int32_t arg); /* 0x00470940(model, 3) */
    /* the object at car +0x890 (a pickup or weapon slot record), or NULL */
    uint8_t* (*object)(void* u, uint32_t addr);
    void* user;
    /* globals the update reads */
    uint8_t event_type;     /* 0x00624ec7 */
    int multiplayer;        /* gameloop_check_multiplayer */
    int32_t land_flag;      /* 0x00520e90 */
    int32_t frame_parity;   /* 0x00a0364c (0x004931f4) */
} CarUpdateOps;

/* 0x0045c248 / 0x0045c2a0 (exported; event type from 0x00474508): the height under which a car is
 * out of bounds (6000 in event types 5 / 0x0C, 0xF00 in 6, in 8 for a player car (player != 0)
 * at y < -0x3660 below 0 then 0, else -8000) and the height it is back in bounds (-800 in event
 * type 8, else 0x50). */
int32_t car_oob_low(const uint8_t* car, uint8_t ev, int player);
int32_t car_oob_back(uint8_t ev);

void car_player_update(uint8_t* car, const CarUpdateOps* ops);

#endif /* TM2_CAR_UPDATE_H */
