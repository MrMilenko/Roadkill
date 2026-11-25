/*
 * en3_small.h: small engine functions of TM2.EXE 0x00488000..0x00490000 in the En2Env of
 * en2_env.h (memory by game address, every call by the original's address; locals the original
 * passes by address live at En2Env.scratch at the original's stack offsets).
 * Verified against the original under emulation (tools/emu/check_en3_small.sh).
 */
#ifndef TM2_EN3_SMALL_H
#define TM2_EN3_SMALL_H

#include <stdint.h>
#include "en2_env.h"

/* 0x0048e240: 0x009f7880 = v, byte 0x009f7890 = 0. Returns v with bits 8..15 cleared (the
 * original's xor ah, ah used to make the zero byte). */
uint32_t en3_collide_set_9f7880(En2Env* e, uint32_t v);
/* 0x0048c50c math_create_vector (eax = int16 angle, edx = out): math_sincos 0x0048b8a0 into two
 * locals (c, s), then out i32[9] = {0x1000, 0, 0, 0, c, s, 0, -s, c} (a rotation about x).
 * Returns out. */
uint32_t en3_math_create_vector(En2Env* e, uint32_t angle, uint32_t out);
/* 0x0048df8c dpc_collide_entry (PS1 hdpQueryPoint; ebx = point i32[3], ecx = out): the query
 * state at 0x009f7820 reset (+0 state, +0x10, +0x68 = 0, +8 / +0xa i16 = -1), *out = 0; with the
 * scene root's (0x00492c44) collision data (+0x15c): the grid reject (0x004bca88, point >> byte
 * 0x004e8288) gives state 2, else dpc_collide (0x0048d0b8) and on state 1 *out = root +0x158.
 * Returns 0x009f7820. */
uint32_t en3_dpc_collide_entry(En2Env* e, uint32_t point, uint32_t out);
/* 0x0048e25c hdpTestSphereBands (eax = cell, edx = point i32[3], ebx = hit record, ecx = shift):
 * the cell's two planes (0x004bcb70, i16 {nx, ny, nz, d} each, the second at local +8); distance
 * = (n . p >> 12) + d of the second plane, or, when cell->+4 byte +0x12 bit 0, of both planes
 * (the first only when the second's is >= 0). A distance in (-(50 << shift), 0) is a hit:
 * hit +0 = 1, +0x10 = distance. Returns the distance (0 when there is no triangle). */
int32_t en3_sphere_bands(En2Env* e, uint32_t cell, uint32_t point, uint32_t hit, uint32_t shift);
/* 0x00488fd8 (audio dialog, eax = cfg, edx = widget): the focus moves from a slider label to its
 * control: 0x48c -> combo 0x9c, 0x658 -> 0x118, 0x824 -> 0x16c, 0x9f0 -> 0x1c0, 0xbbc -> 0x214 and
 * 0xd88 -> 0x268 when the mixer answered (+0x1120) else 0x16c; any other widget: nothing. */
void en3_audio_focus_label(En2Env* e, uint32_t cfg, uint32_t w);

#endif
