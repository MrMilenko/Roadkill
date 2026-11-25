/*
 * pf2_d3d.c: TM2.EXE's hardware 3D draw path (see pf2_d3d.h, docs/PLATFORM.md Part B).
 *
 * Float code: the original runs the x87 at control word 0x127f (53 bit precision, round to
 * nearest); every x87 operation here is done as the IEEE double operation in the original's
 * order with the x87 NaN rules (src/render/gpu3d_float.h), rounded to float where the
 * original stores a dword. Compile with -ffp-contract=off.
 */
#include "pf2_d3d.h"

#include <string.h>

#include "../render/gpu3d_float.h"

#define BE (d->be)
#define SRS(st, v) BE->set_render_state(BE->user, d->device, (st), (v))

#define RS_SHADEMODE 0      /* 0x00c84f74 */
#define RS_BLEND 13         /* 0x00c84fa8 */
#define RS_FILTER 14        /* 0x00c84fac */
#define RS_SRCBLEND 25      /* 0x00c84fd8 */
#define RS_DESTBLEND 26     /* 0x00c84fdc */

#define D3DRS_TEXTUREHANDLE 1
#define D3DRS_SHADEMODE 9
#define D3DRS_TEXTUREMAG 0x11
#define D3DRS_TEXTUREMIN 0x12
#define D3DRS_SRCBLEND 0x13
#define D3DRS_DESTBLEND 0x14
#define D3DRS_ALPHABLENDENABLE 0x1b
#define D3DPT_LINESTRIP 3
#define D3DPT_TRIANGLEFAN 6
#define D3DVT_TLVERTEX 3
#define TEX_NONE 0x7fffu
#define SZ 0x3dcccccdu      /* 0.1f */
#define RHW 0x41200000u     /* 10.0f */

static uint32_t rd32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static int16_t rdi16(const uint8_t* p) { int16_t v; memcpy(&v, p, 2); return v; }
static uint16_t rdu16(const uint8_t* p) { uint16_t v; memcpy(&v, p, 2); return v; }

/* the texture tables, by index (see pf2_d3d.h) */
static uint32_t tex_handle(const Pf2D3d* d, uint32_t i) { return rd32(d->tex + 24u * i); }
static int32_t tex_ckey_count(const Pf2D3d* d, uint32_t i) { return (int32_t)rd32(d->tex + 4 + 24u * i); }
static const uint8_t* tex_info(const Pf2D3d* d, uint32_t i) { return d->tex + (0x00a30c4cu - PF2_TEX_BASE) + 48u * i; }

/* 0x004b611c set_filter(eax display, edx on): unchanged (rs 0x00c84fac) -> 0; else TEXTUREMAG and
   TEXTUREMIN = 2 (LINEAR) when on, 1 (NEAREST) when off; returns the second call's result */
int32_t pf2_d3d_set_filter(Pf2D3d* d, uint32_t on) {
    if (on == d->rs[RS_FILTER]) return 0;
    d->rs[RS_FILTER] = on;
    uint32_t v = on ? 2 : 1;
    SRS(D3DRS_TEXTUREMAG, v);
    return SRS(D3DRS_TEXTUREMIN, v);
}

/* 0x004b617c set_blend(eax display, edx mode): ALPHABLENDENABLE (cache 0x00c84fa8) turned on
   when it is 0 and mode != 3, turned off when it is 1 and mode == 3 (any other cache value: no
   call). Then by mode (unsigned): 0 and 4: SRCBLEND 5 (SRCALPHA), DESTBLEND 6 (INVSRCALPHA);
   1: 5, 2 (ONE); 2: 1 (ZERO), 6; the caches 0x00c84fd8 / 0x00c84fdc set (not checked first);
   returns DESTBLEND's result. 3: returns 0. Above 4: returns the ALPHABLENDENABLE result if it
   was called, else ecx as the caller left it (ORIGINAL BUG: unset register; the port returns 0;
   the only caller, draw_prim, ignores the result). */
int32_t pf2_d3d_set_blend(Pf2D3d* d, uint32_t mode) {
    int32_t r = 0;
    if (d->rs[RS_BLEND] == 0 && mode != 3) {
        d->rs[RS_BLEND] = 1;
        r = SRS(D3DRS_ALPHABLENDENABLE, 1);
    } else if (d->rs[RS_BLEND] == 1 && mode == 3) {
        d->rs[RS_BLEND] = 0;
        r = SRS(D3DRS_ALPHABLENDENABLE, 0);
    }
    uint32_t src, dst;
    switch (mode) {
    case 0: case 4: src = 5; dst = 6; break;
    case 1: src = 5; dst = 2; break;
    case 2: src = 1; dst = 6; break;
    case 3: return 0;
    default: return r;
    }
    d->rs[RS_DESTBLEND] = dst;
    d->rs[RS_SRCBLEND] = src;
    SRS(D3DRS_SRCBLEND, src);
    return SRS(D3DRS_DESTBLEND, dst);
}

/* ---- draw_prim pieces, each in the original's order ---- */

static void shade(Pf2D3d* d, uint32_t mode) {
    if (d->rs[RS_SHADEMODE] != mode) {
        d->rs[RS_SHADEMODE] = mode;
        SRS(D3DRS_SHADEMODE, mode);
    }
}

static void tex_none(Pf2D3d* d) {
    if (*d->tex_cache != TEX_NONE) {
        *d->tex_cache = TEX_NONE;
        SRS(D3DRS_TEXTUREHANDLE, 0);
    }
}

/* texture index (field & 0xfff) set when it changed, then the filter of the current texture
   (always, unlike the 0x80 record path) */
static void tex_set(Pf2D3d* d, uint32_t field) {
    uint32_t i = field & 0xfff;
    if (i != *d->tex_cache) {
        *d->tex_cache = i;
        SRS(D3DRS_TEXTUREHANDLE, tex_handle(d, i));
    }
    pf2_d3d_set_filter(d, rd32(tex_info(d, *d->tex_cache) + 0x28) == 0);
}

/* textured blend: semi -> mode 0; a color keyed texture (info +0xc != 0 and the handle
   record's count > 0) -> 4; else 3 */
static void blend_tex(Pf2D3d* d, int semi) {
    uint32_t c = *d->tex_cache;
    if (semi) pf2_d3d_set_blend(d, 0);
    else if (rd32(tex_info(d, c) + 0xc) != 0 && tex_ckey_count(d, c) > 0) pf2_d3d_set_blend(d, 4);
    else pf2_d3d_set_blend(d, 3);
}

static uint32_t color(const uint8_t* rec, int off, int semi) {
    return (semi ? 0x80u : 0xffu) << 24 | (uint32_t)rec[off] << 16 | (uint32_t)rec[off + 1] << 8 | rec[off + 2];
}

static float fbits(uint32_t u) { return g3_ffrom(u); }

/* a vertex at the record's i16 pair */
static void vpos(Pf2TlVertex* v, const uint8_t* rec, int off, uint32_t col) {
    v->sx = (float)rdi16(rec + off);
    v->sy = (float)rdi16(rec + off + 2);
    v->sz = fbits(SZ);
    v->rhw = fbits(RHW);
    v->color = col;
}

static int32_t draw(Pf2D3d* d, uint32_t type, const void* v, uint32_t n) {
    return BE->draw_primitive(BE->user, d->device, type, D3DVT_TLVERTEX, v, n, 0);
}

/* the sprite's texture scale / offset: filtered: scale 1 / size, offset 0; unfiltered (info
   +0x28 != 0): scale (1 / size) * ((size - 1) * (1 / size)), offset (1 / size) * 0.5, so the
   texel centres are sampled. Each stored as a float. */
static void sprite_uv(const Pf2D3d* d, float* us, float* uo, float* vs, float* vo) {
    const uint8_t* t = tex_info(d, *d->tex_cache);
    double W = (double)rd32(t), H = (double)rd32(t + 4);
    if (rd32(t + 0x28) != 0) {
        *us = g3_st(g3_div(1.0, W));
        *vs = g3_st(g3_div(1.0, H));
        *uo = 0.0f;
        *vo = 0.0f;
    } else {
        double rw = g3_div(1.0, W);
        double a = g3_mul(g3_add(W, -1.0), rw);
        double rh = g3_div(1.0, H);
        double b = g3_mul(g3_add(-1.0, H), rh);
        *uo = g3_st(g3_mul(rw, 0.5));
        *us = g3_st(g3_mul(rw, a));
        *vo = g3_st(g3_mul(0.5, rh));
        *vs = g3_st(g3_mul(rh, b));
    }
}

/* 0x004b67d0 draw_prim(eax display, edx record): code = record +7 & ~2, semi = +7 & 2.
   Returns DrawPrimitive's result; for a code it does not draw the original returns ecx as the
   caller left it (ORIGINAL BUG, unset register; the port returns 0, the callers ignore it).
   Vertices built here have sz 0.1, rhw 10; specular (and tu / tv when untextured) are not set
   by the original: 0 here. */
int32_t pf2_d3d_draw_prim(Pf2D3d* d, const uint8_t* rec) {
    uint32_t code = rec[7] & 0xfd;
    int semi = (rec[7] & 2) != 0;
    Pf2TlVertex v[4];
    memset(v, 0, sizeof v);

    if (code == 0x80) {
        /* the device record of display_cmd_build 0x00491018: +4 u16 flags (bit 0 textured,
           bit 12 gouraud), +6 u8 count, +8 texture, +0x0c blend mode, +0x14 D3DTLVERTEX[] */
        uint32_t flags = rdu16(rec + 4);
        if (flags & 1) {
            uint32_t i = rd32(rec + 8) & 0xfff;
            if (i != *d->tex_cache) {
                *d->tex_cache = i;
                SRS(D3DRS_TEXTUREHANDLE, tex_handle(d, i));
                pf2_d3d_set_filter(d, rd32(tex_info(d, *d->tex_cache) + 0x28) == 0);
            }
        }
        if (!(flags & 1)) tex_none(d);
        shade(d, (flags & 0x1000) ? 2 : 1);
        if (semi) pf2_d3d_set_blend(d, rd32(rec + 0xc));
        else if (!(flags & 1)) pf2_d3d_set_blend(d, 3);
        else blend_tex(d, 0);
        return draw(d, D3DPT_TRIANGLEFAN, rec + 0x14, rec[6]);
    }
    if (code == 0x20 || code == 0x28) {           /* POLY_F3 / POLY_F4 */
        shade(d, 1);
        tex_none(d);
        pf2_d3d_set_blend(d, semi ? 0 : 3);
        uint32_t c = color(rec, 4, semi);
        vpos(&v[0], rec, 8, c);
        vpos(&v[1], rec, 0xc, c);
        if (code == 0x20) {
            vpos(&v[2], rec, 0x10, c);
            return draw(d, D3DPT_TRIANGLEFAN, v, 3);
        }
        vpos(&v[2], rec, 0x14, c);                /* fan order: 0, 1, 3, 2 */
        vpos(&v[3], rec, 0x10, c);
        return draw(d, D3DPT_TRIANGLEFAN, v, 4);
    }
    if (code == 0x24 || code == 0x2c) {           /* POLY_FT3 / POLY_FT4 */
        int n = code == 0x24 ? 3 : 4;
        shade(d, 1);
        tex_set(d, rd32(rec + (n == 3 ? 0x20 : 0x28)));
        blend_tex(d, semi);
        uint32_t c = color(rec, 4, semi);
        const uint8_t* t = tex_info(d, *d->tex_cache);
        double rh = g3_div(1.0, (double)rd32(t + 4));
        double rw = g3_div(1.0, (double)rd32(t));
        for (int k = 0; k < n; k++) {             /* xy at 8 + 8k, uv at 0xc + 8k (vertex order as stored) */
            vpos(&v[k], rec, 8 + 8 * k, c);
            v[k].tu = g3_st(g3_mul((double)rec[0xc + 8 * k], rw));
            v[k].tv = g3_st(g3_mul((double)rec[0xd + 8 * k], rh));
        }
        return draw(d, D3DPT_TRIANGLEFAN, v, (uint32_t)n);
    }
    if (code == 0x74 || code == 0x7c) {           /* sprites: x, y at +8, w, h u16 at +0xc */
        shade(d, 1);
        tex_set(d, rd32(rec + (code == 0x74 ? 0x10 : 0x14)));
        blend_tex(d, semi);
        uint32_t c = color(rec, 4, semi);
        float us, uo, vs, vo;
        sprite_uv(d, &us, &uo, &vs, &vo);
        int32_t x = rdi16(rec + 8), y = rdi16(rec + 0xa);
        int32_t w = rdu16(rec + 0xc), h = rdu16(rec + 0xe);
        for (int k = 0; k < 4; k++) {
            v[k].sz = fbits(SZ);
            v[k].rhw = fbits(RHW);
            v[k].color = c;
        }
        v[0].sx = (float)x;       v[0].sy = (float)y;
        v[1].sx = (float)(x + w); v[1].sy = (float)y;
        v[2].sx = (float)(x + w); v[2].sy = (float)(y + h);
        v[3].sx = (float)x;       v[3].sy = (float)(y + h);
        if (code == 0x74) {
            /* the whole texture: 0 .. w / h texels from the record's size */
            v[0].tu = uo;
            v[0].tv = vo;
            v[1].tu = g3_st(g3_add(g3_mul((double)w, (double)us), (double)uo));
            v[1].tv = vo;
            v[2].tu = v[1].tu;
            v[2].tv = g3_st(g3_add(g3_mul((double)h, (double)vs), (double)vo));
            v[3].tu = uo;
            v[3].tv = v[2].tv;
        } else {
            /* u, v bytes at +0x10 (u0), +0x11 (v0), +0x12 (u1), +0x13 (v1) */
            v[0].tu = g3_st(g3_add(g3_mul((double)rec[0x10], (double)us), (double)uo));
            v[0].tv = g3_st(g3_add(g3_mul((double)rec[0x11], (double)vs), (double)vo));
            v[1].tu = g3_st(g3_add((double)uo, g3_mul((double)rec[0x12], (double)us)));
            v[1].tv = v[0].tv;
            v[2].tu = v[1].tu;
            v[2].tv = g3_st(g3_add((double)vo, g3_mul((double)rec[0x13], (double)vs)));
            v[3].tu = v[0].tu;
            v[3].tv = v[2].tv;
        }
        return draw(d, D3DPT_TRIANGLEFAN, v, 4);
    }
    if (code == 0x40 || code == 0xc0) {           /* LINE_F2 and the game's 0xc0: 2 points */
        shade(d, 1);
        pf2_d3d_set_blend(d, semi ? 0 : 3);
        tex_none(d);
        uint32_t c = color(rec, 4, semi);
        vpos(&v[0], rec, 8, c);
        vpos(&v[1], rec, 0xc, c);
        return draw(d, D3DPT_LINESTRIP, v, 2);
    }
    if (code == 0x50) {                           /* LINE_G2 */
        shade(d, 2);
        pf2_d3d_set_blend(d, semi ? 0 : 3);
        tex_none(d);
        vpos(&v[0], rec, 8, color(rec, 4, semi));
        vpos(&v[1], rec, 0x10, color(rec, 0xc, semi));
        return draw(d, D3DPT_LINESTRIP, v, 2);
    }
    return 0;
}

/* 0x004b6780 draw_ot(eax display (unused), edx ot): slots 0xfff down to 0 (ot + 4 * slot);
   each slot's chain is followed through the records' first dword until it is 0 or the address
   of the next lower slot (ot + 4 * (slot - 1)); every record -> draw_prim(0x00c84840, record).
   Returns edi as the caller left it (unset register; the caller ignores it). */
void pf2_d3d_draw_ot(Pf2D3d* d, uint32_t ot) {
    for (int32_t s = 0xfff; s >= 0; s--) {
        uint32_t lower = ot + 4u * (uint32_t)s - 4u;
        uint32_t p = rd32(d->mem(d->mem_user, ot + 4u * (uint32_t)s));
        while (p != 0 && p != lower) {
            pf2_d3d_draw_prim(d, d->mem(d->mem_user, p));
            p = rd32(d->mem(d->mem_user, p));
        }
    }
}
