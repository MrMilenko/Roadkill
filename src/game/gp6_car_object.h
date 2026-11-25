/*
 * gp6_car_object.h: the car / object branch of TM2.EXE 0x00419604 (car_hits_resolve,
 * src/game/car_hit.h), the code range 0x00419d43..0x0041b859, ported on its own (car_hit.c
 * delegates it through CarHitEnv.hit_entity). It runs for a hit record whose entity is neither
 * a wall (no entity, entity type 6000 and up) nor type 9, and ends at 0x0041ba03 (the record is
 * dropped when the frame byte +0x1B8 is set).
 *
 * The range keeps all its state in the frame of 0x00419604 (fb = that function's esp after its
 * prologue, 0x1C0 bytes of locals), so the port does too: every local is read and written at
 * fb + the original's offset. The surrounding code leaves these set when it enters the range:
 *   +0x6C i32 record index           +0x70 u32 the car block          +0x1B4 u8 its kind (1 player)
 *   +0x78 i32 AI car +0x28 (player 0) +0x84 i32 the record's bounce angle (summary +0x0C)
 *   +0x8C controls  +0x90 stats  +0x94 turbo block   +0x98 the hit record + 4   +0x9C its summary
 *   +0x170 u16 car id (car +0x90 / +0x30)   +0x1B8 u8 = 0 (drop)
 * and ebp = the car's body (+0x6C4 / +0x708).
 *
 * What it does: the record's entity is looked up as a car (0x0045b0ec):
 *   - another car: the two cars' hit summaries get a car to car contact each (the first free one
 *     of the six at hit base +0x274, else +0x328): mass shares from stats +0x7A, the push
 *     direction and strength from both cars' wheel probe positions, impact speeds and angles
 *     against each car's travel angle, bounce sizes capped by each car's stats +0xB4, a spin hint
 *     when the bounce turns a car past 0x200; crash rumble (0x0046f974 / 0x0046f8b0 sounds 5, 3)
 *     and, for the special cars, the 0x20 car's boost hit (sound 0x25), the 0xC0 car's claw
 *     (0x0044c81c) and the damage through 0x004210e4. Cars still touching (both indices match,
 *     slow) are kept apart instead (+0x19C).
 *   - a platform object (entity type 0x64 / 0x65, the car not too high, +0x78 <= 0xFA0): the car
 *     bounces off it, spins (0x00421920(car, kind, 0x0A, 1)) or is pushed sideways by the angle to
 *     the platform's target, takes damage (player cars: 0x0045b154 / 0x004210e4) and the platform
 *     reports the hit (0x00470d3c(0x1FA / 0x1FB, 4 / 5, kind)) every few frames (0x004931f4).
 *   - anything else: the record is dropped.
 * Random numbers and frame ticks come from calls (0x004899c2, 0x004931d4) like every other callee;
 * the fixed point math (atan2, angle wraps, matrix, sine) runs the src/math ports.
 * Verified against the original under emulation (tools/emu/check_gp6_car_object.sh).
 */
#ifndef TM2_GP6_CAR_OBJECT_H
#define TM2_GP6_CAR_OBJECT_H

#include <stdint.h>

#include "../engine/en2_env.h"

#define GP6_FRAME_SIZE 0x1c0u

/* the range 0x00419d43..0x0041ba03 with the frame at fb and ebp = body. Every callee goes
 * through e->call with the original's address (Watcom registers, then the stack words). A zero
 * divisor where the original divides (a stats or probe value of 0) faults in the original; the
 * port leaves the quotient 0 there. */
void gp6_car_object_hit(En2Env* e, uint32_t fb, uint32_t ebp);

#endif /* TM2_GP6_CAR_OBJECT_H */
