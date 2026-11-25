/*
 * carsys_world.h: car system helpers that reach world / session globals, ported from TM2.EXE:
 * 0x00450454, 0x0047766c, 0x00455ef0, 0x00432f2c, 0x0045c300, 0x004707b0, 0x0044ee0c,
 * 0x004583c8. Globals through CarsysWorld, other systems as callbacks. Records hold 32 bit
 * handles where the original stores pointers; the engine resolves them (resolve).
 * Verified against the original under emulation (tools/emu/check_carsys_world.sh).
 */
#ifndef TM2_CARSYS_WORLD_H
#define TM2_CARSYS_WORLD_H

#include <stdint.h>

#include "carsys_records.h"

typedef struct {
    const CarRecords* rec;
    /* fx sprites (0x00500304, 150 records of 0x30): +0x0A i8 frame (< 0 free), +0x0B i8 frames,
     * +0x0C i16 loops, +0x10 u32 frame table handle, +0x14 i32 x 3 position, +0x20 i32 x 3
     * velocity */
    uint8_t* fx_sprites;
    int32_t* fx_cooldown;               /* 0x00500030 */
    uint32_t smoke_tables[3];           /* 0x00502246, 0x005022c6, 0x00502346 */
    /* session: 0x009f6c1c / 0x009f6c1d Sweet Tooth / Minion unlocked, 0x00624ea9 humans,
     * 0x00624eaa opponents, 0x00624ead i8 human cars, 0x00624eb5 i8 opponent cars */
    const uint8_t* unlock;              /* 0x009f6c1c (2 bytes) */
    const uint8_t* session;             /* 0x00624ea9: +0 humans, +1 opponents, +4 / +0x0C cars */
    /* moving platform table (pointer at 0x00502f18, 0x38 per slot; +0 i32, +0x34 entity handle) */
    uint8_t* platforms;
    uint8_t** pickups;                  /* 0x00621388: 50 pickup objects */
    int32_t* player_one_flags;          /* 0x004ffff4: 8 i32 */
    uint8_t** ai_objects;               /* 0x005075e8: object of each AI car */
    uint8_t* (*resolve)(void* user, uint32_t handle);   /* entity handle -> entity (0 = none) */
    void (*particles_update)(void* user);               /* 0x004517e0 */
    void (*car_reset_43ac5c)(void* user, uint8_t* car); /* 0x0043ac5c(car, 0) */
    void (*stats_init)(void* user, uint8_t* car, int32_t owner);    /* 0x0043af58(car, owner, 1) */
    void (*obj_register)(void* user, uint8_t* obj);     /* 0x00492e50(obj, 1) */
    void (*car_init_4102f4)(void* user, uint8_t* car);  /* 0x004102f4 */
    void* user;
} CarsysWorld;

/* 0x00450454: counts the explosion cooldown up, updates the impact particles, then steps every
 * live fx sprite: position += velocity, next frame; at the end of the frames loops again while
 * loops > 0 (else frees it), and the smoke tables get 16 more frames when one loop is left. */
void carsys_fx_sprites_update(const CarsysWorld* w);

/* 0x0047766c: car number is selectable: Sweet Tooth (12) and Minion (13) need their unlock,
 * no human or opponent may have it already. */
int32_t carsys_car_is_selectable(const CarsysWorld* w, uint8_t car);

/* 0x00455ef0: puts moving platform 0 (which = 0) or 1 at its start: entity position, angles,
 * matrix (tm2_build_matrix), platform +0 = 0. */
void carsys_track_vehicle_place(const CarsysWorld* w, int32_t which);

/* 0x0045c300: area numbers 4..11, 14, 15, 17, 18. */
uint8_t carsys_area_jump_set(int32_t area);

/* 0x00432f2c (AI car record): Dark Tooth's jump choice. +0x450 / +0x451 = 0; on route segment
 * (i16 +0x458) 2, 11, 40 or 44 inside the jump box (+0x728 x, +0x72C y): +0x451 = 1 when its
 * target's area (human +0xC6 / AI +0x45E) is in carsys_area_jump_set and above 7, else +0x450 = 1. */
void carsys_dark_tooth_jump_select(const CarsysWorld* w, uint8_t* ai);

/* 0x004707b0: pickup ids 401..430: object index's visibility (type 1: +0x14, type 4: +0x12).
 * The original reads the object's type before checking index < 50, and a negative index reaches
 * objects before the table (ORIGINAL BUG); the port returns for both. */
void carsys_pickup_set_visible(const CarsysWorld* w, int32_t id, int32_t index, uint8_t value);

/* 0x0044ee0c: clears the 8 player one flags, human car 0 gets +0x1DC = 1, +0x848 = 0, +0x850 /
 * +0x851 = 0, then 0x0043ac5c(car, 0). (Paired with PS1 PadSetConfig: wrong.) */
void carsys_player_one_reset(const CarsysWorld* w);

/* 0x004583c8: binds AI car index to object slot: the slot tables, the car's +4 index / +6 slot,
 * stats setup from the slot's owner key, kind 1, object id index + 50, object registration. */
void carsys_bind_ai_slot(const CarsysWorld* w, int32_t index, int32_t slot);

#endif /* TM2_CARSYS_WORLD_H */
