/*
 * mt_bind.h: binding a human car to its world object slot, ported from TM2.EXE 0x0045803c
 * (PS1 decomp: carBindHumanSlot). The AI version is carsys_bind_ai_slot (src/game/carsys_world.c).
 *
 * Globals (pointers in MtBind; object pointers are 32 bit handles):
 *   0x005030a8 human cars, 0x898 each: +0x000 i16 index, +0x002 i16 slot, +0x208 i32
 *   0x00503084 i32 slot count, 0x00503028 i16[] slot owner key, 0x00503048 u32[] slot object,
 *   0x00502fec i32[] slot index, 0x00502fdc u8[] slot kind (0 = human)
 *   0x00503088 u32[] human object, 0x00507570 i32[] human slot, 0x005075b0 i16[] human key,
 *   0x00502f80 u8[] handicap, 0x0050e0fc u8, 0x00502f9c u8 single player
 *   object +0x158 i32 id
 *
 * Verified against the original under emulation (tools/emu/check_match_bind.sh).
 */
#ifndef MT_BIND_H
#define MT_BIND_H

#include <stdint.h>

typedef struct {
    uint8_t* human;             /* 0x005030a8 */
    int32_t* slot_count;        /* 0x00503084 */
    int16_t* slot_owner;        /* 0x00503028 */
    uint32_t* slot_objects;     /* 0x00503048 */
    int32_t* slot_index;        /* 0x00502fec */
    uint8_t* slot_kind;         /* 0x00502fdc */
    uint32_t* human_objects;    /* 0x00503088 */
    int32_t* human_slot;        /* 0x00507570 */
    int16_t* human_key;         /* 0x005075b0 */
    const uint8_t* handicap;    /* 0x00502f80 */
    const uint8_t* keep_208;    /* 0x0050e0fc */
    const uint8_t* singleplayer; /* 0x00502f9c */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    void (*car_init_entity)(void* user, uint8_t* car);                       /* 0x00422e04 */
    uint8_t (*is_multiplayer)(void* user);                                   /* 0x004632b0 (al) */
    void (*car_setup)(void* user, uint8_t* car, int32_t key, uint32_t handicap); /* 0x00437850 */
    void (*car_set_state)(void* user, uint8_t* car, int32_t state);          /* 0x004225a0 */
    void (*object_activate)(void* user, uint32_t object, int32_t on);        /* 0x00492e50 */
    void* user;
} MtBind;

/* 0x0045803c carBindHumanSlot(i, key): the first slot whose owner key equals key (16 bit compare)
 * becomes human car i's: key, slot tables both ways, car +0 / +2, car_init_entity; in
 * multiplayer, unless 0x0050e0fc, car +0x208 = car 0's +0x208 - 1 when that is > 0, else + 1;
 * car_setup(car, the slot's owner key (sign extended), handicap[i]); car_set_state(car, 2) when
 * not single player; slot kind 0, object id = i + 1, car +0 = i, object_activate(object, 1).
 * No matching slot: nothing. */
void mt_bind_human_slot(const MtBind* b, int32_t i, int32_t key);

#endif /* MT_BIND_H */
