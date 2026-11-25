/*
 * mt2_carsnd.h: per car sound update, the car pair vector table and the AI cars' respawn of
 * TM2.EXE, ported from the PC machine code.
 *
 *   0x0045c1c4 car_pair_vec      &0x0050d0d8[car * 0xb4 + other * 0xc] (15 x 15 records of 12)
 *   0x0045b5f0 carSoundUpdate    one car's engine / skid / horn sounds (PS1 carSoundUpdate)
 *   0x0045c56c ai_cars_respawn   restart the AI cars at one of four per event start sets
 *
 * Verified against the original under emulation (tools/emu/check_mt2_carsnd.sh).
 */
#ifndef MT2_CARSND_H
#define MT2_CARSND_H

#include <stddef.h>
#include <stdint.h>

#include "mt_audio.h"

/* ---- 0x0045c1c4 ---- */
/* byte offset car * 0xb4 + other * 0xc (32 bit wrap) and the record pointer in the table */
uint32_t mt2_car_pair_offset(int32_t car, int32_t other);
uint8_t* mt2_car_pair_vec(uint8_t* table_50d0d8, int32_t car, int32_t other);

/* ---- 0x0045b5f0 carSoundUpdate ----
 * car: the car record (eax ignored, edx = car, bl = second).  Two layouts, chosen by second:
 *                    second == 0            second != 0
 *   flags (u8)       car +0x68              car +0x9a
 *   engine record    car +0x708             car +0x6c4      (+0x08 i32 speed 16.16, +0x20 music object)
 *   stats            car +0x4ec             car +0x138      (+0xb4 i32 top speed, divisor)
 *   horn record      car +0xcc              car +0x21c      (+0x0c i32 16.16 volume, +0x0e i16 on)
 *   type             i16 car +0x30          i16 car +0x90
 *   instance id      i32 car +0x02 >> 16    (i32)i16 car +0x00
 *   flag bytes used: +0x00 muted, +0x02 / +0x0b / +0x21 / +0x22 (sound 1), +0x13 (sound 7, cleared),
 *   +0x0e (sound 0x15 variant), +0x1a / +0x22 (sound 0x20).
 * Calls: 0x0046cc54 (the flag 0x00520e90, read here), 0x0046f974 / 0x0046f980 (both 0x004b8010
 * (engine +0x20, 0 / -1): the music callback), the sound wrappers of mt_audio.c. */
typedef struct {
    const MtAudio* audio;           /* sound wrappers 0x0046f8b0.. (mt_audio.c) */
    const uint32_t* flag_520e90;    /* 0x00520e90, read by 0x0046cc54 */
    const int32_t* player_mode;     /* 0x00502fa4 */
    uint32_t (*music_call)(void* user, uint8_t* obj, uint32_t edx); /* 0x004b8010 (eax, edx) */
    void* user;
} Mt2CarSnd;

void mt2_car_sound_update(const Mt2CarSnd* e, uint8_t* car, uint8_t second);

/* ---- 0x0045c56c ai_cars_respawn ----
 * Per event (game_get_event_type 1..8) and start set (0..3) the original sets, for AI cars 0..3
 * (car k = 0x0050760c + k * 0x880), i16 +0x52c and i32 +0x5bc (route nodes), then on an odd
 * rand() the start point 0x005030bc / c0 / c4 and 0x00502fd8 = 1. Those values are immediates in
 * the original's code: they are not in this source but read from the player's TM2.EXE by
 * mt2_respawn_table_load (a small register machine over the 32 case blocks, rule 4). */
typedef struct {
    int16_t node52c[4];
    int32_t node5bc[4];
    int32_t start[3];     /* 0x005030bc, 0x005030c0, 0x005030c4 */
    int32_t start_flag;   /* 0x00502fd8 */
} Mt2RespawnSet;

typedef struct {
    Mt2RespawnSet set[8][4];   /* [event - 1][start set] */
} Mt2RespawnTable;

/* exe: the whole player's TM2.EXE. 0 on success, else -1 with a reason in err. */
int mt2_respawn_table_load(Mt2RespawnTable* t, const uint8_t* exe, size_t size, char* err, size_t errlen);

typedef uint32_t (*Mt2Fn)(void* user, uint32_t eax, uint32_t edx, uint32_t ebx);

typedef struct {
    const Mt2RespawnTable* table;
    int32_t* tries;            /* 0x00502fa8 */
    uint8_t* flag_502f9c;      /* 0x00502f9c */
    uint8_t* used;             /* 0x00502fa0 [4] (indexed by rand() % 4, signed) */
    int32_t* v502fb4;          /* 0x00502fb4 = 3 */
    int32_t* v502fb8;          /* 0x00502fb8 = 3 */
    int32_t* v502fbc;          /* 0x00502fbc = 1 */
    int32_t* v502fc0;          /* 0x00502fc0 = 1 */
    int32_t* start_flag;       /* 0x00502fd8 */
    int32_t* start;            /* 0x005030bc [3] */
    const uint32_t* car_objects; /* 0x005075e8 [count] object handles (object +8 i32 = 1) */
    uint8_t* cars;             /* 0x0050760c, stride 0x880: +0x30 i16, +0x52c i16, +0x58c i32,
                                  +0x5bc i32, +0x83e u8 */
    const int32_t* car_count;  /* 0x0050c2b0 (read again every loop step) */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    uint32_t (*car_addr)(void* user, uint8_t* car); /* the address the callbacks get for a car */
    Mt2Fn rand;                /* 0x004899c2 () */
    Mt2Fn schedule_event;      /* 0x0046ca68 (1, 0) rtScheduleEvent */
    Mt2Fn freeze_reset_all;    /* 0x00470900 () */
    Mt2Fn debris_clear;        /* 0x00489a70 () */
    Mt2Fn start_nodes_clear;   /* 0x0043bd60 () */
    Mt2Fn ai_car_start;        /* 0x0043af58 (car, i16 car +0x30, 1) */
    Mt2Fn event_type;          /* 0x00474508 () -> al */
    Mt2Fn ai_place;            /* 0x00432940 (car) */
    void* user;
} Mt2Respawn;

void mt2_ai_cars_respawn(const Mt2Respawn* e);

#endif /* MT2_CARSND_H */
