/*
 * pf3_texfmt.c: TM2.EXE's Direct3D texture format choice and size fitting (hardware 3D path,
 * docs/PLATFORM.md Part D), ported over Pf2Env (pf2_env.h). Emulator diff:
 * tools/emu/check_pf3_texfmt.sh. x87 at 0x127f with the x87 NaN rules (gpu3d_float.h).
 *
 * Format choice context (0x40 bytes, on 0x004bfe50's stack): +0x00 wanted bits per pixel, +0x04 /
 * +0x08 / +0x0c / +0x10 wanted R / G / B / alpha bits, +0x14 best |bpp difference|, +0x18 best
 * mask score, +0x1c best |alpha difference| (999999 = none), +0x20 the best DDPIXELFORMAT.
 */
#include "pf2_env.h"
#include "../render/gpu3d_float.h"

#define R32 pf2_r32
#define W32 pf2_w32
#define NONE 999999u

static uint32_t popcount32(uint32_t v) { uint32_t n = 0; for (int b = 0; b < 32; b++) n += (v >> b) & 1; return n; }
static uint32_t iabs(uint32_t v) { return (int32_t)v < 0 ? 0u - v : v; }

static void keep(Pf2Env* e, uint32_t ctx, uint32_t pf) {
    for (uint32_t k = 0; k < 32; k += 4) W32(e, ctx + 0x20 + k, R32(e, pf + k));
}

/* 0x004bf920 tex_format_enum_callback(stack DDSURFACEDESC*, context; stdcall, ret 8). Formats
   without DDPF_RGB (0x40), with DDPF_ALPHA (2), below 8 bits, or 8 bits not palettized (0x28)
   are skipped (returns 1). ORIGINAL BUG: it then compares ebp, the bit count, edx, edx and eax
   with context +4, +8, +0x0c, +0x10, +0x14 (ebp / edx / eax as the caller left them: passed in
   here) and on a match clears +0x14 / +0x18 / +0x1c, keeps the format and returns 0 (stops the
   enumeration). Otherwise: bpp distance d = |bpp - want|, s = sum of |mask bits - wanted bits|;
   a smaller d takes the format (alpha distance kept when both want and have alpha); an equal d:
   with alpha wanted, a better alpha distance (or the first with any when none is recorded yet);
   without, a smaller s. Returns 1 (go on). */
uint32_t pf3_tex_format_enum_cb(Pf2Env* e, uint32_t eax, uint32_t edx, uint32_t ebp, uint32_t desc, uint32_t ctx) {
    uint32_t pf = desc + 0x48;
    uint8_t fl = pf2_r8(e, pf + 4);
    uint32_t bpp = R32(e, pf + 0xc);
    if (fl & 2) return 1;
    if (!(fl & 0x40)) return 1;
    if (bpp < 8) return 1;
    if (bpp == 8 && !(fl & 0x28)) return 1;
    if (ebp == R32(e, ctx + 4) && bpp == R32(e, ctx + 8) && edx == R32(e, ctx + 0xc) &&
        edx == R32(e, ctx + 0x10) && eax == R32(e, ctx + 0x14)) {
        W32(e, ctx + 0x18, 0);
        W32(e, ctx + 0x1c, 0);
        W32(e, ctx + 0x14, 0);
        keep(e, ctx, pf);
        return 0;
    }
    uint32_t d = iabs(bpp - R32(e, ctx));
    uint32_t rc = popcount32(R32(e, pf + 0x10)), gc = popcount32(R32(e, pf + 0x14));
    uint32_t bc = popcount32(R32(e, pf + 0x18)), ac = popcount32(R32(e, pf + 0x1c));
    uint32_t ad = iabs(R32(e, ctx + 0x10) - ac);
    uint32_t s = iabs(rc - R32(e, ctx + 4)) + iabs(gc - R32(e, ctx + 8)) + iabs(bc - R32(e, ctx + 0xc)) +
                 iabs(ac - R32(e, ctx + 0x10));
    if ((int32_t)d < (int32_t)R32(e, ctx + 0x14)) {
        W32(e, ctx + 0x14, d);
        W32(e, ctx + 0x18, s);
        if (R32(e, ctx + 0x10) != 0 && ac != 0) W32(e, ctx + 0x1c, ad);
        keep(e, ctx, pf);
        return 1;
    }
    if (d != R32(e, ctx + 0x14)) return 1;
    if (R32(e, ctx + 0x10) != 0) {
        if ((int32_t)R32(e, ctx + 0x1c) < (int32_t)NONE) {
            if ((int32_t)ac > 0 && (int32_t)ad < (int32_t)R32(e, ctx + 0x1c)) {
                W32(e, ctx + 0x1c, ad);
                W32(e, ctx + 0x18, s);
                keep(e, ctx, pf);
            }
            return 1;
        }
        W32(e, ctx + 0x18, s);
        if (ac) W32(e, ctx + 0x1c, ad);
        keep(e, ctx, pf);
        return 1;
    }
    if ((int32_t)s < (int32_t)R32(e, ctx + 0x18)) {
        W32(e, ctx + 0x1c, NONE);
        W32(e, ctx + 0x18, s);
        keep(e, ctx, pf);
    }
    return 1;
}

/* 0x004bfe50 tex_choose_format(eax bpp, edx R, ebx G, ecx B bits, stack alpha bits; ret 4): the
   context above (bests 999999), IDirect3DDevice2::EnumTextureFormats(0x004bf920, &context), the
   best format copied to 0x00c8b350; returns EnumTextureFormats' result */
uint32_t pf3_tex_choose_format(Pf2Env* e, uint32_t bpp, uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
    uint32_t dev = pf2_call1(e, 0x004b5f8cu, PF2_DISPLAY);
    uint32_t c = e->scratch;
    pf2_zero(e, c, 0x40);
    W32(e, c, bpp);
    W32(e, c + 4, r);
    W32(e, c + 0xc, b);
    W32(e, c + 8, g);
    W32(e, c + 0x14, NONE);
    W32(e, c + 0x18, NONE);
    W32(e, c + 0x1c, NONE);
    W32(e, c + 0x10, a);
    uint32_t res = PF2_C2(e, PF2_IDEV2, 0x24, dev, 0x004bf920u, c);
    for (uint32_t k = 0; k < 32; k += 4) W32(e, 0x00c8b350u + k, R32(e, c + 0x20 + k));
    return res;
}

static int32_t fit(uint32_t v, float s) {
    return g3_fistp(__builtin_trunc(g3_div((double)v, (double)s)));
}

static void scale(Pf2Env* e, uint32_t p, uint32_t cst) {
    pf2_wf(e, p, g3_st(g3_mul((double)pf2_rf(e, p), (double)pf2_rf(e, cst))));
}

/* signed halving as the original's sar sequence (toward zero) */
static int32_t half(int32_t v) { return (int32_t)((uint32_t)v - (uint32_t)(v >> 31)) >> 1; }

/* 0x004bfc88 tex_fit_size(eax texture index, edx float* x scale, ebx float* y scale): w, h = the
   texture size (0x00a30c4c / 0x00a30c50 + 48 i) / scale, chopped. With square textures required
   (0x00c84f50): the longer side is matched to the shorter one, by doubling the shorter (scale *
   [0x004e4920]) when it is less than half the longer, else by halving the longer (scale *
   [0x004e491c]). Size limits 0x00c84f54 / 0x00c84f58 (default 256 when 0; 0x00c84f5c / 0x00c84f60
   default 32) halve w / h while above them. Only the scales are the result (eax undefined). */
void pf3_tex_fit_size(Pf2Env* e, uint32_t i, uint32_t sx, uint32_t sy) {
    const uint32_t C1 = 0x004e4920u, C2 = 0x004e491cu;
    int32_t w = fit(R32(e, 0x00a30c4cu + 48u * i), pf2_rf(e, sx));
    int32_t h = fit(R32(e, 0x00a30c50u + 48u * i), pf2_rf(e, sy));
    if (R32(e, 0x00c84f50u)) {
        if (w > h) {
            if ((int32_t)((uint32_t)h << 1) < w) { while (w > h) { scale(e, sy, C1); h = (int32_t)((uint32_t)h << 1); } }
            else { while (w > h) { w = half(w); scale(e, sx, C2); } }
        }
        if (h > w) {
            if ((int32_t)((uint32_t)w << 1) < h) { while (h > w) { scale(e, sx, C1); w = (int32_t)((uint32_t)w << 1); } }
            else { while (h > w) { h = half(h); scale(e, sy, C2); } }
        }
    }
    if (R32(e, 0x00c84f54u) == 0) W32(e, 0x00c84f54u, 0x100);
    if (R32(e, 0x00c84f58u) == 0) W32(e, 0x00c84f58u, 0x100);
    if (R32(e, 0x00c84f5cu) == 0) W32(e, 0x00c84f5cu, 0x20);
    if (R32(e, 0x00c84f60u) == 0) W32(e, 0x00c84f60u, 0x20);
    if ((int32_t)R32(e, 0x00c84f54u) > 0)
        while (w > (int32_t)R32(e, 0x00c84f54u)) { w = half(w); scale(e, sx, C2); }
    if ((int32_t)R32(e, 0x00c84f58u) > 0)
        while (h > (int32_t)R32(e, 0x00c84f58u)) { h = half(h); scale(e, sy, C2); }
}
