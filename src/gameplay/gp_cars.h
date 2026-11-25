/*
 * gp_cars.h: per human car round helpers, ported from TM2.EXE.
 *
 *   0x004327fc gp_cars_pick_spawns   resets the spawn list (0x0043bd60), then gives every human car
 *                                    (0x0045afb4 count, 0x0045afbc block) a spawn node (0x0043289c with
 *                                    the spawn count, signed byte 0x004fe1e1) in car +0x208 (i32)
 *   0x00432838 gp_cars_spawn_events  for each car (count 0x004632c8): 0x00464e84(car +0x894), then the
 *                                    event 0x00473650(1, u16 car +0x208, EBX), and 0x00473568(0, 0)
 *                                    when flag is set
 * ORIGINAL BUG: 0x00432838 passes its caller's ESI as the third event argument (EBX = ESI, never set
 *   in the function); 0x00473650 stores it in the event record. The port takes it as `esi`.
 *
 * Verified against the original under emulation (tools/emu/check_gameplay_cars.sh).
 */
#ifndef TM2_GP_CARS_H
#define TM2_GP_CARS_H

#include <stdint.h>

typedef struct {
    const int8_t* spawn_count;   /* 0x004fe1e1 */
    void* user;
    void (*spawn_reset)(void* user);                       /* 0x0043bd60 */
    int32_t (*car_count)(void* user);                      /* 0x0045afb4 */
    int32_t (*player_count)(void* user);                   /* 0x004632c8 */
    uint8_t* (*car)(void* user, int32_t i);                /* 0x0045afbc */
    int32_t (*spawn_pick)(void* user, int32_t count);      /* 0x0043289c */
    void (*car_reset)(void* user, uint32_t v);             /* 0x00464e84 */
    void (*event)(void* user, int32_t kind, int32_t a, uint32_t b);   /* 0x00473650 */
    void (*event_flush)(void* user, int32_t a, int32_t b); /* 0x00473568 */
} GpCarsEnv;

void gp_cars_pick_spawns(GpCarsEnv* env);
void gp_cars_spawn_events(GpCarsEnv* env, uint8_t flag, uint32_t esi);

#endif /* TM2_GP_CARS_H */
