/*
 * spawn.h: spawn node selection, ported from TM2.EXE 0x0043289c.
 * The level keeps up to 15 spawn waypoint indices (int16 at 0x004fe1b4) and a used flag per entry
 * (byte at 0x004fe1d2). Verified under emulation (tools/emu/check_spawn.sh).
 */
#ifndef TM2_SPAWN_H
#define TM2_SPAWN_H

#include <stdint.h>

#define SPAWN_MAX 15

typedef struct {
    int16_t node[SPAWN_MAX];   /* waypoint (PTS node) index per spawn slot */
    uint8_t used[SPAWN_MAX];
} SpawnList;

/* Starts at a random slot, takes the first unused one (wrapping) and marks it used. If none is
 * free (or the taken node index is negative) returns a random slot's node. Uses tm2_rand(). */
int32_t spawn_pick(SpawnList* s, int32_t count);

#endif /* TM2_SPAWN_H */
