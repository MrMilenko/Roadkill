/*
 * carsys_misc.h: small game / settings accessors and one-liners around the car systems, ported
 * from TM2.EXE. Each reaches its global through CarsysMisc so the engine can point it at its
 * own data; calls into systems not ported here are callbacks.
 * Verified against the original under emulation (tools/emu/check_carsys_misc.sh).
 */
#ifndef TM2_CARSYS_MISC_H
#define TM2_CARSYS_MISC_H

#include <stdint.h>

typedef struct {
    const int8_t* event_byte;       /* 0x004f07f9: byte +5 of the random event record 0x004f07f4 */
    uint8_t* flame_markers;         /* 0x004f087c: 20 byte records */
    int32_t* bullets_parity;        /* 0x004f11d0 */
    uint8_t** player_one_object;    /* 0x004f11d4 */
    const int32_t* surface_count;   /* 0x004fb174 */
    uint8_t* section_first;         /* 0x004fe162: u8 first surface per world section */
    uint8_t* section_last;          /* 0x004fe185: u8 last surface per world section */
    int32_t* fx_args;               /* 0x005024d0, 0x005024d4 */
    uint8_t** body_objects;         /* 0x00502910: 15 car body objects by car group (id >> 4) - 1 */
    const int32_t* slam_arm;        /* 0x00502e98 */
    const int32_t* slam_claw;       /* 0x00502e9c */
    const int32_t* game_mode;       /* 0x00502fa4 */
    int32_t* world_flags_i32;       /* 0x0050e0f4 (+0 = 1, +4 = 0) */
    uint8_t* world_flags_u8;        /* 0x0050e0fc (+0 = 1, +1 = 0) */
    const uint8_t* dark_tooth_music;/* 0x00621181 */
    uint8_t* shared_loop;           /* 0x00621188 */
    uint8_t* freeze_objects;        /* 0x00621640: 16 car choice objects, 0x108 each */
    uint8_t** turbo_objects;        /* 0x0062129c: 16 object pointers */
    uint8_t* target_actors;         /* 0x00622b58 */
    /* other systems */
    void (*bullets_move)(void* user);                       /* 0x00412fb8 */
    uint8_t* (*human_object)(void* user, int32_t i);        /* 0x0045ae24 (carsys_human_object) */
    void (*anim_step)(void* user, uint8_t* obj, uint32_t step); /* animSetStep 0x004a0c94 */
    void (*fx_451f54)(void* user, int32_t a, int32_t b);    /* 0x00451f54 */
    void (*world_reset)(void* user);                        /* 0x00454ae0 */
    void (*obj_finish)(void* user, uint8_t* obj);           /* 0x004a3494 */
    void* user;
} CarsysMisc;

/* 0x00411dbc: the signed byte at 0x004f07f9. (Paired with PS1 carObjectCount: wrong, that one
 * is 0x0045afac.) */
int32_t carsys_event_byte(const CarsysMisc* m);
/* 0x00411e14: flame marker record i. */
uint8_t* carsys_flame_marker(const CarsysMisc* m, int32_t i);
/* 0x004133a8: flips the bullets frame parity, moves the bullets, remembers human car 0's object. */
void carsys_bullets_update(const CarsysMisc* m);
/* 0x00419160: body +0x4C (i32) = 0. */
void carsys_body_spin_clear(uint8_t* body);
/* 0x00420bac: clears a car contact record: +2, +3, +4 (u8) and +6 (i16). */
void carsys_push_clear(uint8_t* contact);
/* 0x004325a8: world sections 13 and 15 get surfaces 54..66 (the fallen Eiffel tower). */
void carsys_eiffel_fallen_surfaces(const CarsysMisc* m);
/* 0x0043b4b0: surface_count - 1 - i. */
int32_t carsys_track_surface_index(const CarsysMisc* m, int32_t i);
/* 0x00451a5c: stores b, c in 0x005024d0 / 0x005024d4 then calls 0x00451f54(a, d). (Paired with
 * PS1 gameGetLives: wrong.) */
void carsys_fx_451a5c(const CarsysMisc* m, int32_t a, int32_t b, int32_t c, int32_t d);
/* 0x00453c5c: registers a car body object (type byte 5) for car id's group: body_objects[(id >> 4)
 * - 1] when that is below 15. ORIGINAL BUG not reproduced: ids below 0x10 give a negative index
 * and the original writes before the table; the port ignores them. */
void carsys_body_object_register(const CarsysMisc* m, int32_t id, uint8_t* obj);
/* 0x00453ec0 / 0x00453ec8: Mr. Slam's arm and claw angles. */
int32_t carsys_slam_arm_angle(const CarsysMisc* m);
int32_t carsys_slam_claw_angle(const CarsysMisc* m);
/* 0x00458004: world reset (0x00454ae0) and four world flags. */
void carsys_world_teardown(const CarsysMisc* m);
/* 0x0045c338: the game mode. */
int32_t carsys_game_mode(const CarsysMisc* m);
/* 0x00469cc4: a scene object's choice (p +4 -> object of type 9) gets state value at +7. */
void carsys_scene_choice_set(uint8_t* const* p, uint8_t value);
/* 0x0046fc04 / 0x0046fc88 */
uint8_t carsys_dark_tooth_music(const CarsysMisc* m);
void carsys_set_shared_loop(const CarsysMisc* m, uint8_t v);
/* 0x00470900: every car choice object and turbo object back to step 0. */
void carsys_freeze_reset_all(const CarsysMisc* m);
/* 0x00470940: car id's choice object (group id >> 4, up to 15) to step. ORIGINAL BUG not
 * reproduced: negative ids reach objects before the table; the port ignores them. The original
 * returns whatever eax holds (callers ignore it). */
void carsys_freeze_set_state(const CarsysMisc* m, int32_t id, uint32_t step);
/* 0x00471aec: the target actor table. */
uint8_t* carsys_target_actors(const CarsysMisc* m);
/* 0x004a28a8: the session mode: always 0 on the PC. */
uint8_t carsys_session_mode(void);
/* 0x004a341c: an object whose byte +0x37 is 8 is finished (0x004a3494) and gives 0, others 1.
 * (Paired with PS1 sub_8012A910: wrong.) */
int32_t carsys_obj_finish_if_8(const CarsysMisc* m, uint8_t* obj);

#endif /* TM2_CARSYS_MISC_H */
