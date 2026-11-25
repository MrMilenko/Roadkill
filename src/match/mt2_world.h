/*
 * mt2_world.h: cars_update_all / worldUpdate, TM2.EXE 0x00458470..0x00459f39 (PS1 decomp pair:
 * worldUpdate in world_update.c), ported from the PC machine code.
 *
 * The per frame update of every car: human cars (blocks of 0x898 at 0x005030a8, count
 * 0x005075e0), AI cars (blocks of 0x880 at 0x0050760c, count 0x0050c2b0), the multiplayer
 * ranking / AI mood pass or the single player timers, damage smoke states and death, radar
 * blips, speed gauges, collisions, hit responses, the car pair table (0x0050c2c8..0x0050e0a8:
 * vectors, normals, bearings, distances), then the end of frame display / sound / effect steps.
 *
 * Everything the function touches in the game's data lives in the block 0x00502f00..0x0050e200
 * (handed over as `blk`, addressed by the original's absolute addresses) plus the input latch
 * 0x004e7420. Object handles (0x00503088, 0x005075e8, the return of targetObject) go through
 * `resolve`. Every call to other code goes through `fn[]`, in the original's order, with the
 * original's register arguments (pointers into blk / objects as host pointers, values as uint32).
 *
 * Verified against the original under emulation (tools/emu/check_mt2_world.sh).
 *
 * Fields read / written (absolute address of car 0; human +i*0x898, AI +j*0x880):
 *   human: 0x005030a8 i16 id (0 = local player), +0x9a u8 dead (0x00503142), +0xa5 / +0xd6 /
 *     +0xd7 / +0xe2 u8 input flags, +0xd3 u8 view key (1 back, 2 forward), +0xd4 u8 view wanted,
 *     +0x106 i8 view mode, 0x00503138 i16 car type, 0x00503166 u8, 0x005031e4 i8 smoke arg,
 *     0x005031e5 i8 smoke state (0..2, 4 = none), 0x0050321a i16 max health, 0x0050327c i32
 *     collide on, 0x00503280 i32 health, 0x005032c6 u8 hit active, 0x005032d4 i16 hit frames,
 *     0x00503538.. u8 x6 (step 0x24) cleared, 0x00503776 i16 speed, 0x0050378c i32 x3 position,
 *     0x00503774.. / 0x005037a4.. copied to 0x00503784.. / 0x00503858.. (previous state).
 *   AI: 0x0050760d i8 mood (1..3), 0x00507610 i16 sound id, 0x00507616 i16 rank, 0x00507618 i16
 *     frames, 0x0050761a / 0x0050761c / 0x0050761e i16 mood timers, 0x00507634 i32 distance,
 *     0x0050763c i16 car type, 0x0050763e u8, 0x00507649 u8, 0x00507674 u8 dead, 0x005076da u8
 *     hit active, 0x005076de / 0x005076e8 i16, 0x00507afc i8 / 0x00507afd i8 smoke arg / state,
 *     0x00507b32 i16 max health, 0x00507b7c i16, 0x00507b80 / 0x00507b82 / 0x00507b84 i16,
 *     0x00507b86 i16, 0x00507b94 i32 active, 0x00507b98 i32 health, 0x00507d34 i32 x3 position.
 *   globals: 0x00502f78, 0x00502f80 u8[], 0x00502f8c, 0x00502f90 watched AI car, 0x00502f94,
 *     0x00502f9c u8 single player, 0x00502fa4 game mode, 0x00502fac / 0x00502fb0 timers,
 *     0x00502fd8, 0x00502fdc u8[] slot is AI, 0x00502fec i32[] slot car index, 0x00503084 slot
 *     count, 0x00507570 / 0x0050c28c sound handles, 0x005075c0 i32[] speed gauges, 0x0050c2b4
 *     AI alive count, 0x0050c2c0, 0x0050e0b0, 0x0050e0f4 / 0x0050e0f8.
 */
#ifndef MT2_WORLD_H
#define MT2_WORLD_H

#include <stdint.h>

#define MT2W_BASE 0x00502f00u
#define MT2W_END 0x0050e200u

/* callees, in the order of fn[] (address, register arguments) */
enum {
    MT2W_EVENT_TYPE,      /* 0x00474508 () -> al */
    MT2W_456_06C,         /* 0x0045606c () */
    MT2W_472_B70,         /* 0x00472b70 () */
    MT2W_READ_INPUT,      /* 0x00417734 car_read_input (car, obj handle) */
    MT2W_SET_VIEW,        /* 0x004225a0 car_set_view_mode (car, mode) */
    MT2W_436_F60,         /* 0x00436f60 (0) */
    MT2W_436_F80,         /* 0x00436f80 (0) */
    MT2W_436_F98,         /* 0x00436f98 (0) */
    MT2W_PLAYER_UPDATE,   /* 0x00415f00 car_player_update (car, obj handle) */
    MT2W_COPY_WORDS,      /* 0x0048ce88 copy_words (dst, src, n) */
    MT2W_NET_IS_MULTI,    /* 0x004632b0 () -> al */
    MT2W_473_254,         /* 0x00473254 (cars, 1) */
    MT2W_463_F78,         /* 0x00463f78 () */
    MT2W_473_534,         /* 0x00473534 (1) */
    MT2W_CAR_SETUP,       /* 0x00437850 player car setup (car, type, 0x00502f80[id]) */
    MT2W_CHOICE_STATE,    /* 0x0047068c (type, state, smoke arg) */
    MT2W_RAND,            /* 0x004899c2 () -> eax */
    MT2W_RADAR_BLIP,      /* 0x00436a40 (type, int32[3]* offset, view word, screen) */
    MT2W_CAR_SOUND,       /* 0x0045b5f0 carSoundUpdate (sound handle, car, human) */
    MT2W_BODY_SHOW,       /* 0x00453c00 (type) */
    MT2W_BODY_HIDE,       /* 0x00453c38 (type) */
    MT2W_SMOKE,           /* 0x00451058 (position, 0) */
    MT2W_DESTROYED,       /* 0x0045a39c carDestroyed (car, human) */
    MT2W_SCHEDULE,        /* 0x0046ca68 rtScheduleEvent (event, delay) */
    MT2W_463_2D4,         /* 0x004632d4 () -> eax */
    MT2W_432_7FC,         /* 0x004327fc () */
    MT2W_432_838,         /* 0x00432838 (0) */
    MT2W_TICKS,           /* 0x004931d4 frame ticks () -> ax */
    MT2W_APPROACH,        /* 0x0048caec (current, target, rate) -> eax */
    MT2W_46C_C54,         /* 0x0046cc54 () -> eax */
    MT2W_CHASE_CAMERA,    /* 0x0043cdc0 (car, i) */
    MT2W_VIEWER_POS,      /* 0x00494c5c (int32[3]*, i) */
    MT2W_45C_56C,         /* 0x0045c56c () */
    MT2W_45C_350,         /* 0x0045c350 () */
    MT2W_TIMEOUT,         /* 0x00493694 (0) -> al */
    MT2W_AI_UPDATE,       /* 0x00422e08 ai_car_update (car) */
    MT2W_DISPLAY_CB,      /* 0x0046f918 (id) */
    MT2W_432_5C8,         /* 0x004325c8 () */
    MT2W_43A_6D8,         /* 0x0043a6d8 (0x00507af8) */
    MT2W_438_320,         /* 0x00438320 (AI car 0, 0) */
    MT2W_421_920,         /* 0x00421920 (AI car 0, 0, 12, 0) */
    MT2W_SOUND_9,         /* 0x0046f898 (9, id) */
    MT2W_TARGET_OBJECT,   /* 0x0044df10 () -> object handle */
    MT2W_RANDOM_EVENT,    /* 0x004498d8 () */
    MT2W_COLLIDE,         /* 0x0049cec0 car_collide (obj, a, 1, b, stack c) */
    MT2W_HITS_RESOLVE,    /* 0x00419604 (car, human, flag) */
    MT2W_HITS_GATHER,     /* 0x0041f40c (car, human) */
    MT2W_HIT_PUSH,        /* 0x0041fc3c (car, human) */
    MT2W_HIT_RECOVER,     /* 0x0042026c (car, human) */
    MT2W_NORMALIZE,       /* 0x0048b5a0 (in vec, out vec) */
    MT2W_ATAN2,           /* 0x0048b71c (x, y) -> eax */
    MT2W_ANGLE_WRAP,      /* 0x0048c9f4 (int32*) */
    MT2W_46F_C04,         /* 0x0046fc04 () -> al */
    MT2W_46F_CB0,         /* 0x0046fcb0 (0x0046fcc0 or 0) */
    MT2W_493_038A,        /* 0x00493038 () first */
    MT2W_46F_CA8,         /* 0x0046fca8 () -> al */
    MT2W_46F_CA0,         /* 0x0046fca0 (0) */
    MT2W_TRANSITION,      /* 0x0046f8b0 (0x3f, 0, 0, 99) */
    MT2W_46F_94C,         /* 0x0046f94c () */
    MT2W_46F_958,         /* 0x0046f958 () */
    MT2W_493_038B,        /* 0x00493038 () second */
    MT2W_44A_59C, MT2W_44A_D14, MT2W_44B_B18, MT2W_44B_BE4, MT2W_44B_D3C, MT2W_44C_EE0,
    MT2W_44D_1F4, MT2W_454_50C, /* 0x0044a59c .. 0x0045450c () */
    MT2W_455_050,         /* 0x00455050 () when the event type is 6 */
    MT2W_N
};

typedef uintptr_t (*Mt2WFn)(void* user, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e);

typedef struct {
    uint8_t* blk;            /* 0x00502f00..0x0050e200 */
    int32_t* input_latch;    /* 0x004e7420 */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    Mt2WFn fn[MT2W_N];
    void* user;
} Mt2World;

/* 0x00458470 */
void mt2_world_update(const Mt2World* w);

#endif /* MT2_WORLD_H */
