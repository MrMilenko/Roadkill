/*
 * carsys_records.h: car record accessors, ported from TM2.EXE 0x0045a8bc..0x0045b3dc.
 *
 * The game keeps two car arrays (layouts in car.h) and a table of car world object slots:
 *
 *   human cars    0x005030a8, 0x898 each (8 max), count 0x005075e0
 *                 +0x106 u8 camera view (7 = watching an AI car), +0x1D8 i32 health,
 *                 +0x6CC i32 speed (16.16, the integer part is i16 +0x6CE), +0x6E4 i32 x, y, z,
 *                 +0x6F0 / +0x6F4 / +0x6F8 i32 angles (low halves read as i16)
 *   AI cars       0x0050760c, 0x880 each (9 max), count 0x0050c2b0
 *                 +0x4EC stats block, +0x58C i32 health, +0x710 i32 speed, +0x728 i32 x, y, z
 *   watched AI    0x00502f90 (i32 index)
 *   object slots  count 0x00503084; 0x00503048 object pointer per slot (object +0x158 i32 id),
 *                 0x00503028 i16 owner key, 0x00502fec i32 index in its array,
 *                 0x00502fdc u8 kind (0 human, 1 AI, anything else: none)
 *   0x00503088    object pointer per human car (directly after the 16 slot pointers)
 *   0x00502fb4    i32 per AI car: the AI car it is after, -1 = the nearest human
 *   0x00507570    i32 per human: its object slot; 0x0050c28c i32 per AI car: its object slot
 *   0x00502f98    i32 AI difficulty; 0x0050c2c0 u8 boss defeated; 0x005075c0 i32 per human:
 *                 smoothed display speed
 *
 * Every array is reached through CarRecords so the engine can point it at its own data.
 * Verified against the original under emulation (tools/emu/check_carsys_records.sh).
 */
#ifndef TM2_CARSYS_RECORDS_H
#define TM2_CARSYS_RECORDS_H

#include <stdint.h>

#define CARSYS_HUMAN_SIZE 0x898
#define CARSYS_AI_SIZE 0x880

typedef struct {
    uint8_t* human;             /* 0x005030a8 */
    int32_t* human_count;       /* 0x005075e0 */
    uint8_t* ai;                /* 0x0050760c */
    int32_t* ai_count;          /* 0x0050c2b0 */
    int32_t* watched_ai;        /* 0x00502f90 */
    int32_t* slot_count;        /* 0x00503084 */
    /* 0x00503048: slot object pointers. The original reads one entry past the count when a
     * lookup by owner fails (carsys_object_from_owner), so the array needs slot_count + 1
     * entries; in the original that entry is human_objects[0] when 16 slots are in use. */
    uint8_t** slot_objects;
    int16_t* slot_owner;        /* 0x00503028 */
    int32_t* slot_index;        /* 0x00502fec */
    uint8_t* slot_kind;         /* 0x00502fdc */
    uint8_t** human_objects;    /* 0x00503088 */
    int32_t* ai_targets;        /* 0x00502fb4 */
    int32_t* human_slot;        /* 0x00507570 */
    int32_t* ai_slot;           /* 0x0050c28c */
    int32_t* difficulty;        /* 0x00502f98 */
    uint8_t* boss_defeated;     /* 0x0050c2c0 */
    int32_t* hud_speed;         /* 0x005075c0 */
} CarRecords;
/* The frame's 60 Hz ticks (frame_get_ticks60 0x004931d4) come from tm2_frame_ticks60. */

/* 0x0045a8bc: the boss defeated flag. */
uint8_t carsys_boss_defeated(const CarRecords* r);
/* 0x0045acb8: index of the living (health > 0) human car nearest pos (|dx| + |dy| < 0x7FFF),
 * skipping index exclude; 0 when none. */
int32_t carsys_human_nearest(const CarRecords* r, const int32_t* pos, int32_t exclude);
/* 0x0045ad38: index of the living AI car nearest pos, 0 when none. */
int32_t carsys_ai_nearest_alive(const CarRecords* r, const int32_t* pos);
/* 0x0045adac / 0x0045add0: a car's position. */
void carsys_human_pos(const CarRecords* r, int32_t i, int32_t* out);
void carsys_ai_pos(const CarRecords* r, int32_t i, int32_t* out);
/* 0x0045adf8: a human car's three angles as i16. */
void carsys_human_angles(const CarRecords* r, int32_t i, int16_t* out);
/* 0x0045ae24: a human car's world object. */
uint8_t* carsys_human_object(const CarRecords* r, int32_t i);
/* 0x0045ae34: clears a human's smoothed display speed (index is the low 16 bits). */
void carsys_hud_speed_reset(const CarRecords* r, int32_t i);
/* 0x0045ae44: eases a human's smoothed display speed toward its current speed with
 * tm2_angle_step at rate 13 / 9 / 5 / 3 for a gap over 15 / 10 / 5 / else. */
void carsys_hud_speed_smooth(const CarRecords* r, int32_t i);
/* 0x0045aee8: a human car's speed for the speedometer (the watched AI car's in view 7):
 * (speed >> 16) / 32 * 100 / (ticks60 * 19). */
int32_t carsys_display_speed(const CarRecords* r, int32_t i);
/* 0x0045af58: the same for an AI car. */
int32_t carsys_ai_display_speed(const CarRecords* r, int32_t i);
/* 0x0045afac: number of car object slots. */
int32_t carsys_object_count(const CarRecords* r);
/* 0x0045afb4 / 0x0045afbc: human car count and record. */
int32_t carsys_human_count(const CarRecords* r);
uint8_t* carsys_human_get(const CarRecords* r, int32_t i);
/* 0x0045afc8: the car AI car i is after (*human = 1: the human nearest it, else an AI car). */
uint8_t* carsys_ai_target(const CarRecords* r, int32_t i, uint8_t* human);
/* 0x0045b028: the record of object slot i, NULL when its index is negative or its kind is not
 * 0 / 1 (*human is then left alone). */
uint8_t* carsys_from_slot(const CarRecords* r, int32_t i, uint8_t* human);
/* 0x0045b078 / 0x0045b080 / 0x0045b0a0: AI car count, record (index outside 0..8 -> 0), the
 * watched AI car. */
int32_t carsys_ai_count(const CarRecords* r);
uint8_t* carsys_ai_get(const CarRecords* r, int32_t i);
uint8_t* carsys_ai_watched(const CarRecords* r);
/* 0x0045b0b8: the object of the slot whose owner key equals owner (16 bit compare); not found:
 * slot_objects[slot_count]. */
uint8_t* carsys_object_from_owner(const CarRecords* r, int32_t owner);
/* 0x0045b0ec: finds the slot holding object; on a slot of kind 0 / 1 sets *human and *index and
 * returns 1, else returns 0 leaving both alone. */
uint8_t carsys_find_by_object(const CarRecords* r, const uint8_t* object, uint8_t* human, int32_t* index);
/* 0x0045b154: the id (object +0x158) of car i's object; human != 0 picks the human table. */
int32_t carsys_object_id(const CarRecords* r, int32_t i, uint8_t human);
/* 0x0045b178: finds the slot whose object id is id. Quirk kept: a matching slot of another kind
 * still writes *index and the search goes on. */
uint8_t carsys_find_by_id(const CarRecords* r, int32_t id, uint8_t* human, int32_t* index);
/* 0x0045b370: sets the AI difficulty and every AI car's speed percentage (0: 75, 2: 140,
 * else 120) through 0x0043a93c (stats +0x76 = +0x78 = percent). */
void carsys_ai_set_difficulty(const CarRecords* r, uint32_t level);
/* 0x0045b3dc */
int32_t carsys_ai_difficulty(const CarRecords* r);
/* 0x0043a93c: stats +0x76 and +0x78 (i16) = percent, then falls through into car_stats_derive
 * 0x0043a944. */
void carsys_stats_speed_percent(uint8_t* stats, int32_t percent);

#endif /* TM2_CARSYS_RECORDS_H */
