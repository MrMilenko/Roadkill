/*
 * mt_cars.h: small car system accessors in TM2.EXE 0x00458034..0x0045c348 (the leaves around
 * cars_update_all 0x00458470), ported from the PC machine code.
 *
 * Globals (all reached through MtCars so the engine can point them at its own data):
 *   0x005075e0 i32 human car count            0x005030a8 human cars, 0x898 each
 *                 human +0x1D8 i32 health, +0x894 i32 player id (0x0050393c - 0x005030a8)
 *   0x0050c2b0 i32 AI car count               0x0050760c AI cars, 0x880 each, +0x58C i32 health
 *   0x005075e8 u32[] AI car world objects (handles)
 *   0x00507590 u32[] per car value read by (i16) index (0x0045b3f0)
 *   0x0050c2b8 u32[] spawn point objects (handles): +0xB0 i32[9] matrix, +0xE0 i16[3] angles,
 *                 +0xE8 i32[3] position
 *   0x0050e0b4 i32[8], 0x0050e0d4 i32[8]: per human counters, set by player id
 *   0x004e7428 i16[14] car type table (initialized data of TM2.EXE)
 *   0x0050db64 i16[15][15], 0x0050dd28 i32[15][15] per car pair tables (beside 0x0050d0d8)
 *   0x00502fa4 i32 game mode, 0x00502f9c u8 single player (mode == 0)
 *   u8 0x0050c2c4, 0x00502f7c, 0x00502f88, 0x00502f94; i32 0x00502f8c (watched player),
 *   0x00502f90 (watched AI car), 0x00502f78
 *
 * Verified against the original under emulation (tools/emu/check_match_cars.sh).
 */
#ifndef MT_CARS_H
#define MT_CARS_H

#include <stdint.h>

typedef struct {
    int32_t* human_count;       /* 0x005075e0 */
    uint8_t* human;             /* 0x005030a8 */
    int32_t* ai_count;          /* 0x0050c2b0 */
    uint8_t* ai;                /* 0x0050760c */
    uint32_t* ai_objects;       /* 0x005075e8 */
    uint32_t* car_value;        /* 0x00507590 */
    uint32_t* spawn_objects;    /* 0x0050c2b8 */
    int32_t* counter_b4;        /* 0x0050e0b4 */
    int32_t* counter_d4;        /* 0x0050e0d4 */
    const int16_t* type_table;  /* 0x004e7428 */
    int16_t* pair_i16;          /* 0x0050db64 */
    int32_t* pair_i32;          /* 0x0050dd28 */
    int32_t* game_mode;         /* 0x00502fa4 */
    uint8_t* singleplayer;      /* 0x00502f9c */
    uint8_t* flag_c2c4;         /* 0x0050c2c4 */
    uint8_t* flag_2f7c;         /* 0x00502f7c */
    uint8_t* flag_2f88;         /* 0x00502f88 */
    uint8_t* flag_2f94;         /* 0x00502f94 */
    int32_t* watched_player;    /* 0x00502f8c */
    int32_t* watched_ai;        /* 0x00502f90 */
    int32_t* state_2f78;        /* 0x00502f78 */
    uint8_t* (*resolve)(void* user, uint32_t handle);   /* object handle -> memory */
    uint8_t (*is_multiplayer)(void* user);              /* 0x004632b0 */
    void* user;
} MtCars;

/* 0x00458034 */ void mt_set_human_count(const MtCars* c, int32_t n);
/* 0x0045ae2c */ uint32_t mt_ai_object(const MtCars* c, int32_t i);
/* 0x0045b1e8: spawn object i (if any) gets position pos[3] and angles ang[3], then its matrix is
 * rebuilt from the angles (math_build_matrix 0x0048c360). */
void mt_spawn_point_set(const MtCars* c, int32_t i, const int32_t* pos, const int16_t* ang);
/* 0x0045b2f4 / 0x0045b2fc / 0x0045b55c / 0x0045bb50: u8 flags */
uint8_t mt_flag_c2c4(const MtCars* c);
uint8_t mt_flag_2f7c(const MtCars* c);
uint8_t mt_flag_2f88(const MtCars* c);
uint8_t mt_flag_2f94(const MtCars* c);
/* 0x0045b3f0: car_value[(int16_t)i] */
uint32_t mt_car_value(const MtCars* c, int32_t i);
/* 0x0045b3f8: does nothing */
void mt_nop_45b3f8(void);
/* 0x0045b54c / 0x0045b554 / 0x0045c340 */
int32_t mt_watched_player(const MtCars* c);
int32_t mt_watched_ai(const MtCars* c);
int32_t mt_state_2f78(const MtCars* c);
/* 0x0045b978: human car i's player id */
void mt_human_set_id(const MtCars* c, int32_t i, int32_t id);
/* 0x0045b988: the human car with player id, else the first human car */
uint8_t* mt_human_by_id(const MtCars* c, int32_t id);
/* 0x0045b9c4 / 0x0045b9cc / 0x0045b9d4 */
int32_t mt_counter_d4(const MtCars* c, int32_t i);
int32_t mt_counter_b4(const MtCars* c, int32_t i);
void mt_counter_b4_inc(const MtCars* c, int32_t i);
/* 0x0045b9dc / 0x0045ba20: counter of every human car whose player id is id = v */
void mt_counter_b4_set_by_id(const MtCars* c, int32_t id, int32_t v);
void mt_counter_d4_set_by_id(const MtCars* c, int32_t id, int32_t v);
/* 0x0045bab8: cars still alive (health > 0): multiplayer: human cars 1.. (car 0 not counted),
 * else AI cars. */
int32_t mt_cars_alive(const MtCars* c);
/* 0x0045bb20: index of type (16 bit compare) in the 14 entry type table, 14 when absent */
int32_t mt_car_type_index(const MtCars* c, int32_t type);
/* 0x0045c1f4 / 0x0045c20c: pair tables [a][b] (15 x 15). The original returns only ax for the
 * i16 table (the high half of eax is the high half of a * 15). */
int16_t mt_pair_i16(const MtCars* c, int32_t a, int32_t b);
int32_t mt_pair_i32(const MtCars* c, int32_t a, int32_t b);
/* 0x0045c328: game mode and the single player flag (mode == 0); 0x0045c348 reads the flag */
void mt_set_game_mode(const MtCars* c, int32_t mode);
uint8_t mt_singleplayer(const MtCars* c);

#endif /* MT_CARS_H */
