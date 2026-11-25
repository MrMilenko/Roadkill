/*
 * exe_carstats.h: the per car stat tables, read from the player's own TM2.EXE.
 *
 * The car stats setup at 0x0043846c is a compare tree on the car id (cmp bx, imm / jb / jbe /
 * je ...) whose branches write constants into the stats block. The extractor walks that code for
 * every car id 0..0x1FF with a small x86 decoder and keeps the constant writes only one id reaches:
 * those are the car's own table. Code it does not recognize (another TM2.EXE build) is an error,
 * never a guess.
 */
#ifndef TM2_EXE_CARSTATS_H
#define TM2_EXE_CARSTATS_H

#include "pe.h"
#include "../game/car_stats.h"

/* Fills *out (malloc'd entries with malloc'd byte pairs) and *count. 0 on success; on failure
 * err gets a reason. */
int car_stats_extract(const PeImage* pe, CarStatsEntry** out, int* count, char* err, size_t errlen);

/* Reads the file, extracts, installs the table with car_stats_set_table. 0 on success. */
int tm2_car_stats_load(const char* exe_path);

#endif /* TM2_EXE_CARSTATS_H */
