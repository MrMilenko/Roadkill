/*
 * pf3_tex.c: TM2.EXE 0x004bff08 tex_create (hardware 3D path: a Direct3D texture for a texture
 * page), ported over Pf2Env (pf2_env.h). Emulator diff: tools/emu/check_pf3_tex.sh.
 * x87 at 0x127f: the size division is done in double with the x87 NaN rules (gpu3d_float.h),
 * 0x004979a2 (frndint with chop) is trunc, fistp gives 0x80000000 out of range.
 *
 * tex_create(eax obj, edx texture index, ebx float* x scale, ecx float* y scale):
 *   texture info 0x00a30c48 + 48 i: +0x00 (to the upload), +0x04 width, +0x08 height, +0x0c CLUT
 *   pointer (256 PS1 colors), +0x10 color keyed, +0x20 (to the upload);
 *   texture object: +0x00 D3DTEXTUREHANDLE, +0x04 alpha bits of the format, +0x08 IDirect3DTexture2,
 *   +0x0c the texture surface, +0x10 palette, +0x14 1 = the surface is a separate video memory copy;
 *   the chosen texture format (DDPIXELFORMAT, 32 bytes) at 0x00c8b350.
 * Interfaces: the device (PF2_IDEV2) GetDirect3D 0x40, GetCaps 0x0c; IDirect3D2 (PF2_ID3D2)
 * QueryInterface(0x004e5504) -> PF3_IQ, whose slot 0x90 gives PF3_IDD (CreatePalette 0x14,
 * CreateSurface 0x18); surfaces (PF2_ISURF) AddRef 4, SetColorKey 0x74, SetPalette 0x7c,
 * QueryInterface(0x004e55e4) -> PF3_ITEX (GetHandle 0x0c, Load 0x14).
 */
#include "pf2_env.h"
#include "../render/gpu3d_float.h"

#define R32 pf2_r32
#define W32 pf2_w32
#define PF3_IQ 11
#define PF3_IDD 12
#define PF3_ITEX 13

#define F_GET_DEVICE 0x004b5f8cu
#define F_FIT_SIZE 0x004bfc88u     /* tex_fit_size(eax index, edx x scale, ebx y scale, ecx device) */
#define F_TEX_RELEASE 0x004c1024u  /* releases a texture object's parts (eax obj) */
#define F_UPLOAD 0x004c0488u       /* (eax obj, edx surface, ebx info +0, ecx CLUT, 10 stack words; ret 0x28) */

static int32_t size_div(uint32_t v, uint32_t scale_bits) {
    double q = g3_div((double)(int32_t)v, (double)g3_ffrom(scale_bits));
    return g3_fistp(__builtin_trunc(q));
}

uint32_t pf3_tex_create(Pf2Env* e, uint32_t obj, uint32_t i, uint32_t sxp, uint32_t syp) {
    uint32_t info = 0x00a30c48u + 48u * i;
    uint32_t s = e->scratch, big = e->scratch + 0x200;
    uint32_t surf = s, d3d = s + 4, q = s + 8, palout = s + 0xc, ddp = s + 0x10, tex2 = s + 0x14, ck = s + 0x20;
    uint32_t pal = big, hw = big + 0x400, hel = big + 0x4d0, desc = big + 0x5a0;
    uint32_t i0 = R32(e, info), clut = R32(e, info + 0xc), i20 = R32(e, info + 0x20);
    uint32_t w = R32(e, info + 4), h = R32(e, info + 8), keyed = R32(e, info + 0x10);
    uint32_t dev = pf2_call1(e, F_GET_DEVICE, PF2_DISPLAY);
    pf2_call4(e, F_FIT_SIZE, i, sxp, syp, dev);
    int32_t w2 = size_div(w, R32(e, sxp));
    int32_t h2 = size_div(h, R32(e, syp));

    /* the IDirectDraw of the device: GetDirect3D, QueryInterface(0x004e5504), its slot 0x90.
       ORIGINAL BUG: each object is released at once and the last one (dd) used after its
       Release. Any failure: return 0 (nothing released). */
    uint32_t dd = 0;
    if (dev && PF2_C1(e, PF2_IDEV2, 0x40, dev, d3d) == 0 &&
        PF2_C2(e, PF2_ID3D2, 0, R32(e, d3d), 0x004e5504u, q) == 0) {
        PF2_C0(e, PF2_ID3D2, 8, R32(e, d3d));
        PF2_C1(e, PF3_IQ, 0x90, R32(e, q), ddp);
        PF2_C0(e, PF3_IQ, 8, R32(e, q));
        PF2_C0(e, PF3_IDD, 8, R32(e, ddp));
        dd = R32(e, ddp);
    }
    if (dd == 0) return 0;

    pf2_call1(e, F_TEX_RELEASE, obj);
    pf2_zero(e, desc, 0x6c);
    W32(e, desc, 0x6c);
    for (uint32_t k = 0; k < 32; k += 4) W32(e, desc + 0x48 + k, R32(e, 0x00c8b350u + k));
    uint32_t amask = R32(e, desc + 0x64), bits = 0;
    for (int b = 0; b < 32; b++) bits += (amask >> b) & 1;
    W32(e, obj + 4, bits);
    W32(e, desc + 0xc, (uint32_t)w2);
    pf2_w16(e, desc + 4, (uint16_t)(R32(e, desc + 4) | 0x1007));   /* CAPS | HEIGHT | WIDTH | PIXELFORMAT */
    W32(e, desc + 8, (uint32_t)h2);
    uint32_t bpp = R32(e, desc + 0x54);
    pf2_zero(e, hw, 0xcc);
    W32(e, hw, 0xcc);
    pf2_zero(e, hel, 0xcc);
    W32(e, hel, 0xcc);
    PF2_C2(e, PF2_IDEV2, 0x0c, dev, hw, hel);
    W32(e, desc + 0x68, 0x1800);                       /* SYSTEMMEMORY | TEXTURE */
    uint32_t r = PF2_C3(e, PF3_IDD, 0x18, dd, desc, surf, 0);
    if (r) goto fail;
    if (R32(e, hw + 8) != 0) {                         /* a hardware device (color model set) */
        W32(e, desc + 0x68, 0x4005000);                /* TEXTURE | VIDEOMEMORY | ALLOCONLOAD */
        r = PF2_C3(e, PF3_IDD, 0x18, dd, desc, obj + 0xc, 0);
        if (r) goto fail;
        W32(e, obj + 0x14, 1);
    } else {
        W32(e, obj + 0xc, R32(e, surf));
        PF2_C0(e, PF2_ISURF, 4, R32(e, surf));
        W32(e, obj + 0x14, 0);
    }
    {
        uint32_t st[10] = {i20, bpp, R32(e, desc + 0x58), R32(e, desc + 0x5c), R32(e, desc + 0x60), amask,
                           (uint32_t)w2, (uint32_t)h2, R32(e, sxp), R32(e, syp)};
        uint32_t a[14] = {obj, R32(e, surf), i0, clut};
        for (int k = 0; k < 10; k++) a[4 + k] = st[k];
        r = e->call(e->user, F_UPLOAD, a, 14);
        if (r) goto fail;
    }
    if ((int32_t)bpp <= 8) {
        /* palette: PS1 colors (red in the low bits) -> PALETTEENTRY with the red field in peBlue
           and the blue field in peRed (bytes 2 / 0), peFlags 0xff for the STP bit */
        W32(e, palout, 0);
        for (uint32_t k = 0; k < 256; k++) {
            uint32_t c = pf2_r16(e, clut + 2 * k);
            uint32_t v = (c & 0x1f) << 19 | (c & 0x3e0) << 6 | (c & 0x7c00) >> 7;
            W32(e, pal + 4 * k, v | ((c & 0x8000) ? 0xff000000u : 0));
        }
        PF2_C4(e, PF3_IDD, 0x14, dd, 0x44, pal, palout, 0);
        W32(e, obj + 0x10, R32(e, palout));
        PF2_C1(e, PF2_ISURF, 0x7c, R32(e, surf), R32(e, obj + 0x10));
        PF2_C1(e, PF2_ISURF, 0x7c, R32(e, obj + 0xc), R32(e, obj + 0x10));
    }
    if (keyed) {
        if (R32(e, desc + 0x54) <= 8) {                /* palettized: key index 0xff */
            W32(e, ck, 0xff);
            W32(e, ck + 4, 0xff);
            PF2_C2(e, PF2_ISURF, 0x74, R32(e, surf), 8, ck);
            PF2_C2(e, PF2_ISURF, 0x74, R32(e, obj + 0xc), 8, ck);
        } else if (R32(e, obj + 4) == 0) {             /* no alpha bits: key color 0 */
            W32(e, ck, 0);
            W32(e, ck + 4, 0);
            PF2_C2(e, PF2_ISURF, 0x74, R32(e, surf), 8, ck);
            PF2_C2(e, PF2_ISURF, 0x74, R32(e, obj + 0xc), 8, ck);
        }
    }
    PF2_C2(e, PF2_ISURF, 0, R32(e, obj + 0xc), 0x004e55e4u, obj + 8);   /* IDirect3DTexture2, unchecked */
    r = PF2_C2(e, PF3_ITEX, 0x0c, R32(e, obj + 8), dev, obj);          /* GetHandle -> obj +0 */
    if (r) goto fail;
    if (R32(e, obj + 0x14)) {
        PF2_C2(e, PF2_ISURF, 0, R32(e, surf), 0x004e55e4u, tex2);
        PF2_C1(e, PF3_ITEX, 0x14, R32(e, obj + 8), R32(e, tex2));      /* Load from the system copy */
        PF2_C0(e, PF3_ITEX, 8, R32(e, tex2));
    }
    PF2_C0(e, PF2_ISURF, 8, R32(e, surf));
    return 0;
fail:
    pf2_call1(e, F_TEX_RELEASE, obj);
    return r;
}
