/*
 * mt_destroyed.h: a car is destroyed, ported from TM2.EXE 0x0045a39c (PS1 decomp: carDestroyed).
 * Works on both car layouts (src/game/car.h): human (0x898 bytes) and AI (0x880 bytes).
 *
 *                      human            AI
 *   index              i16 +0x000       i16 +0x004
 *   car type           i16 +0x090       i16 +0x030
 *   health (i16)       -                i16 +0x020 (> 0: alive flag 0, else 1)
 *   state block S      +0x09a           +0x068     S+0x0d, S+0x10, S+0x11, S+0x13 u8 flags, S+0x1d u8
 *   timers block T     +0x138           +0x4ec     T+0x02 u8, T+0x1a i16, T+0x26 i16
 *   body B             +0x6c4           +0x708     B+0x20 i32 x, y, z (B+0x28 z), B+0x34 i16
 *   attached object    u32 +0x0c8       u32 +0x094 (its +8 i32 cleared, the field cleared)
 *   carried pickup     u32 +0x890       u32 +0x87c (+0 u8, +7 u8, +0xe i16, +0xc..+0xf)
 *   +0x13c..+0x140 (both): signed bytes +0x13f / +0x140, the killer's player slot
 *
 * Globals: 0x00502f94 i32 (human cars: nothing more happens when set), 0x00502fa4 game mode,
 * 0x0050e0d4 i32 kill counter of player 0, 0x00507570 / 0x0050c28c i32 object slot of each
 * human / AI car, 0x00503048 u32 slot objects (object +0x158 i32 id).
 *
 * Every other system is a callback of one shape (MtDestroyedFn): register arguments eax, edx,
 * ebx, ecx (pointers as host pointers) and stack arguments, in the original's order.
 * Verified against the original under emulation (tools/emu/check_match_destroyed.sh).
 */
#ifndef MT_DESTROYED_H
#define MT_DESTROYED_H

#include <stdint.h>

typedef uint32_t (*MtDestroyedFn)(void* user, uintptr_t eax, uintptr_t edx, uintptr_t ebx, uintptr_t ecx,
                                  const uint32_t* stack, int nstack);

typedef struct {
    const int32_t* flag_2f94;   /* 0x00502f94 */
    const int32_t* game_mode;   /* 0x00502fa4 */
    int32_t* kills;             /* 0x0050e0d4 */
    const int32_t* human_slot;  /* 0x00507570 */
    const int32_t* ai_slot;     /* 0x0050c28c */
    const uint32_t* slot_objects; /* 0x00503048 */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    MtDestroyedFn player_id;    /* 0x00464e84 (slot) */
    MtDestroyedFn net_send3;    /* 0x00473650 (5, kills & 0xffff, id) */
    MtDestroyedFn net_send2;    /* 0x004735b4 (3, id) */
    MtDestroyedFn pickup_drop;  /* 0x00432340 (a, b, 0) */
    MtDestroyedFn sound_mode;   /* 0x0046cc54 () */
    MtDestroyedFn music_call0;  /* 0x0046f974 (pos) */
    MtDestroyedFn music_call;   /* 0x0046f980 (pos, -1) */
    MtDestroyedFn get_990;      /* 0x0046f990 () */
    MtDestroyedFn get_988;      /* 0x0046f988 () */
    MtDestroyedFn sound_play;   /* 0x0046f8b0 (id, a, b, 99) */
    MtDestroyedFn loop_stop;    /* 0x0046f898 (9, -1) */
    MtDestroyedFn sound_stop;   /* 0x0046f918 (0x35) */
    MtDestroyedFn event_type;   /* 0x00474508 () (al) */
    MtDestroyedFn object_activate; /* 0x00492e50 (object, 0) */
    MtDestroyedFn fx_457138;    /* 0x00457138 (pos, 2000) */
    MtDestroyedFn fx_451058;    /* 0x00451058 (int32 v[3] = x, y, 0, 0) */
    MtDestroyedFn car_explode;  /* 0x00421920 (car, human, 4, 0) */
    MtDestroyedFn fx_450984;    /* 0x00450984 (pos, 0) */
    MtDestroyedFn damage;       /* 0x004210e4 (id, 0xf, 0, 2; stack 0) */
    MtDestroyedFn car_state;    /* 0x00420be8 (car, 0, 2, 0; stack human) */
    void* user;
} MtDestroyed;

/* 0x0045a39c carDestroyed(car, human). */
void mt_car_destroyed(uint8_t* car, uint8_t human, const MtDestroyed* e);

#endif /* MT_DESTROYED_H */
