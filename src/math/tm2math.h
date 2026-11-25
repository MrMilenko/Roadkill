/*
 * tm2math.h: TM2 fixed point math, ported from TM2.EXE.
 *
 * Every function here is checked against the original machine code by
 * tools/emu/check_math.sh (Unicorn emulation of TM2.EXE vs this code, random + edge inputs).
 *
 * Units
 *   angles: 12 bit, 0x1000 = full turn. Signed range after wrapping: [-0x7FF, +0x7FE].
 *   fixed point: 12 fractional bits, 0x1000 = 1.0.
 *   tm2_fx12(): the original's ">> 12" truncates toward zero (not floor, not round).
 *
 * Integer overflow wraps like the original 32 bit x86 code (done with uint32_t math).
 */
#ifndef TM2_MATH_H
#define TM2_MATH_H

#include <stdint.h>

#define TM2_ANGLE_FULL    0x1000
#define TM2_ANGLE_HALF    0x800
#define TM2_ANGLE_QUARTER 0x400

/* Tables come from the player's own game files, never shipped with the port.
 *   sintab_path: SINTAB.DAT (1024 int32, loaded raw into g_sintab 0x00a04678 by 0x00495924)
 *   exe_path:    TM2.EXE (513 int16 atan table at VA 0x004e8a0c, initialized data)
 * Returns 0 on success, negative on error (message on stderr). */
int tm2_math_load_tables(const char* sintab_path, const char* exe_path);

/* Two's complement wrapping helpers matching 32 bit x86 arithmetic. */
static inline int32_t tm2_wmul(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
static inline int32_t tm2_wadd(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }

/* x >> 12 truncated toward zero, the original's
 * "(v + (v >> 31) * -0x1000 - ((v >> 31) << 11 < 0)) >> 12" pattern. */
static inline int32_t tm2_fx12(int32_t v) {
    return (int32_t)(v < 0 ? -(int32_t)((-(int64_t)v) >> 12) : v >> 12);
}

/* math_sin 0x0048b624 / math_cos 0x0048b690.
 * ORIGINAL BUG (not reproduced): for angle = -0x1000 * k the original computes index 0x1000 and
 * reads outside the table (sin: the int32 at 0x00a04674, cos: the int32 at 0x00a05678, which holds
 * the VGA8X8.FNT pointer stored by subsys_sintab_font_init 0x00495924, so cos returns an address).
 * The port returns the correct 0 / 0x1000 there. */
int32_t tm2_sin(int32_t angle);
int32_t tm2_cos(int32_t angle);

/* math_sincos 0x0048b8a0 (EAX=angle as int16, EDX=out_cos, EBX=out_sin). Same original bug. */
void tm2_sincos(int16_t angle, int32_t* out_cos, int32_t* out_sin);

/* math_atan2 0x0048b71c (EAX=x, EDX=y). Returns an angle in 12 bit units. */
int32_t tm2_atan2(int32_t x, int32_t y);
/* DOMAIN: matches 0x0048b71c only for |x|, |y| below 2^22 and neither INT_MIN; above that the
 * original's x << 9 overflows and it reads outside its table (found by the gameplay and shell
 * diffs, 2026-11-21). Callers in verified ports stay inside the range. */

/* math_build_matrix 0x0048c360 (EAX=angles, EDX=out). angles = three int16 at +0, +2, +4 exactly
 * as the original reads them (entities keep them at +0xE0/+0xE2/+0xE4, matrix at +0xB0).
 * out = 3x3 int32, row major as stored by the original, equal to Ry(a[1]) * Rx(a[0]) * Rz(a[2]).
 *   a[2] (+0xE4): heading, rotation about world Z, which is up (vehicle_physics_tick 0x00414f98
 *                 lowers +0xF0 with the height above ground; spawning zeroes a[0], a[1], sets a[2])
 *   a[0] (+0xE0): pitch, rotation about model X (side axis)
 *   a[1] (+0xE2): roll, rotation about model Y (forward axis)
 * Model axes: +Y forward, +X side, +Z up (by reading: vehicle_physics_tick moves along M^T * (0, v, 0)
 * via mat_vec_mul_row, 0x0041354c offsets (0, -200, 0) behind a vehicle, 0x00425674 uses (x, 0, 0)
 * sideways). World direction of travel = (cos a0 sin a2, cos a0 cos a2, -sin a0). */
void tm2_build_matrix(const int16_t angles[3], int32_t out[9]);

/* math_mat_vec_mul_col 0x0048cb80: out[i] = fx12(m[3i]*v0 + m[3i+1]*v1 + m[3i+2]*v2)  (M * v)
 * math_mat_vec_mul_row 0x0048cc14: out[i] = fx12(m[i]*v0 + m[i+3]*v1 + m[i+6]*v2)    (M^T * v)
 * Old Ghidra names kept; "col"/"row" describe the original names, not a verified convention. */
void tm2_mat_vec_mul_col(const int32_t m[9], const int32_t v[3], int32_t out[3]);
void tm2_mat_vec_mul_row(const int32_t m[9], const int32_t v[3], int32_t out[3]);

/* 0x0048bc3c (old Ghidra name matrix_int_to_float, wrong: it is a plain transpose). */
void tm2_matrix_transpose(const int32_t in[9], int32_t out[9]);

/* matrix_load_globals 0x0048b960 + matrix_multiply 0x0048bfb8:
 * load A into a module global, then out = fx12(A * B) per element. Same two step API as the
 * original because callers use the loaded matrix for several multiplies. */
void tm2_matrix_load(const int32_t a[9]);
void tm2_matrix_multiply(const int32_t b[9], int32_t out[9]);

/* Wrap to [-0x7FF, +0x7FE] by steps of 0xFFF (not 0x1000, as in the original).
 *   math_angle_normalize 0x0048ca2c: int16 in place (16 bit arithmetic)
 *   0x0048c9f4 (old Ghidra name math_cross_product, wrong): int32 in place
 *   math_normalize_int 0x0048c8e0: three int32 in place */
void tm2_angle_wrap16(int16_t* angle);
void tm2_angle_wrap32(int32_t* angle);
void tm2_angle_wrap32x3(int32_t v[3]);

/* 60 Hz ticks of the current frame: frame_get_ticks60 0x004931d4, set by
 * frame_timer_update 0x004930ac as 60 / fps (min 1, fps clamped >= 12, so 1..5). */
int16_t tm2_frame_ticks60(void);
void tm2_set_frame_ticks60(int16_t ticks);

/* frame_get_fps 0x004931dc: the int at 0x004e8304, set by the frame timer. */
int32_t tm2_frame_fps(void);
void tm2_set_frame_fps(int32_t fps);

/* math_value_approach 0x0048cb04 (EAX=delta, EDX=current, EBX=target, ECX=rate):
 * current + (rate * delta * ticks60) / 100, clamped to not pass target; moves by 1 if that rounds
 * to no movement. math_angle_interp 0x0048ca54 wraps delta and result to the signed angle range.
 * math_angle_step 0x0048caec uses delta = target - current without wrapping. */
int32_t tm2_value_approach(int32_t delta, int32_t current, int32_t target, int32_t rate);
int32_t tm2_angle_interp(int32_t current, int32_t target, int32_t rate);
int32_t tm2_angle_step(int32_t current, int32_t target, int32_t rate);

#endif /* TM2_MATH_H */
