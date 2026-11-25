/*
 * gpu3d.h: the 3D (float) primitive of TM2.EXE's software GPU, command 0xa0 of
 * command_dispatch 0x004ad874, its three collect functions and their helpers, and the low
 * detail texture kernels. See docs/RENDERER_3D.md (and docs/RENDERER.md for the rest).
 *
 * State the 3D path keeps beyond TM2Gpu (src/render/gpu.h) is in Gpu3d below, with the
 * original's address of every field.
 *
 * Floats: the game runs the x87 with control word 0x127f (Watcom startup 0x004c15bd loads
 * 0x004e94dc = 0x127f: 53 bit precision, round to nearest, all exceptions masked; nothing in
 * the game changes it but 0x004979a2, which restores it). At 53 bit precision every x87
 * add / sub / mul / div is an IEEE double operation as long as no value leaves the double
 * exponent range, which no product / sum / quotient of finite floats in these functions does
 * (see gpu3d_float.h). So the port computes in double, exactly in the original's order and
 * with the original's stores to float, with FP contraction off (-ffp-contract=off), fistp as
 * round to nearest even with the x87 "integer indefinite" 0x80000000 out of range, and the x87
 * NaN rules (gpu3d_float.h).
 */
#ifndef TM2_RENDER_GPU3D_H
#define TM2_RENDER_GPU3D_H

#include <stdint.h>

#include "gpu.h"

/* 0xa0 record (beyond +4 u16 flags, +6 u8 n, +7 code):
 *   +0x08 u32 color, +0x0c u16 page (& 0xfff), +0x0e u16 light index, +0x10 float z,
 *   +0x14.. float gradients, by interpolant (d/dx, d/dy, value at the origin):
 *     u    +0x14 +0x30 +0x4c      v    +0x18 +0x34 +0x50      w (1/z) +0x2c +0x48 +0x64
 *     g0   +0x1c +0x38 +0x54      g1   +0x20 +0x3c +0x58      g2   +0x24 +0x40 +0x5c
 *     e    +0x28 +0x44 +0x60 (flag 0x80)
 *   +0x68 float 1/w clamp min, +0x6c max, +0x70 float origin x, +0x74 origin y (1/32 pixel
 *   with the guard band, as the tile corners: x = (col - row * 0x54... ) * 0x100)
 *   +0x78 n x (i32 x, i32 y) vertices in edge units.
 * Flags: 1 textured, 2 overlay (see gpu3d_overlay_mark), 0x40 no fog, 0x80 the fourth
 * interpolant, 0x4000 gouraud light. */

typedef struct Gpu3d {
    /* per primitive (dispatch 0xa0) */
    uint32_t color;         /* 0x00c847c0 record +8 */
    uint32_t color_r;       /* 0x00c847e4 color >> 16 & 0xff */
    uint32_t color_g;       /* 0x00c847f4 color >> 8 & 0xff */
    uint32_t color_b;       /* 0x00c847dc color & 0xff */
    float z;                /* 0x00c84674 +0x10 */
    float fogk;             /* 0x00c84670 4096.0 / z (fog on and flag 0x40 clear), else 0 */
    /* texture interpolants: raw d/dx, d/dy, origin and per tile (x 256) steps */
    float u_dx, u_dy, u_0;  /* 0x00c847e0 +0x14, 0x00c8476c +0x30, 0x00c84758 +0x4c */
    float u_tx, u_ty;       /* 0x00c8479c +0x14 * 256, 0x00c84798 +0x30 * 256 */
    float v_dx, v_dy, v_0;  /* 0x00c84764 +0x18 << lg, 0x00c8475c +0x34 << lg, 0x00c846fc +0x50 << lg */
    float v_tx, v_ty;       /* 0x00c84790 +0x18 * 256 << lg, 0x00c84794 +0x34 * 256 << lg */
    float w_dx, w_dy, w_0;  /* 0x00c847bc +0x2c, 0x00c847b8 +0x48, 0x00c84714 +0x64 */
    float w_tx, w_ty;       /* 0x00c847a0 +0x2c * 256, 0x00c847a4 +0x48 * 256 */
    float rmin, rmax;       /* 0x00c84730 +0x68, 0x00c84700 +0x6c */
    float org_x, org_y;     /* 0x00c84754 +0x70, 0x00c84750 +0x74 */
    /* gouraud (flag 0x4000): three interpolants and a light */
    float g_dx[3], g_dy[3], g_0[3];   /* 0x00c84788 0x00c847a8 0x00c847b0 / 0x00c8478c 0x00c847ac
                                         0x00c847b4 / 0x00c8470c 0x00c84708 0x00c84720 */
    float g_tx[3], g_ty[3];           /* 0x00c84778 0x00c84774 0x00c8477c / 0x00c84770 0x00c84734
                                         0x00c84740 (x 256) */
    float light_r2;         /* 0x00c84738 light +0 squared */
    float light_k;          /* 0x00c84718 light +0xc */
    uint32_t light_s;       /* 0x00c84744 light +4 (bits of a float) */
    /* flag 0x80 */
    float e_dx, e_dy, e_0;  /* 0x00c84768 +0x28, 0x00c84760 +0x44, 0x00c84710 +0x60 */
    float e_tx, e_ty;       /* 0x00c84784 +0x28 * 256, 0x00c84780 +0x44 * 256 */
    /* helpers (0x004b0cb4, 0x004b10ec): fog steps per pixel / row */
    int32_t fog_dx;         /* 0x00c846a4 */
    int32_t fog_dy;         /* 0x00c846a8 */
    /* inputs (read only) */
    int32_t dither;         /* 0x004e85f4 picks the fog kernel 0x004b0cb4 in 0x004afadc */
    int32_t special;        /* 0x004e82cc g_special_render_mode */
    /* the overlay list 0x00be0b50 (tile indices; the original keeps pointers), count in
     * TM2Gpu r.f7fc (0x00c847fc) */
    uint32_t list2[GPU_TILES];
} Gpu3d;

/* light table 0x00c85c68, 0x78 bytes per entry: the setup reads +0 (float, squared), +4
 * (copied as bits), +0xc (float). */
typedef struct {
    const uint8_t* base;
    uint32_t count;         /* entries readable; others read as 0 */
} Gpu3dLights;

/* 0x004ad874 code 0xa0 (+ the common head: counters, stamp), then the collect it picks.
 * Returns 0. */
int gpu3d_dispatch(TM2Gpu* g, Gpu3d* s, const Gpu3dLights* lights, const uint8_t* prim);

/* the same without the common head of 0x004ad874 (draw list / overlay counts 0, stamp + 1,
 * hit 0): what gpu_dispatch's case 0xa0 calls after doing the head itself */
void gpu3d_prim(TM2Gpu* g, Gpu3d* s, const Gpu3dLights* lights, const uint8_t* prim);

/* the three collects (each ends in its kernel) */
void gpu3d_collect_light(TM2Gpu* g, Gpu3d* s);   /* 0x004afadc */
void gpu3d_collect_fog(TM2Gpu* g, Gpu3d* s);     /* 0x004b029c */
void gpu3d_collect_plain(TM2Gpu* g, Gpu3d* s);   /* 0x004b07ec */

/* helpers (gpu3d_helpers.c) */
void gpu3d_fog_fill(TM2Gpu* g, Gpu3d* s);        /* 0x004b0cb4 */
void gpu3d_overlay_fill(TM2Gpu* g, Gpu3d* s);    /* 0x004b10ec */
void gpu3d_overlay_mark(TM2Gpu* g, Gpu3d* s);    /* 0x004b1554 */

/* textured kernel by r.mmx / r.tex_mode: 0x004c8820, 0x004c71f0 (gpu_tex_fill) and the low
 * detail ones 0x004c7c70 / 0x004c6590 (gpu3d_lowdetail.c) */
void gpu3d_tex_kernel(TM2Gpu* g);
void gpu3d_lowdetail_fill(TM2Gpu* g);            /* 0x004c7c70 (r.mmx 0), 0x004c6590 (MMX) */

/* the whole dispatcher with 0xa0 and the low detail kernels wired in (test / hookup) */
int gpu3d_dispatch_all(TM2Gpu* g, Gpu3d* s, const Gpu3dLights* lights, const uint8_t* prim);

#endif /* TM2_RENDER_GPU3D_H */
