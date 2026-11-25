/*
 * en10_viewport.h: the view's projection set up, TM2.EXE 0x00493ed0 (level_setup_viewport), in the
 * En2Env of en2_env.h. x87 at control word 0x127f computed in double in the original's order
 * (-ffp-contract=off); the runtime's trig helpers (crt_ftan 0x004bf619, crt_fatan 0x004bf63a,
 * crt_fcos 0x004bf5c2, crt_fsin 0x004bf5cc) are a boundary: host libm tan / atan / cos / sin
 * (the user's decision for the x87 transcendental question). Build with -fno-builtin so the
 * compiler does not merge sin / cos into sincos.
 *
 * View block 0x00a03b70 + 0x94 * i: +0x00 i32 field of view (degrees), +0x04 i32 projection
 * distance, +0x08 / +0x0c i32 centre offsets, +0x10 / +0x14 screen size (0x004e83ec / f0),
 * +0x18 / +0x1c / +0x20 / +0x24 viewport left / width / top / bottom, +0x28..+0x34 float
 * viewport fractions, +0x38 / +0x40 i32 4096 / tan, +0x3c..+0x88 the clip plane normals,
 * +0x8c / +0x90 border x / y. Globals 0x00a04144 / 48 half angles, 0x00a0414c..58 their cos / sin,
 * 0x00a0415c..68 the same * 4096 chopped.
 * Verified against the original under emulation (tools/emu/check_en10_viewport.sh), trig
 * through host libm at the x87 helper boundary.
 */
#ifndef TM2_EN10_VIEWPORT_H
#define TM2_EN10_VIEWPORT_H

#include <stdint.h>
#include "en2_env.h"

/* 0x00493ed0 (eax = view, stack: float x0, y0, w, h fractions (bits), i32 fov, one unused word;
 * ret 0x18) */
/* Roadkill's display.widescreen, not the original: > 0 (set by src/host/qol/display.c) widens
 * the horizontal angle on screens wider than 4:3, keeping the original's vertical angle */
extern double en10_widescreen_aspect;

void en10_level_setup_viewport(En2Env* e, uint32_t view, uint32_t fx, uint32_t fy, uint32_t fw, uint32_t fh,
                               uint32_t fov, uint32_t unused);

#endif
