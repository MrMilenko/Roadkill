/*
 * carsys_input.h: PC input and render list helpers, ported from TM2.EXE 0x004932a0, 0x0049ecd0,
 * 0x004a14a0, 0x004a14c0, 0x004a2068, 0x004a2380. These are PC only code: the PS1 pairs in
 * tools/match (padClearLatched, padClearActionCache, renderAddWorldModel, renderRemoveWorldModel,
 * padFlush, padUpdate) do something else.
 * Verified against the original under emulation (tools/emu/check_carsys_input.sh).
 */
#ifndef TM2_CARSYS_INPUT_H
#define TM2_CARSYS_INPUT_H

#include <stdint.h>

typedef struct {
    const uint8_t* device_types;    /* 0x00a03a78: u8 type of devices 1, 2, 4 (1 = present) */
    const uint8_t* keys;            /* 0x00a5d810: 256 key states */
    void** models;                  /* 0x00a81c4c: 20 world model slots (NULL = free) */
    void* mouse;                    /* 0x00a82164, handed to the button queries */
    /* 0x004c41c8 / 0x004c4208 / 0x004c41e8 (mouse): nonzero when the button is down */
    int32_t (*button)(void* user, void* mouse, int which);     /* which = 0, 1, 2 in that order */
    void* user;
} CarsysInput;

/* 0x004932a0: device 1 / 2 / 4's type byte, 2 for any other device. */
uint8_t carsys_input_device_type(const CarsysInput* in, int32_t device);
/* 0x0049ecd0: index of the first key down, 0 when none. */
uint8_t carsys_input_first_key(const CarsysInput* in);
/* 0x004a14a0 / 0x004a14c0: puts a model in the first free slot / frees the slot holding it
 * (nothing when the list is full / the model is not in it). */
void carsys_world_model_add(const CarsysInput* in, void* model);
void carsys_world_model_remove(const CarsysInput* in, void* model);
/* 0x004a2068: the first key down when device 1 is present (type 1), else 0. */
uint8_t carsys_input_key(const CarsysInput* in);
/* 0x004a2380: with device 2 present: 5 / 6 / 7 for the first mouse button query that answers,
 * else 0. (The decompiler shows the queries as bytes; the code tests all 32 bits.) */
uint8_t carsys_input_mouse_button(const CarsysInput* in);

#endif /* TM2_CARSYS_INPUT_H */
