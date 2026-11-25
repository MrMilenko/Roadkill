/*
 * en9_polyset.h: the per primitive vertex setup before clipping, TM2.EXE 0x00491698, in the
 * En2Env of en2_env.h (the 0x50 byte frame at En2Env.scratch: +0 the two line end points i32[3]
 * each, passed to 0x0048c6b8). x87 code at control word 0x127f: computed in double in the
 * original's order (build with -ffp-contract=off).
 *
 * Primitive record (edx): +0 u8 bits 0..6 = uv index, +1 u8 shade mode (0x009f7d34),
 * +4 i32[3] vertices (12 bytes each), +0x10 / +0x14 / +0x1c sizes for the 1 / 0 vertex kinds,
 * +0x34 bits 16..18 vertex count n, bits 28..30 colour words to skip, +0x38 flags (bit 0
 * line kind, bit 6 forced depth = bits 7..12), +0x3c u32 colours, then u16 uv pairs.
 * Vertex slots 0x009f78b0 + 0x34 * k (pointer table 0x009f7de0): +0 / +4 / +8 float view
 * position, +0xc / +0x10 float u, v, +0x1c..+0x24 float r, g, b, +0x28 0.1f, +0x2c 0x004e82b4.
 * Verified against the original under emulation (tools/emu/check_en9_polyset.sh).
 */
#ifndef TM2_EN9_POLYSET_H
#define TM2_EN9_POLYSET_H

#include <stdint.h>
#include "en2_env.h"

/* 0x00491698 (eax = the scene object, edx = the primitive): globals of the clipper set up, the
 * vertices transformed (0x0048c6b8) and scaled by 1 / (1 << byte 0x004e82c4), n = 0 a sprite
 * (0x00490780), n = 1 a line (0x004908b0; flag bit 0: the two points given, else a vertical
 * segment of +-[+0x14]), n = 2 / 3.. polygons with the depth range (min / max z) and colours /
 * uvs; depth 0x009f7d58 = chop(z); with a positive depth: the render mode, the five frustum
 * planes (0x0048f59c) while vertices remain, every vertex projected (0x0048c890) and the result
 * submitted (0x0048fd54 or, with the hardware device, 0x00491018). Object +0x8008 is kept.
 * Not covered: NaN / infinity inputs where the x87 and IEEE disagree (sign of a generated NaN). */
void en9_poly_setup(En2Env* e, uint32_t obj, uint32_t prim);

#endif
