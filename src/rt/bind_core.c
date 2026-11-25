/*
 * bind_core.c: src/core (the C runtime's rand / srand, the frame clock, the 16 bit blit) and the
 * fixed point math of src/math/tm2math.c (0x0048b5a0..0x0048cc14), called the way
 * tools/emu/diff_rand.py, diff_frame_timer.py, diff_shell_bg.py and diff_math.py called the
 * originals: registers in order, then the stack words; pointers are game addresses whose host
 * bytes the ports get directly (so in place / aliased arguments behave as in the original).
 *
 * Host state: tm2rand keeps the one random state every port draws from (crt_rand / crt_srand
 * are bound to it here). tm2math's tables are loaded once from TM2_DATA (default disc/files);
 * its frame ticks / fps are copied from the frame clock's globals before the value approach
 * family runs and after the frame clock updates. tm2math's loaded matrix (matrix_load_globals
 * 0x0048b960 for matrix_multiply 0x0048bfb8) is host state shared by every caller through
 * these two bindings.
 */
#include "rt_env.h"

#include <stdio.h>
#include <stdlib.h>

#include "core/blit.h"
#include "core/frame_timer.h"
#include "core/tm2rand.h"
#include "math/tm2math.h"

#define A(i) rt_arg(a, n, (i))

static void math_tables(void)
{
    static int loaded;
    if (!loaded) {
        const char* dir = getenv("TM2_DATA");
        char s[1024], x[1024];
        if (!dir || !*dir) dir = "disc/files";
        snprintf(s, sizeof s, "%s/SINTAB.DAT", dir);
        snprintf(x, sizeof x, "%s/TM2.EXE", dir);
        if (tm2_math_load_tables(s, x) != 0) rt_stop("math tables (set TM2_DATA to the game's files)", 0);
        loaded = 1;
    }
}

#define G_TICKS60 0x004e8300u
#define G_FPS 0x004e8304u

static void frame_sync(void)
{
    tm2_set_frame_ticks60((int16_t)rt_r16(G_TICKS60));
    tm2_set_frame_fps((int32_t)rt_r32(G_FPS));
}

/* ---- rand ---- */

static uint32_t b_crt_rand(const uint32_t* a, int n) { (void)a; (void)n; return (uint32_t)tm2_rand(); }
static uint32_t b_crt_srand(const uint32_t* a, int n) { tm2_srand(A(0)); return 0; }

/* ---- frame clock (globals: frame_timer.h) ---- */

static const uint32_t FT[9] = {0x00a03654u, 0x00a03658u, 0x00a03650u, 0x00a0365cu, 0x00a0364cu,
                               0x004e8304u, 0x004e8308u, 0x004e830cu, 0x004e8300u};

static void ft_load(TM2FrameTimer* t)
{
    uint32_t* f[9] = {&t->last_us, &t->now_us, &t->dt_us, &t->max_fps, &t->frames, &t->fps, &t->prev_fps, &t->raw_fps, &t->ticks60};
    for (int i = 0; i < 9; i++) *f[i] = rt_r32(FT[i]);
}
static void ft_store(const TM2FrameTimer* t)
{
    const uint32_t f[9] = {t->last_us, t->now_us, t->dt_us, t->max_fps, t->frames, t->fps, t->prev_fps, t->raw_fps, t->ticks60};
    for (int i = 0; i < 9; i++) rt_w32(FT[i], f[i]);
}

/* the clock reading of 0x004930ac in microseconds: QueryPerformanceCounter (import slot
 * 0x00c902f0) scaled by the double at 0x00a03660 while 0x00a03668 (its last result) is set, else
 * timeGetTime (slot 0x00c90640) * 1000. The original: (hi * 2^32 (double 0x004e2e60) + lo) * scale,
 * stored with truncation (0x004979a2 sets the x87 to chop). */
static uint32_t clock_us(void)
{
    if (rt_r32(0x00a03668u)) {
        uint32_t f = rt_frame_push(8);
        uint32_t v[1] = {f};
        uint32_t ok = rt_call(0x00c902f0u, v, 1);
        double k, scale;
        memcpy(&k, rt_ptr(0x004e2e60u), 8);
        memcpy(&scale, rt_ptr(0x00a03660u), 8);
        double t = ((double)(int32_t)rt_r32(f + 4) * k + (double)rt_r32(f)) * scale;
        rt_frame_pop(8);
        rt_w32(0x00a03668u, ok);
        return (uint32_t)(int64_t)t;
    }
    return rt_call(0x00c90640u, NULL, 0) * 1000u;
}

/* 0x004930ac */
static uint32_t b_frame_timer_update(const uint32_t* a, int n)
{
    (void)a; (void)n;
    uint32_t now = clock_us();
    TM2FrameTimer t;
    ft_load(&t);
    frame_timer_update(&t, now);
    ft_store(&t);
    frame_sync();
    return 0;
}
/* 0x00493274(tics) */
static uint32_t b_frame_tics_to_rate(const uint32_t* a, int n)
{
    TM2FrameTimer t;
    ft_load(&t);
    return (uint32_t)frame_tics_to_rate(&t, A(0));
}

/* ---- blit ---- */

/* 0x00497854(src, sx, sy, w, then on the stack h, src pitch, dst, dx, dy, dst pitch, unused) */
static uint32_t b_blit16(const uint32_t* a, int n)
{
    uint32_t s = A(0), d = A(6);
    return (uint32_t)tm2_blit16(s ? rt_ptr(s) : NULL, (int32_t)A(1), (int32_t)A(2), A(3), (int32_t)A(4), (int32_t)A(5),
                                d ? rt_ptr(d) : NULL, (int32_t)A(7), (int32_t)A(8), (int32_t)A(9));
}

/* ---- tm2math ---- */

#define P32(x) ((int32_t*)(void*)rt_ptr(x))
#define P16(x) ((int16_t*)(void*)rt_ptr(x))

static uint32_t b_sin(const uint32_t* a, int n) { math_tables(); return (uint32_t)tm2_sin((int32_t)A(0)); }
static uint32_t b_cos(const uint32_t* a, int n) { math_tables(); return (uint32_t)tm2_cos((int32_t)A(0)); }
/* 0x0048b8a0(angle as int16, out cos, out sin) */
static uint32_t b_sincos(const uint32_t* a, int n) { math_tables(); tm2_sincos((int16_t)A(0), P32(A(1)), P32(A(2))); return 0; }
static uint32_t b_atan2(const uint32_t* a, int n) { math_tables(); return (uint32_t)tm2_atan2((int32_t)A(0), (int32_t)A(1)); }
/* 0x0048c360(int16 angles[3], out[9]) */
static uint32_t b_build_matrix(const uint32_t* a, int n) { math_tables(); tm2_build_matrix(P16(A(0)), P32(A(1))); return 0; }
/* 0x0048cb80 / 0x0048cc14(m, v, out) */
static uint32_t b_mat_vec_mul_col(const uint32_t* a, int n) { tm2_mat_vec_mul_col(P32(A(0)), P32(A(1)), P32(A(2))); return 0; }
static uint32_t b_mat_vec_mul_row(const uint32_t* a, int n) { tm2_mat_vec_mul_row(P32(A(0)), P32(A(1)), P32(A(2))); return 0; }
static uint32_t b_matrix_transpose(const uint32_t* a, int n) { tm2_matrix_transpose(P32(A(0)), P32(A(1))); return 0; }
static uint32_t b_matrix_load(const uint32_t* a, int n) { tm2_matrix_load(P32(A(0))); return 0; }
static uint32_t b_matrix_multiply(const uint32_t* a, int n) { tm2_matrix_multiply(P32(A(0)), P32(A(1))); return 0; }
static uint32_t b_angle_wrap32(const uint32_t* a, int n) { tm2_angle_wrap32(P32(A(0))); return 0; }
static uint32_t b_angle_wrap16(const uint32_t* a, int n) { tm2_angle_wrap16(P16(A(0))); return 0; }
static uint32_t b_angle_wrap32x3(const uint32_t* a, int n) { tm2_angle_wrap32x3(P32(A(0))); return 0; }
static uint32_t b_value_approach(const uint32_t* a, int n)
{
    frame_sync();
    return (uint32_t)tm2_value_approach((int32_t)A(0), (int32_t)A(1), (int32_t)A(2), (int32_t)A(3));
}
static uint32_t b_angle_interp(const uint32_t* a, int n) { frame_sync(); return (uint32_t)tm2_angle_interp((int32_t)A(0), (int32_t)A(1), (int32_t)A(2)); }
static uint32_t b_angle_step(const uint32_t* a, int n) { frame_sync(); return (uint32_t)tm2_angle_step((int32_t)A(0), (int32_t)A(1), (int32_t)A(2)); }

void rt_bind_core(void)
{
    rt_bind(0x004899c2u, b_crt_rand, "crt_rand");
    rt_bind(0x004899e6u, b_crt_srand, "crt_srand");
    rt_bind(0x004930acu, b_frame_timer_update, "frame_timer_update");
    rt_bind(0x00493274u, b_frame_tics_to_rate, "frame_tics_to_rate");
    rt_bind(0x00497854u, b_blit16, "blit16");

    rt_bind(0x0048b624u, b_sin, "math_sin");
    rt_bind(0x0048b690u, b_cos, "math_cos");
    rt_bind(0x0048b8a0u, b_sincos, "math_sincos");
    rt_bind(0x0048b71cu, b_atan2, "math_atan2");
    rt_bind(0x0048c360u, b_build_matrix, "math_build_matrix");
    rt_bind(0x0048cb80u, b_mat_vec_mul_col, "math_mat_vec_mul_col");
    rt_bind(0x0048cc14u, b_mat_vec_mul_row, "math_mat_vec_mul_row");
    rt_bind(0x0048bc3cu, b_matrix_transpose, "matrix_transpose");
    rt_bind(0x0048b960u, b_matrix_load, "matrix_load_globals");
    rt_bind(0x0048bfb8u, b_matrix_multiply, "matrix_multiply");
    rt_bind(0x0048c9f4u, b_angle_wrap32, "math_angle_wrap32");
    rt_bind(0x0048ca2cu, b_angle_wrap16, "math_angle_normalize");
    rt_bind(0x0048c8e0u, b_angle_wrap32x3, "math_normalize_int3");
    rt_bind(0x0048cb04u, b_value_approach, "math_value_approach");
    rt_bind(0x0048ca54u, b_angle_interp, "math_angle_interp");
    rt_bind(0x0048caecu, b_angle_step, "math_angle_step");
}

RT_BIND_AREA(core)
