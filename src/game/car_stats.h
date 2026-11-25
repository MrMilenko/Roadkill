/*
 * car_stats.h: car stats setup, ported from TM2.EXE 0x0043846c (old project: vehicle_init_car_physics).
 *
 * Fills a car's stats block (player car +0x138, AI car +0x4EC) for a car id (0x10, 0x20, .. 0xF0;
 * other ids get a generic set, 0x00439fa8), then derives the per frame values (car_stats_derive).
 * The per car bytes come from the player's own TM2.EXE (src/assets/exe_carstats.c, which walks
 * 0x0043846c) and are installed with car_stats_set_table. Verified against the original under
 * emulation (tools/emu/check_car_stats.sh) for every id 0..0x1FF.
 */
#ifndef TM2_CAR_STATS_H
#define TM2_CAR_STATS_H

#include <stdint.h>

typedef struct {
    uint16_t id;
    const uint8_t* bytes;   /* (offset, value) pairs */
    int count;
} CarStatsEntry;

/* the per car tables (not copied; must stay alive). Without a table every id gets the generic set. */
void car_stats_set_table(const CarStatsEntry* entries, int count);

/* Car 0xC0 also turns two scene objects (0x00453f10 / 0x00453ed0): which = 0 / 1, angle. */
typedef void (*CarStatsRotateFn)(void* user, int which, int32_t angle);

/* slot: the original's first argument (>0 with multiplayer clears stats +0xA4 on a player car);
 * multiplayer: gameloop_check_multiplayer(). */
/* 0x00439fa8 (exported): the generic stats for car ids without a table of their own. */
void car_stats_generic(uint8_t* stats);

void car_stats_init(uint8_t* stats, int16_t slot, uint16_t car_id, int multiplayer, CarStatsRotateFn rotate, void* user);

#endif /* TM2_CAR_STATS_H */
