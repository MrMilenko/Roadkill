/*
 * sh4_target_actors.h: the target actors' frame update, TM2.EXE 0x00471b14 (PS1 decomp:
 * targetActorsUpdate). The actors are 0x4c byte records from 0x00622b58, count 0x00622b54:
 *   +0 i8 place row, +1 i8 place column (toggled), +2 i8 state (0 idle, 1 hit: thrown, 2 lying),
 *   +4 i16 target type, +6 i16 instance, +8 i16 timer, +0x0a i16 throw strength, +0x0c..+0x14 i32
 *   goal position, +0x18 12 words: the hitting car's matrix, +0x48 the model node (+0xb0 matrix,
 *   +0xe0 i16 rotation, +0xe8..+0xf0 i32 position, +8 u32 flag).
 * Per actor: animation step 0 when it is more than 200 above the ground (0x0042c2d4); walkers
 * (types 0x202 / 0x205, idle) step 7 forward through their matrix, face their goal (0x0048b71c)
 * and at the goal (within 0x50) take the next goal from TM2.EXE's table of the event type
 * (0x00474508: types 1, 3, 6, 8; 2 and 4 keep it; ORIGINAL BUG / quirk: any other event type
 * ends the whole update at once, skipping the rest of the actors). Idle actors are hit by any
 * human or AI car within 80 on every axis (sounds, score, the car's matrix kept, the throw
 * strength from the car's speed; a human's tenth hit gives armor and health); a thrown actor
 * moves along the kept matrix for 40 frames, then lies 300 frames and is placed again
 * (targetActorPlace 0x004713f0).
 *
 * Verified against the original under emulation (tools/emu/check_sh4_target_actors.sh).
 */
#ifndef TM2_SH4_TARGET_ACTORS_H
#define TM2_SH4_TARGET_ACTORS_H

#include "sh3_screen.h"

/* fb = the game address of the original's frame base (esp after its prologue, 0x28 bytes):
 * +0 the step vector, +0x0c the step result / car position, +0x18 human index, +0x1c actor
 * index, +0x20 actor pointer, +0x24 AI index. */
void sh4_target_actors_update(const ShEnv* e, uint32_t fb);

#endif /* TM2_SH4_TARGET_ACTORS_H */
