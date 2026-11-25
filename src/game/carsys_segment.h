/*
 * carsys_segment.h: an AI car's speed and aim for its route segment, ported from TM2.EXE
 * 0x00433000 (PS1 aiSetSegmentSpeed), with the difficulty speed percentage 0x0045b488.
 *
 * AI car record fields (i16 unless noted): +0x30 car id, +0x441..0x443 u8 flags, +0x446 u8,
 * +0x458 current node, +0x45A next node, +0x46E lane, +0x470, +0x478 speed target, +0x47A its
 * copy, +0x480 turn, +0x494 / +0x498 i32 node delta, +0x4AC / +0x4B0 i32 aim point, +0x516 base
 * speed, +0x540, +0x544 rate, +0x562, +0x5AC i32. PTS nodes: 24 bytes, +0 / +4 i32 x, y.
 * Verified against the original under emulation (tools/emu/check_carsys_segment.sh).
 */
#ifndef TM2_CARSYS_SEGMENT_H
#define TM2_CARSYS_SEGMENT_H

#include <stdint.h>

#include "carsys_records.h"

typedef struct {
    const CarRecords* rec;              /* the difficulty */
    const uint8_t* nodes;               /* 0x004fe1e4 */
    /* 0x00439588(car id): the car's default top speed */
    int32_t (*top_speed)(void* user, int32_t car_id);
    void* user;
} CarsysSegmentEnv;

/* 0x0045b488: 75 / 120 / 140 for difficulty 0 / 1 / 2, and 75 for anything else (where
 * carsys_ai_set_difficulty uses 120). */
int32_t carsys_ai_speed_percent(const CarRecords* r);

/* 0x00433000 (ai, active): picks the speed target (+0x478) and rate (+0x544) from the lane,
 * turn and flags (only when active, plus the 160 floor and the difficulty scaling), keeps the
 * rate at least 2000 / +0x562, sets +0x5AC, the node delta, which axes it is aligned to and the
 * aim point. +0x562 = 0 divides by zero in the original. */
void carsys_ai_segment_speed(uint8_t* ai, uint8_t active, const CarsysSegmentEnv* env);

#endif /* TM2_CARSYS_SEGMENT_H */
