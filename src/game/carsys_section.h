/*
 * carsys_section.h: switching a world section off, ported from TM2.EXE 0x0042f2dc (PS1
 * worldSectionDisable). World tables (pointers into the level's data, as the original's globals):
 *
 *   0x004fca2c  12 bytes per section: +0 i32 floor height (32000 = gone)
 *   0x004fcbc8  i32 per section, 0x004fcc54 i32 per section (cleared)
 *   0x004fe162 / 0x004fe185  u8 first / last surface per section
 *   0x004fb17c  surfaces, 0x1C bytes: +2 i16 section, +4 / +6 i16, +0x0C..0x12 i16 x 4,
 *               +0x14 / +0x18 i32
 *   0x004fe1e4  PTS nodes, 24 bytes (count 0x004fe1b0): +0x0A i16 neighbors, +0x16 i8 neighbor
 *               count, +0x17 i8 section
 *   0x004fdc0d  u8 per section
 * Verified against the original under emulation (tools/emu/check_carsys_section.sh).
 */
#ifndef TM2_CARSYS_SECTION_H
#define TM2_CARSYS_SECTION_H

#include <stdint.h>

#include "carsys_records.h"

typedef struct {
    uint8_t* heights;           /* 0x004fca2c */
    int32_t* sec_a;             /* 0x004fcbc8 */
    int32_t* sec_b;             /* 0x004fcc54 */
    const uint8_t* first;       /* 0x004fe162 */
    const uint8_t* last;        /* 0x004fe185 */
    uint8_t* surfaces;          /* 0x004fb17c */
    uint8_t* nodes;             /* 0x004fe1e4 */
    const int32_t* node_count;  /* 0x004fe1b0 */
    uint8_t* section_flags;     /* 0x004fdc0d */
    const CarRecords* rec;
    void (*node_removed)(void* user, int32_t node);         /* 0x0043bd74(node) */
    void (*spin_out)(void* user, uint8_t* car, int human, int kind);    /* 0x00421920(car, 1, 5, 0) */
    void* user;
} CarsysSectionEnv;

/* 0x0042f2dc: section's floor goes to 32000, its surfaces get 32000 heights, every PTS node in it
 * is unlinked from all nodes' neighbor lists (then 0x0043bd74(node)), and every human car standing
 * in it (i16 +0xC6) starts a kind 5 spin out. Kept from the original: after removing a neighbor
 * the next one is not checked (it moved into the removed one's place), and the shift reads one
 * entry past the count. */
void carsys_world_section_disable(const CarsysSectionEnv* env, int32_t section);

#endif /* TM2_CARSYS_SECTION_H */
