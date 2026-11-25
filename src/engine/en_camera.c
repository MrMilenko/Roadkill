/*
 * en_camera.c: view matrix loads, ported from TM2.EXE 0x0048b9b4, 0x0048ba84, 0x0048ba48
 * (see en_camera.h). fild + fmul by the double 1/4096 (0x004e2d54) is exact in double; the
 * float store rounds.
 */
#include "en_camera.h"

void en_view_load_rot(EnXform* x, const int32_t m[9]) {
    for (int i = 0; i < 9; i++) x->m[i] = (float)((double)m[i] * 0.000244140625);
}

void en_view_load_pos(EnXform* x, const int32_t p[3]) {
    for (int i = 0; i < 3; i++) x->t[i] = (float)p[i];
}

void en_set_trans(int32_t out[3], const int32_t mat[12]) {
    out[0] = mat[9];
    out[1] = mat[10];
    out[2] = mat[11];
}
