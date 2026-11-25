/*
 * gp7_car.h: the parts of two car functions of TM2.EXE that were left unported.
 *
 *   0x00437850 player_car_setup (PS1 carRestart): the whole function. Its camera fields are the
 *     verified car_camera_init (src/game/camera.c); the rest is here. EAX = car (player layout),
 *     EDX = car type (the word at +0x90), EBX = full weapon setup flag (low byte). Calls, with the
 *     registers the original has set for them: car_spawn_player 0x00437ac4(car); weapon setup
 *     0x0043807c(car, car + 0x848, 1) or (type 0xC0) 0x0044c81c(car + 0x848); in game mode 7
 *     (0x0045c338) the round reset once (0x004fb170) and the car's freeze object / registry step
 *     0x00470940(type, 0), 0x00470320(type + 6, 0, 0), else always the round reset
 *     (0x004fb170 = 0, 0x00448098, 0x00447b70, 0x00412d58, 0x00451458); 0x0044ef60(car + 0x7F0,
 *     car id); the hit state 0x00420be8(car, 2, 2, 0, stack 1); 0x00429fd0(car + 0xCC);
 *     0x0045b3f0(id) handed to the nullsub 0x0045b3f8(car, value); the model slot 0x0045ae24(id)
 *     gets +8 = 1, +4 = 0x00495864(0); returns 0x00436010()'s EAX.
 *
 *   0x00429fd0 car_clear_controls: the whole function.
 *
 *   0x0042a020 car_terrain_step, the part from 0x0042a4e7 to the end (after car_ground_contact,
 *     src/game/car.c): the surface effects of the tile under the car (surface kind byte
 *     0x004fdc76[tile], skipping kind 4 to the other probes'): event type 5 tiles 7 (the boss
 *     arena): flag ctl +0x23 with 0x00457138 / light 0x00495864(2 / 0) into the model slot +4;
 *     kind 2 (water?): wheel splashes 0x0045723c / 0x0042b02c, a counter stats +6 that takes 3
 *     health every 60 ticks; kinds 3 / 4 (lava / fire pits): the car's burn entity
 *     (0x00457138, 0x004573f8 into car +0xC8 / +0x94) follows the body and is dropped with the
 *     health; kind 4 sinks: ctl +0x24 / +0x1F, random side (0x004899c2), 0x00453c38(id), halves
 *     body +0x0A and +0x00 and lowers the probes; leaving the ground (ctl +0x0A) or a mixed
 *     contact ends it (0x00453c00(id)). The turbo object's +0x0C is cleared when the turbo flag
 *     (ctl +0x1A or stats +0x22 > 0) meets two probes on the ground.
 *
 * Both run on game addresses (En2Env mem) and reach every callee through e->call with the
 * original's address and its register arguments (compact: docs/RUNTIME.md "Calls").
 * Verified against the original under emulation (tools/emu/check_gp7_car.sh).
 */
#ifndef TM2_GP7_CAR_H
#define TM2_GP7_CAR_H

#include <stdint.h>

#include "../engine/en2_env.h"

/* 0x00429fd0 car_clear_controls(EAX block): clears the control bytes +0x00 +0x01 +0x03 +0x04 +0x05
 * +0x0D..+0x10 +0x12 +0x13 +0x17 +0x18 +0x1B..+0x1D +0x1F, sets +0x1A = 1 (in this order). The
 * same clear is a static in car_input.c / ai_reset.c; this is the exported one. */
void gp7_clear_controls(En2Env* e, uint32_t p);

/* 0x00437850 (EAX car, EDX type, EBX flag): returns the original's EAX */
uint32_t gp7_player_car_setup(En2Env* e, uint32_t car, uint32_t edx, uint32_t ebx);

/* 0x0042a020 from 0x0042a4e7 (edi = car, [esp + 0x34] = player flag) to its return */
void gp7_car_terrain_surface(En2Env* e, uint32_t car, int player);

#endif /* TM2_GP7_CAR_H */
