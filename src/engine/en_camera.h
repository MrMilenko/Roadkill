/*
 * en_camera.h: view matrix loads of the polygon path, ported from TM2.EXE
 *   0x0048b9b4 en_view_load_rot  (camera_load_orient): float rotation 0x009f77d0 = i32 matrix / 4096
 *   0x0048ba84 en_view_load_pos  (camera_load_position): float translation 0x009f77f4 = (float)i32
 *   0x0048ba48 en_set_trans      (SetTransMatrix, PsyQ name): i32 translation 0x009f77c4 = mat +0x24..+0x2c
 * The float transform is the one en_xform_short / en_xform_int (src/engine/en_clip.c) apply.
 * Verified under emulation: tools/emu/check_engine_camera.sh.
 */
#ifndef TM2_EN_CAMERA_H
#define TM2_EN_CAMERA_H

#include <stdint.h>

#include "en_clip.h"

/* m: 9 i32 (.12 fixed, row major) -> x->m[i] = (float)(m[i] * (1.0 / 4096)) */
void en_view_load_rot(EnXform* x, const int32_t m[9]);
/* p: i32 x, y, z -> x->t */
void en_view_load_pos(EnXform* x, const int32_t p[3]);
/* mat: a 12 int PsyQ style MATRIX (9 rotation + translation at +0x24); out = 0x009f77c4 */
void en_set_trans(int32_t out[3], const int32_t mat[12]);

#endif /* TM2_EN_CAMERA_H */
