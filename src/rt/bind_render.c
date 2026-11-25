/*
 * bind_render.c: the software PS1 GPU (src/render: gpu_*.c, gpu3d*.c) on the runtime.
 *
 * Ownership. The ports keep the renderer's state in one host TM2Gpu (plus the Gpu3d state of the
 * 0xa0 path), whose big tables are not laid out as in game memory (the draw list holds tile
 * indices, not pointers; there is a gap before the mask table). So:
 *   - the tables (edges, tiles, draw list, row spans, masks, row table, noise, overlay list) live
 *     only in the host struct below. Nothing outside the renderer reads them: a scan of TM2.EXE's
 *     code for their addresses, outside 0x004acfe0..0x004b2100 and the kernels at 0x004c6000..,
 *     finds no reference.
 *   - the viewports (0x00c7fa60, 3 x 0x1958) are mirrored to game memory after every viewport
 *     setup: screen_rect_outline 0x004ad624 (en7_d.c) reads viewport 0's rectangle there.
 *   - the scalars (GpuRegs: the REGS table of tools/emu/diff_render_common.py) stay in game memory:
 *     every adapter loads them into the host struct, runs the port and stores them back, so game
 *     code that sets them (render flags 0x00c846d8, fog 0x004e85f8, format 0x00a2dba5, texture
 *     mode 0x004e861c, ...) and the ports see one value. Pointer registers (0x00c847e8 / e4 / e0)
 *     are addresses in game memory and offsets from viewport 0 in the port.
 *   - inputs read on every call: the locked surface (pointer 0x00a04674, pitch 0x00a04670), the
 *     texture page table 0x00a30c48 (TpcPage is the original's 0x30 byte entry, read in place;
 *     its 32 bit addresses are game addresses, so tex_mem is the whole address space), the light
 *     table 0x00c85c68 (read in place, 0x10000 entries: any u16 light index stays inside the
 *     address space, as the original reads whatever is there), dither 0x004e85f4 and special
 *     mode 0x004e82cc.
 * The per primitive scratch of the 0xa0 path (Gpu3d floats at 0x00c84670.., fog steps 0x00c846a4 /
 * 0x00c846a8) is host only: nothing outside the renderer reads it.
 *
 * Calls are as the emulator diffs made them (diff_render_flat / _tex / _init / render3d_*.py):
 * init, pitch (eax), viewport setup (eax i, edx w, ebx h, ecx x0, stack x1, y0, y1), viewport
 * select (eax), frame begin, command_dispatch (eax record). The inner functions, which only the
 * ported renderer itself calls (checked by scanning every call rel32 in TM2.EXE), are bound too,
 * with the registers the original's callers load.
 *
 * Port code (not a decompiled function).
 */
#include "rt_env.h"
#include "render/gpu.h"
#include "render/gpu3d.h"

#include <stddef.h>

#define VP_BASE 0x00c7fa60u
#define EDGE_BASE 0x00be5f10u

static TM2Gpu G;
static Gpu3d S3;
static Gpu3dLights L;

/* the renderer's scalars: game address, size, offset in GpuRegs, 1 = pointer into the viewports */
static const struct { uint32_t addr; uint8_t size; uint16_t off; uint8_t vp; } REGS[] = {
#define R(a, n, f) {a, n, (uint16_t)offsetof(GpuRegs, f), 0}
#define P(a, f) {a, 4, (uint16_t)offsetof(GpuRegs, f), 1}
    R(0x00c84668u, 8, color64), R(0x00c846f0u, 4, pitch_px), R(0x00c846a0u, 4, pitch_bytes),
    R(0x00c84694u, 4, pitch_m16), R(0x00c84688u, 4, pitch_q), R(0x00c84690u, 4, pitch_e),
    R(0x00c846b4u, 4, pitch_px_m8), R(0x00c8469cu, 4, pitch_x4), R(0x00c84698u, 4, pitch_x8),
    R(0x00c846c8u, 4, color32), R(0x00c847c8u, 4, color24), R(0x00c8480eu, 2, color16),
    R(0x00c8480cu, 2, half_mask), R(0x00c847c4u, 4, row_min), R(0x00c847f8u, 4, row_max),
    R(0x00c84704u, 4, hit), R(0x00c847ccu, 4, ntiles), R(0x00c847fcu, 4, f7fc),
    R(0x004e85d4u, 4, stamp), R(0x004e85ecu, 4, flip), R(0x00c847d0u, 4, vp_index),
    P(0x00c847e8u, vp), P(0x00c846e4u, vp_enable), P(0x00c846e0u, vp_rows),
    R(0x004e85c4u, 4, mmx), R(0x004e85c0u, 4, cpu_mmx),
    R(0x00c846d8u, 4, render_flags), R(0x00c846f8u, 4, tex_pixels), R(0x00c846bcu, 4, tex_clut),
    R(0x00c8474cu, 4, tex_w), R(0x00c84748u, 4, tex_h), R(0x00c84724u, 4, tex_shift),
    R(0x00c846c4u, 4, tex_log_a), R(0x00c846dcu, 4, tex_log_b), R(0x00c846ecu, 4, umask),
    R(0x00c846e8u, 4, vmask), R(0x00c846c0u, 4, umask12), R(0x00c846b0u, 4, vmask12),
    R(0x00c84684u, 4, du_dx), R(0x00c84680u, 4, du_dy), R(0x00c84678u, 4, dv_dx),
    R(0x00c8467cu, 4, dv_dy), R(0x00c847f0u, 4, du8x), R(0x00c847ecu, 4, du8y),
    R(0x00c847d4u, 4, dv8x), R(0x00c847d8u, 4, dv8y), R(0x00c8471cu, 4, org_x),
    R(0x00c84728u, 4, org_y), R(0x00c8473cu, 4, u0), R(0x00c8472cu, 4, v0),
    R(0x004e85f8u, 4, fog), R(0x004e861cu, 4, tex_mode), R(0x00a2dba5u, 1, format),
#undef R
#undef P
};

static void sync_in(void)
{
    uint8_t* r = (uint8_t*)&G.r;
    for (size_t i = 0; i < sizeof REGS / sizeof REGS[0]; i++) {
        if (REGS[i].vp) {
            uint32_t v = rt_r32(REGS[i].addr) - VP_BASE;
            memcpy(r + REGS[i].off, &v, 4);
        } else {
            memcpy(r + REGS[i].off, rt_ptr(REGS[i].addr), REGS[i].size);
        }
    }
    /* reads past viewport 2 land on the scalars that follow it in game memory (0x00c84668..) */
    memcpy(G.vp_slack, rt_ptr(VP_BASE + 3u * (uint32_t)sizeof(GpuViewport)), sizeof G.vp_slack);
    uint32_t s = rt_r32(0x00a04674u);
    G.screen = rt_ptr(s);
    G.screen_bytes = (s != 0 && s < RT_SIZE) ? RT_SIZE - s : 0;
    G.pitch = (int32_t)rt_r32(0x00a04670u);
    G.pages = (const TpcPage*)rt_ptr(0x00a30c48u);
    G.tex_mem = rt_base;
    G.tex_addr = 0;
    G.tex_size = RT_SIZE;
    L.base = rt_ptr(0x00c85c68u);
    L.count = 0x10000u;
    G.g3d = &S3;
    G.lights = &L;
    S3.dither = (int32_t)rt_r32(0x004e85f4u);
    S3.special = (int32_t)rt_r32(0x004e82ccu);
}

static void sync_out(void)
{
    const uint8_t* r = (const uint8_t*)&G.r;
    for (size_t i = 0; i < sizeof REGS / sizeof REGS[0]; i++) {
        if (REGS[i].vp) {
            uint32_t v;
            memcpy(&v, r + REGS[i].off, 4);
            rt_w32(REGS[i].addr, v + VP_BASE);
        } else {
            memcpy(rt_ptr(REGS[i].addr), r + REGS[i].off, REGS[i].size);
        }
    }
}

static void mirror_viewports(void) { memcpy(rt_ptr(VP_BASE), G.vp, sizeof G.vp); }

/* rand() for the noise tables: the game's own rand 0x004899c2, so its state stays the game's */
static int b_rand(void* user)
{
    (void)user;
    return (int)rt_call(0x004899c2u, NULL, 0);
}

/* 0x004ad360 init. The original tests CPUID for MMX; every CPU this runs on has it, so mmx = 1
 * (the MMX and plain kernels give the same pixels, see docs/RENDERER.md). */
static uint32_t b_gpu_init(const uint32_t* a, int n)
{
    (void)a; (void)n;
    sync_in();
    gpu_init(&G, 1, b_rand, NULL);
    sync_out();
    mirror_viewports();
    return 0;
}

/* 0x004b1b90: the edge mask table (host only) */
static uint32_t b_build_masks(const uint32_t* a, int n)
{
    (void)a; (void)n;
    gpu_build_masks(G.mask);
    return 0;
}

/* 0x004ad760 (eax pitch in bytes) */
static uint32_t b_set_pitch(const uint32_t* a, int n)
{
    sync_in();
    gpu_set_pitch(&G, (int32_t)rt_arg(a, n, 0));
    sync_out();
    return 0;
}

/* 0x004ad3d4 (eax i, edx w, ebx h, ecx x0, stack x1, y0, y1) */
static uint32_t b_viewport_setup(const uint32_t* a, int n)
{
    sync_in();
    gpu_viewport_setup(&G, (int)rt_arg(a, n, 0), (int32_t)rt_arg(a, n, 1), (int32_t)rt_arg(a, n, 2),
                       (int32_t)rt_arg(a, n, 3), (int32_t)rt_arg(a, n, 4), (int32_t)rt_arg(a, n, 5),
                       (int32_t)rt_arg(a, n, 6));
    sync_out();
    mirror_viewports();
    return 0;
}

/* 0x004ad5e0 (eax i) */
static uint32_t b_viewport_select(const uint32_t* a, int n)
{
    sync_in();
    gpu_viewport_select(&G, (int)rt_arg(a, n, 0));
    sync_out();
    return 0;
}

#define NOARG(name, call) \
    static uint32_t name(const uint32_t* a, int n) { (void)a; (void)n; sync_in(); call; sync_out(); return 0; }

NOARG(b_frame_clear, gpu_frame_clear(&G))              /* 0x004ad084 */
NOARG(b_frame_begin, gpu_frame_begin(&G))              /* 0x004ad848 */
NOARG(b_flat_collect, gpu_flat_collect(&G))            /* 0x004af694 */
NOARG(b_tex_collect, gpu_tex_collect(&G))              /* 0x004af7e8 */
NOARG(b_collect_light, gpu3d_collect_light(&G, &S3))   /* 0x004afadc */
NOARG(b_collect_fog, gpu3d_collect_fog(&G, &S3))       /* 0x004b029c */
NOARG(b_collect_plain, gpu3d_collect_plain(&G, &S3))   /* 0x004b07ec */
NOARG(b_fog_fill, gpu3d_fog_fill(&G, &S3))             /* 0x004b0cb4 */
NOARG(b_overlay_fill, gpu3d_overlay_fill(&G, &S3))     /* 0x004b10ec */
NOARG(b_overlay_mark, gpu3d_overlay_mark(&G, &S3))     /* 0x004b1554 */

/* 0x004ad874 command_dispatch (eax record): every code, 0xa0 included (gpu3d_dispatch_all, as
 * diff_render3d_ngon.py ran it). The original's eax is not used by its callers (0x004963fb,
 * 0x00496807, 0x00496843); the port's 0 is returned. */
static uint32_t b_command_dispatch(const uint32_t* a, int n)
{
    sync_in();
    int r = gpu3d_dispatch_all(&G, &S3, &L, rt_ptr(rt_arg(a, n, 0)));
    sync_out();
    return (uint32_t)r;
}

/* 0x0049653c (eax 0x00RRGGBB; format 0x00a2dba5) */
static uint32_t b_rgb24_to_16(const uint32_t* a, int n)
{
    return gpu_rgb24_to_16(rt_r8(0x00a2dba5u), rt_arg(a, n, 0));
}

/* 0x00496590 (al r, dl g, bl b; format 0x00a2dba5), diff_render3d_rgb.py */
static uint32_t b_rgb_to_16(const uint32_t* a, int n)
{
    return gpu_rgb_to_16(rt_r8(0x00a2dba5u), (uint8_t)rt_arg(a, n, 0), (uint8_t)rt_arg(a, n, 1),
                         (uint8_t)rt_arg(a, n, 2));
}

/* 0x004af118 (eax minor, edx major, ebx one) */
static uint32_t b_slope(const uint32_t* a, int n)
{
    return (uint32_t)gpu_slope((int32_t)rt_arg(a, n, 0), (int32_t)rt_arg(a, n, 1), (int32_t)rt_arg(a, n, 2));
}

/* 0x004af164 (eax edge slot, edx p0, ebx p1: i32 x, y). A slot in the edge table
 * (0x00be5f10..) is the host one; any other address is written in game memory. */
static uint32_t b_edge_setup(const uint32_t* a, int n)
{
    uint32_t ea = rt_arg(a, n, 0);
    int32_t p0[2], p1[2];
    memcpy(p0, rt_ptr(rt_arg(a, n, 1)), 8);
    memcpy(p1, rt_ptr(rt_arg(a, n, 2)), 8);
    uint32_t k = ea - EDGE_BASE;
    if (k < GPU_EDGES * sizeof(GpuEdge) && k % sizeof(GpuEdge) == 0) {
        gpu_edge_setup(&G.edge[k / sizeof(GpuEdge)], p0, p1);
    } else {
        GpuEdge e;
        memcpy(&e, rt_ptr(ea), sizeof e);
        gpu_edge_setup(&e, p0, p1);
        memcpy(rt_ptr(ea), &e, sizeof e);
    }
    return 0;
}

/* 0x004ad030 / 0x004acfe0 (eax x, edx y) */
static uint32_t b_pixel_open(const uint32_t* a, int n)
{
    return gpu_pixel_open(&G, (int32_t)rt_arg(a, n, 0), (int32_t)rt_arg(a, n, 1));
}
static uint32_t b_pixel_take(const uint32_t* a, int n)
{
    gpu_pixel_take(&G, (int32_t)rt_arg(a, n, 0), (int32_t)rt_arg(a, n, 1));
    return 0;
}

/* 0x004b17bc / 0x004b1934 (eax x0, edx y0, ebx x1, ecx y1; color 0x00c8480e) */
static uint32_t b_line(const uint32_t* a, int n)
{
    sync_in();
    gpu_line(&G, (int32_t)rt_arg(a, n, 0), (int32_t)rt_arg(a, n, 1), (int32_t)rt_arg(a, n, 2),
             (int32_t)rt_arg(a, n, 3));
    sync_out();
    return 0;
}
static uint32_t b_line_raw(const uint32_t* a, int n)
{
    sync_in();
    gpu_line_raw(&G, (int32_t)rt_arg(a, n, 0), (int32_t)rt_arg(a, n, 1), (int32_t)rt_arg(a, n, 2),
                 (int32_t)rt_arg(a, n, 3));
    sync_out();
    return 0;
}

void rt_bind_render(void)
{
    rt_bind(0x004ad360u, b_gpu_init, "gpu_init");
    rt_bind(0x004b1b90u, b_build_masks, "gpu_build_masks");
    rt_bind(0x004ad760u, b_set_pitch, "gpu_set_pitch");
    rt_bind(0x004ad3d4u, b_viewport_setup, "gpu_viewport_setup");
    rt_bind(0x004ad5e0u, b_viewport_select, "gpu_viewport_select");
    rt_bind(0x004ad084u, b_frame_clear, "gpu_frame_clear");
    rt_bind(0x004ad848u, b_frame_begin, "gpu_frame_begin");
    rt_bind(0x004ad874u, b_command_dispatch, "command_dispatch");
    rt_bind(0x0049653cu, b_rgb24_to_16, "gpu_rgb24_to_16");
    rt_bind(0x00496590u, b_rgb_to_16, "gpu_rgb_to_16");
    rt_bind(0x004af118u, b_slope, "gpu_slope");
    rt_bind(0x004af164u, b_edge_setup, "gpu_edge_setup");
    rt_bind(0x004af694u, b_flat_collect, "gpu_flat_collect");
    rt_bind(0x004af7e8u, b_tex_collect, "gpu_tex_collect");
    rt_bind(0x004ad030u, b_pixel_open, "gpu_pixel_open");
    rt_bind(0x004acfe0u, b_pixel_take, "gpu_pixel_take");
    rt_bind(0x004b17bcu, b_line, "gpu_line");
    rt_bind(0x004b1934u, b_line_raw, "gpu_line_raw");
    rt_bind(0x004afadcu, b_collect_light, "gpu3d_collect_light");
    rt_bind(0x004b029cu, b_collect_fog, "gpu3d_collect_fog");
    rt_bind(0x004b07ecu, b_collect_plain, "gpu3d_collect_plain");
    rt_bind(0x004b0cb4u, b_fog_fill, "gpu3d_fog_fill");
    rt_bind(0x004b10ecu, b_overlay_fill, "gpu3d_overlay_fill");
    rt_bind(0x004b1554u, b_overlay_mark, "gpu3d_overlay_mark");
}

RT_BIND_AREA(render)
