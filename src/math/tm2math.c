/*
 * tm2math.c: TM2 fixed point math, ported from TM2.EXE. See tm2math.h.
 * Verified against the original code by tools/emu/check_math.sh.
 */
#include "tm2math.h"

#include <stdio.h>
#include <string.h>

#define SINTAB_COUNT 1024      /* one quadrant, int32, 0x1000 = 1.0 */
#define ATAN_COUNT   513       /* atan(i / 512) for i = 0..512, angle units */
#define ATAN_VA      0x004e8a0cu

static int32_t s_sintab[SINTAB_COUNT];
static int16_t s_atan[ATAN_COUNT];
static int32_t s_mat[9];          /* matrix_load_globals target (g_mat_tmp_00..22) */
static int16_t s_ticks60 = 1;     /* 0x004e8300 */
static int32_t s_fps = 60;        /* 0x004e8304 */

/* ---- table loading ------------------------------------------------------------------- */

static uint32_t rd32(const unsigned char* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const unsigned char* p) { return (uint16_t)(p[0] | p[1] << 8); }

static int load_sintab(const char* path) {
    unsigned char buf[SINTAB_COUNT * 4];
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "tm2math: cannot open %s\n", path); return -1; }
    size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    if (n != sizeof buf) { fprintf(stderr, "tm2math: %s is %zu bytes, expected %zu\n", path, n, sizeof buf); return -1; }
    for (int i = 0; i < SINTAB_COUNT; i++) s_sintab[i] = (int32_t)rd32(buf + 4 * i);
    return 0;
}

/* Find VA in TM2.EXE's section table and read the atan table from the file. */
static int load_atan(const char* path) {
    unsigned char hdr[0x400];
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "tm2math: cannot open %s\n", path); return -1; }
    int rc = -1;
    if (fread(hdr, 1, sizeof hdr, f) != sizeof hdr) goto done;
    uint32_t pe = rd32(hdr + 0x3c);
    if (pe + 0x18 > sizeof hdr || memcmp(hdr + pe, "PE\0\0", 4) != 0) goto done;
    uint16_t nsec = rd16(hdr + pe + 6), optsz = rd16(hdr + pe + 0x14);
    uint32_t base = rd32(hdr + pe + 0x18 + 0x1c);
    uint32_t sec = pe + 0x18 + optsz;
    for (uint16_t i = 0; i < nsec && sec + 40 * (i + 1) <= sizeof hdr; i++) {
        const unsigned char* s = hdr + sec + 40 * i;
        uint32_t va = base + rd32(s + 12), rawsz = rd32(s + 16), rawptr = rd32(s + 20);
        if (ATAN_VA >= va && ATAN_VA + ATAN_COUNT * 2 <= va + rawsz) {
            unsigned char buf[ATAN_COUNT * 2];
            if (fseek(f, (long)(rawptr + (ATAN_VA - va)), SEEK_SET) != 0) goto done;
            if (fread(buf, 1, sizeof buf, f) != sizeof buf) goto done;
            for (int k = 0; k < ATAN_COUNT; k++) s_atan[k] = (int16_t)rd16(buf + 2 * k);
            rc = 0;
            break;
        }
    }
done:
    fclose(f);
    /* sanity check: starts at 0, ends just under 45 degrees (0x200), never decreases */
    if (rc == 0) {
        if (s_atan[0] != 0 || s_atan[ATAN_COUNT - 1] < 0x1F0 || s_atan[ATAN_COUNT - 1] > 0x200) rc = -1;
        for (int k = 1; rc == 0 && k < ATAN_COUNT; k++) if (s_atan[k] < s_atan[k - 1]) rc = -1;
    }
    if (rc != 0) fprintf(stderr, "tm2math: atan table not found in %s (is this the 1997 PC TM2.EXE?)\n", path);
    return rc;
}

int tm2_math_load_tables(const char* sintab_path, const char* exe_path) {
    if (load_sintab(sintab_path) != 0) return -1;
    if (load_atan(exe_path) != 0) return -2;
    return 0;
}

/* ---- sin / cos ----------------------------------------------------------------------- */

/* Index into the full turn as the original computes it (0..0x1000; 0x1000 only for -0x1000*k). */
static int32_t turn_index(int32_t angle) {
    if (angle < 0) return 0x1000 - (int32_t)((0u - (uint32_t)angle) & 0xFFF);
    return angle & 0xFFF;
}

int32_t tm2_sin(int32_t angle) {
    int32_t u = turn_index(angle);
    if (u == 0x1000) return 0;                       /* original reads outside the table here */
    if (u < 0x400) return s_sintab[u];
    if (u < 0x800) return s_sintab[0x7FF - u];
    if (u < 0xC00) return -s_sintab[u - 0x800];
    return -s_sintab[0xFFF - u];
}

int32_t tm2_cos(int32_t angle) {
    int32_t u = turn_index(angle);
    if (u == 0x1000) return 0x1000;                  /* original reads outside the table here */
    if (u < 0x400) return s_sintab[0x3FF - u];
    if (u < 0x800) return -s_sintab[u - 0x400];
    if (u < 0xC00) return -s_sintab[0xBFF - u];
    return s_sintab[u - 0xC00];
}

void tm2_sincos(int16_t angle, int32_t* out_cos, int32_t* out_sin) {
    /* the original does the same quadrant logic on the 16 bit angle */
    *out_sin = tm2_sin(angle);
    *out_cos = tm2_cos(angle);
}

/* ---- atan2 --------------------------------------------------------------------------- */

int32_t tm2_atan2(int32_t x, int32_t y) {
    if (y == 0) return x < 0 ? -0x400 : 0x400;
    if (x == 0) return y < 0 ? 0x800 : 0;
    /* octant: bit0 = x < 0, bit1 = y < 0, bit2 = |y| <= |x| */
    int oct = (x < 0) | (y < 0) << 1;
    int32_t ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
    if (ay <= ax) oct |= 4;
    int32_t x9 = tm2_wmul(x, 0x200), y9 = tm2_wmul(y, 0x200);
    switch (oct) {
    case 0: return s_atan[x9 / y];
    case 1: return -s_atan[-x9 / y];
    case 2: return (int16_t)(0x800 - s_atan[x9 / -y]);
    case 3: return (int16_t)(s_atan[x9 / y] - 0x800);
    case 4: return (int16_t)(0x400 - s_atan[y9 / x]);
    case 5: return (int16_t)(s_atan[y9 / -x] - 0x400);
    case 6: return (int16_t)(s_atan[-y9 / x] + 0x400);
    default: return (int16_t)(-0x400 - s_atan[y9 / x]);
    }
}

/* ---- matrices ------------------------------------------------------------------------ */

void tm2_build_matrix(const int16_t angles[3], int32_t out[9]) {
    int32_t c0, s0, c1, s1, c2, s2;
    /* call order and argument offsets as in the original: +4, +0, +2 */
    tm2_sincos(angles[2], &c2, &s2);
    tm2_sincos(angles[0], &c0, &s0);
    tm2_sincos(angles[1], &c1, &s1);
    int32_t s1s0 = tm2_fx12(tm2_wmul(s1, s0));
    int32_t c1s0 = tm2_fx12(tm2_wmul(c1, s0));
    out[0] = tm2_fx12(tm2_wadd(tm2_wmul(c1, c2), tm2_wmul(s1s0, s2)));
    out[1] = tm2_fx12(tm2_wadd(tm2_wmul(c2, s1s0), tm2_wmul(-s2, c1)));
    out[2] = tm2_fx12(tm2_wmul(s1, c0));
    out[3] = tm2_fx12(tm2_wmul(c0, s2));
    out[4] = tm2_fx12(tm2_wmul(c0, c2));
    out[5] = -s0;
    out[6] = tm2_fx12(tm2_wadd(tm2_wmul(-s1, c2), tm2_wmul(c1s0, s2)));
    out[7] = tm2_fx12(tm2_wadd(tm2_wmul(s1, s2), tm2_wmul(c1s0, c2)));
    out[8] = tm2_fx12(tm2_wmul(c1, c0));
}

static int32_t dot3(int32_t a0, int32_t a1, int32_t a2, int32_t b0, int32_t b1, int32_t b2) {
    return tm2_wadd(tm2_wadd(tm2_wmul(a0, b0), tm2_wmul(a1, b1)), tm2_wmul(a2, b2));
}

void tm2_mat_vec_mul_col(const int32_t m[9], const int32_t v[3], int32_t out[3]) {
    for (int i = 0; i < 3; i++) out[i] = tm2_fx12(dot3(m[3 * i], m[3 * i + 1], m[3 * i + 2], v[0], v[1], v[2]));
}

void tm2_mat_vec_mul_row(const int32_t m[9], const int32_t v[3], int32_t out[3]) {
    for (int i = 0; i < 3; i++) out[i] = tm2_fx12(dot3(m[i], m[i + 3], m[i + 6], v[0], v[1], v[2]));
}

void tm2_matrix_transpose(const int32_t in[9], int32_t out[9]) {
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) out[3 * c + r] = in[3 * r + c];
}

void tm2_matrix_load(const int32_t a[9]) { memcpy(s_mat, a, sizeof s_mat); }

void tm2_matrix_multiply(const int32_t b[9], int32_t out[9]) {
    int32_t r[9];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            r[3 * i + j] = tm2_fx12(dot3(s_mat[3 * i], s_mat[3 * i + 1], s_mat[3 * i + 2], b[j], b[j + 3], b[j + 6]));
    memcpy(out, r, sizeof r);
}

/* ---- angle wrapping ------------------------------------------------------------------ */

void tm2_angle_wrap16(int16_t* angle) {
    while (*angle > 0x7FE) *angle = (int16_t)(*angle - 0xFFF);
    while (*angle < -0x7FF) *angle = (int16_t)(*angle + 0xFFF);
}

void tm2_angle_wrap32(int32_t* angle) {
    while (*angle > 0x7FE) *angle -= 0xFFF;
    while (*angle < -0x7FF) *angle += 0xFFF;
}

void tm2_angle_wrap32x3(int32_t v[3]) {
    for (int i = 0; i < 3; i++) tm2_angle_wrap32(&v[i]);
}

/* ---- frame ticks and approach -------------------------------------------------------- */

int16_t tm2_frame_ticks60(void) { return s_ticks60; }
void tm2_set_frame_ticks60(int16_t ticks) { s_ticks60 = ticks; }
int32_t tm2_frame_fps(void) { return s_fps; }
void tm2_set_frame_fps(int32_t fps) { s_fps = fps; }

int32_t tm2_value_approach(int32_t delta, int32_t current, int32_t target, int32_t rate) {
    int32_t n = (int32_t)tm2_frame_ticks60() * 60 / 60;   /* as in the original */
    int32_t acc = 0;
    for (int32_t i = 0; i < n; i++) acc = tm2_wadd(acc, tm2_wmul(rate, delta));
    int32_t r = tm2_wadd(current, acc / 100);
    if (r == current) {
        if (current == target) return r;
        return delta < 1 ? current - 1 : current + 1;
    }
    if (current < target) return r <= target ? r : target;
    return target <= r ? r : target;
}

int32_t tm2_angle_interp(int32_t current, int32_t target, int32_t rate) {
    int32_t delta = target - current;
    tm2_angle_wrap32(&delta);
    int32_t r = tm2_value_approach(delta, current, target, rate);
    tm2_angle_wrap32(&r);
    return r;
}

int32_t tm2_angle_step(int32_t current, int32_t target, int32_t rate) {
    return tm2_value_approach(target - current, current, target, rate);
}
