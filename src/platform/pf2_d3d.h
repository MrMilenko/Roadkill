/*
 * pf2_d3d.h: TM2.EXE's hardware 3D (Direct3D 5) draw path, ported over a backend instead of COM
 * (docs/PLATFORM.md, Part B). The IDirect3DDevice2 calls go through Pf2D3dBackend in the same
 * order and with the same arguments as the original.
 *
 * Ported (emulator diff: tools/emu/check_pf2_d3d.sh):
 *   0x004b611c pf2_d3d_set_filter   TEXTUREMAG / TEXTUREMIN LINEAR or NEAREST, cached
 *   0x004b617c pf2_d3d_set_blend    PS1 semi transparency mode -> ALPHABLENDENABLE / SRC / DESTBLEND
 *   0x004b67d0 pf2_d3d_draw_prim    one GPU record -> SetRenderState(s) + DrawPrimitive
 *   0x004b6780 pf2_d3d_draw_ot      walks a 0x1000 slot ordering table into draw_prim
 *
 * State the functions touch (game addresses):
 *   display object 0x00c84840 +0x28: IDirect3DDevice2* (here Pf2D3d.device)
 *   0x004e86b0 u32: index of the texture last set on the device, 0x7fff = none (tex_cache)
 *   0x00c84f74..0x00c84fdc: render state cache (rs[27]): rs[0] 0x00c84f74 SHADEMODE,
 *     rs[13] 0x00c84fa8 ALPHABLENDENABLE, rs[14] 0x00c84fac texture filter on,
 *     rs[25] 0x00c84fd8 SRCBLEND, rs[26] 0x00c84fdc DESTBLEND
 *   texture tables (one memory block from 0x00a2dc48, tex below): for an index i (record & 0xfff)
 *     0x00a2dc48 + 24 i u32 TEXTUREHANDLE, 0x00a2dc4c + 24 i i32 color key count,
 *     0x00a30c4c + 48 i u32 width, 0x00a30c50 + 48 i u32 height, 0x00a30c58 + 48 i color keyed,
 *     0x00a30c74 + 48 i nonzero = no filtering. The two tables overlap for large indices; the
 *     original reads them as one block, so does the port (tex covers 0x00a2dc48..0x00a60c4c).
 */
#ifndef PF2_D3D_H
#define PF2_D3D_H

#include <stdint.h>

#define PF2_TEX_BASE 0x00a2dc48u
#define PF2_TEX_SIZE (0x00a30c4cu + 48u * 0x1000u - PF2_TEX_BASE)

/* D3DTLVERTEX, 32 bytes */
typedef struct {
    float sx, sy, sz, rhw;
    uint32_t color, specular;
    float tu, tv;
} Pf2TlVertex;

typedef struct Pf2Device Pf2Device;   /* opaque: IDirect3DDevice2* */

typedef struct {
    void* user;
    /* IDirect3DDevice2::SetRenderState (vtable 0x5c) */
    int32_t (*set_render_state)(void* user, Pf2Device* dev, uint32_t state, uint32_t value);
    /* IDirect3DDevice2::DrawPrimitive (vtable 0x74)(type, vertex type, vertices, count, flags).
       The vertices are 32 byte D3DTLVERTEX; for the records built by the wrapper the specular
       field is not set by the original (stack garbage), the port passes 0 there. */
    int32_t (*draw_primitive)(void* user, Pf2Device* dev, uint32_t type, uint32_t vtype, const void* verts,
                              uint32_t count, uint32_t flags);
} Pf2D3dBackend;

typedef struct {
    Pf2Device* device;          /* display +0x28 */
    uint32_t* tex_cache;        /* 0x004e86b0 */
    uint32_t* rs;               /* 0x00c84f74, 27 dwords */
    const uint8_t* tex;         /* game memory 0x00a2dc48 .. +PF2_TEX_SIZE */
    /* game address -> host bytes, for draw_ot's ordering table and records */
    uint8_t* (*mem)(void* user, uint32_t addr);
    void* mem_user;
    const Pf2D3dBackend* be;
} Pf2D3d;

int32_t pf2_d3d_set_filter(Pf2D3d* d, uint32_t on);
int32_t pf2_d3d_set_blend(Pf2D3d* d, uint32_t mode);
int32_t pf2_d3d_draw_prim(Pf2D3d* d, const uint8_t* rec);
void pf2_d3d_draw_ot(Pf2D3d* d, uint32_t ot);

#endif /* PF2_D3D_H */
