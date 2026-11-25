/*
 * gp_landmark.h: the arena landmark records (two record tables of 5, the Moscow dish style
 * "burning object" record, debris points, a mounted object), ported from TM2.EXE.
 *
 *   0x00410700 gp_landmarks_reset     clears both record tables, the burning record, the point lists
 *   0x004107f4 gp_landmark_a_add      table A record r: active, object by part, part position
 *   0x00410878 gp_landmark_a_addr     game address of table A record r
 *   0x00410890 gp_landmark_a_blast    a blast at pos breaks table A records 1..4: those in the zone of
 *                                     pos (0x0042cba8) and those with a part 1..4 position within
 *                                     max(|dx|, |dy|, |dz|) >> 3 < 100 (abs and >> as the original:
 *                                     wrapping differences)
 *   0x00410b34 gp_landmark_a_break    record r, once (+0x01 and +0x04 clear, +0x00 set): falling parts
 *                                     with random spins and heights, a fall direction per record 1..4,
 *                                     then its zone's floor goes (0x0042f2dc(+0x0C))
 *   0x00411334 gp_landmark_b_blast    the same for table B records 1..2, part positions 1..2
 *   0x004114c0 gp_landmark_b_break    the same for table B (2 parts, heights + 20, no +0x16; record 1
 *                                     falls left or right by a rand() bit)
 *   0x00410df4 gp_landmark_a_update   the same for table A records 1..4 (old name event_weapon_pickup
 *                                     is wrong): three falling parts, then a fourth (top, +0x68) that
 *                                     is placed every frame once the frame passes +0x12 / +0x14 / +0x16,
 *                                     the triple blast with sound 0xFA0, the end when all four are done
 *   0x00411fcc gp_mount_update        one frame of the mounted object (0x004f08dc record; PS1 pair
 *                                     eiffelTowerUpdate): after its start frame it hops (+0x2C / +0x30
 *                                     of the object for 16 frames), tilts (x angle += spin, spin + 1 every
 *                                     5 frames; z eases to 0x5B) until x passes 0x444: then the topple
 *                                     (8 jittered effects at points of the object's matrix, 6 fixed
 *                                     effect points, flash, sounds); smoke at random frames until then;
 *                                     at the start frame three parts are thrown (0x0048a738)
 *   0x004116b0 gp_landmark_b_update   one frame of the broken table B records 1..2 (old name
 *                                     event_vehicle_destroyed is wrong): per part smoke / explosions,
 *                                     the part placed (0x0048a738) at its fall frame, a crash sound and
 *                                     explosion during its 10 frames after, then the double blast with
 *                                     flash and sound 0xFA5 (record number) once both parts landed,
 *                                     and the end 20 frames later (same scheme as gp_burn_update)
 *   0x00411270 gp_landmark_b_mark     every active table B record gets state 6
 *   0x004112a4 gp_landmark_b_add      table B record r: active, object by part, offset (0, -42, 240)
 *   0x004119e4 gp_burn_set            the burning record's object and position, countdown 1
 *   0x00411b30 gp_burn_countdown      countdown minus n; at <= 0 starts the burning (0x00411a0c, the
 *                                     tail jumps into it): random sizes and spins from rand()
 *   0x00411b48 gp_burn_update         one frame of the burning object: smoke and explosions at random
 *                                     frames (rand() % +0x03, +0x03 counts down to 5), the big blast
 *                                     at frame +0x08 (object placed, flash, sound, explosions at the
 *                                     four burn points), the fall (+0x18 += +0x2C) until then, smoke
 *                                     for 10 frames after +0x08 + +0x0A, then the final blast once
 *                                     and the end after +0x02 more frames
 *   0x00411dc8 gp_burn_point_set      point i of the 16 byte list 0x004f0830
 *   0x00411de8 gp_debris_point_set    point i of the 20 byte list 0x004f0884
 *   0x00412564 gp_mount_set           mounted object 0x004f08ec, offset (0, -0x1800, 0xF0), angles 0
 *   0x004125a8 gp_mount_part_set      u32 0x004f08f0[i]
 *   0x00412638 gp_trail_frames_set    the missile trail frame table pointer 0x004f11d8
 *
 * Game memory 0x004f0000..0x004f0fff is one block (env->d); records hold game addresses of objects
 * (part index = u16 object +0x10).
 *   Table A 0x004f004c, table B 0x004f0420: 5 records of 0xC4: +0x00 u8 active, +0x01 u8, +0x04..+0x09
 *     u8 (cleared), +0x0C i16 state (-1 reset, 6 marked; table A: its zone), +0x24 u32 object[part],
 *     +0x38 i32[3] per part (stride 0x0C).
 *   Break (table A): +0x01 = 1, +0x02 = 0, +0x03 = 30, +0x04 = 0, +0x0A i16 = 0, +0x16 i16 (rand >> 9) + 60;
 *     per part k = 0..3: +0x06 + k u8 0, +0x10 + 2k i16 (rand >> 9) + 40, +0x1A + 2k i16 20 when a first
 *     (rand >> 9) <= 20 else a second rand >> 9, +0xA4 + 8k / +0xA6 + 8k i16 rand / 8, +0xA8 + 8k i16
 *     0xFFF; record 1..4: the fall vectors (i16 triples at +0x7C, +0x84, +0x8C, +0x94; values in
 *     gp_landmark.c).
 *   Burning record 0x004f07f4: +0x00 u8 active, +0x01 i8 countdown, +0x02 u8, +0x03 u8 (30), +0x04 u8,
 *     +0x05 u8, +0x06 i16 frame, +0x08 i16 (rand >> 9) + 100, +0x0A i16 (rand >> 9) + 20,
 *     +0x0C u32 object (0 = none: the start does nothing), +0x10 i32[3] position,
 *     +0x1C / +0x1E i16 spins +-rand / 0x1999, +0x20 i16 0, +0x24 i32[3] velocity (0, 0, -6),
 *     +0x30 / +0x32 i16 rand / 8, +0x34 i16 0xFFF.
 *   Mount record 0x004f08dc: +0x04 u8 active, +0x05 i8 smoke countdown (>= 2), +0x06 u8 toppled,
 *     +0x08 i16 spin, +0x0A i16 hop speed, +0x0C i16 frame, +0x0E i16 start frame, +0x10 u32 object
 *     (+0x04 i32[9] matrix, +0x2C / +0x30 i32 position y / z), +0x14 u32[3] parts, +0x20 i32[3]
 *     offset, +0x2C i16[3] angles. Effect point 0x004f003c, last jittered point 0x004f0048, the
 *     parts' spins 0x004f0010 (3 x 12 bytes) and velocity 0x004f0034; fixed points and angles in the
 *     executable's data (0x004e6644.., 0x004e66b0, 0x004e66d4, 0x004e66bc) are passed by address.
 *   Point lists: 0x004f082c (5 of 0x10; +4 i32[3] set by 0x00411dc8), 0x004f087c (5 of 0x14;
 *     +8 i32[3] set by 0x00411de8); the reset clears the first u32 of each 0x10 entry and the first two of
 *     each 0x14 entry.
 * The second argument of 0x004112a4 (EDX) is unused.
 *
 * Verified against the original under emulation (tools/emu/check_gameplay_landmark.sh).
 */
#ifndef TM2_GP_LANDMARK_H
#define TM2_GP_LANDMARK_H

#include <stdint.h>

#include "weapons/wmem.h"

#define GP_LM_BASE 0x004f0000u
#define GP_LM_SIZE 0x1000u

typedef struct {
    uint8_t* d;                 /* game memory 0x004f0000..0x004f0fff */
    const WMem* mem;            /* objects (part index at +0x10) */
    uint32_t* trail_frames;     /* 0x004f11d8 */
    const uint8_t* grp;         /* 0x004fca18 (zone lookup, src/physics/grp.c) */
    void* user;
    void (*section_disable)(void* user, int32_t zone);   /* 0x0042f2dc */
    /* the burning update's calls (game addresses for points and the object):
     * 0x0044a40c(pos, a, b, c) jittered point (returns its game address), the effects 0x004508f4 /
     * 0x00450984 / 0x00450a10 (point, 0), the sound 0x00470320(id, a, b, c), the screen flash
     * 0x00415bc4(frames, r, g, b), the object placement 0x0048a738(obj, pos, +0x30, +0x1C, then
     * on the stack +0x24, +0x0A, 0x14, 0xC8, 0) */
    uint32_t (*jitter)(void* user, uint32_t pos, int32_t a, int32_t b, int32_t c);
    void (*fx)(void* user, uint32_t fn, uint32_t pos, int32_t arg);
    void (*sound)(void* user, int32_t id, int32_t a, int32_t b, int32_t c);
    void (*flash)(void* user, int32_t frames, int32_t r, int32_t g, int32_t b);
    void (*place)(void* user, uint32_t obj, uint32_t pos, uint32_t ang, uint32_t spin, uint32_t vel, int32_t s1,
                  int32_t s2, int32_t s3, int32_t s4);
    /* table B update only: split mode 0x0046cc54, sound placement 0x0046f974(pos) / 0x0046f980(pos, -1)
     * in split screen, volume 0x0046f990, pan 0x0046f988, play 0x0046f8b0(id, pan, volume, prio) */
    int32_t (*split)(void* user);
    void (*locate)(void* user, uint32_t pos);
    void (*locate_split)(void* user, uint32_t pos, int32_t view);
    int32_t (*volume)(void* user);
    uint32_t (*pan)(void* user);
    void (*play)(void* user, int32_t id, uint32_t pan, int32_t vol, int32_t prio);
    /* mount update only: 0x004325a8 and 0x004325c8 (no arguments) at the topple */
    void (*topple_a)(void* user);
    void (*topple_b)(void* user);
} GpLmEnv;

void gp_landmarks_reset(GpLmEnv* env);
void gp_landmark_a_add(GpLmEnv* env, uint32_t obj, const int32_t pos[3], int32_t r);
uint32_t gp_landmark_a_addr(int32_t r);
void gp_landmark_a_blast(GpLmEnv* env, const int32_t pos[3]);
void gp_landmark_a_break(GpLmEnv* env, int32_t r);
void gp_landmark_b_blast(GpLmEnv* env, const int32_t pos[3]);
void gp_landmark_b_break(GpLmEnv* env, int32_t r);
void gp_landmark_b_update(GpLmEnv* env);
void gp_landmark_a_update(GpLmEnv* env);
void gp_mount_update(GpLmEnv* env);
void gp_landmark_b_mark(GpLmEnv* env);
void gp_landmark_b_add(GpLmEnv* env, uint32_t obj, int32_t unused, int32_t r);
void gp_burn_set(GpLmEnv* env, uint32_t obj, const int32_t pos[3]);
void gp_burn_countdown(GpLmEnv* env, int8_t n);
void gp_burn_update(GpLmEnv* env);
void gp_burn_point_set(GpLmEnv* env, const int32_t pos[3], int32_t i);
void gp_debris_point_set(GpLmEnv* env, const int32_t pos[3], int32_t i);
void gp_mount_set(GpLmEnv* env, uint32_t obj);
void gp_mount_part_set(GpLmEnv* env, uint32_t v, int32_t i);
void gp_trail_frames_set(GpLmEnv* env, uint32_t v);

#endif /* TM2_GP_LANDMARK_H */
