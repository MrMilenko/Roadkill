/*
 * dpc_mesh.c: mesh node walk and polygon record decoding (see dpc_mesh.h).
 */
#include "dpc_mesh.h"

#include "dpc.h"

#include <stdlib.h>
#include <string.h>

static uint32_t rd32(const uint8_t* b, size_t o) { return b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24; }
static uint16_t rd16(const uint8_t* b, size_t o) { return (uint16_t)(b[o] | b[o + 1] << 8); }

int dpc_poly_decode(const uint8_t* b, size_t size, size_t o, DpcPoly* p) {
    if (o + 0x18 > size) return -1;
    memset(p, 0, sizeof *p);
    uint32_t w10 = rd32(b, o + 0x10);
    p->words = b[o + 2];
    p->shading = b[o + 1];
    p->nuv = b[o] & 0x7f;
    p->textured = p->nuv != 0;
    p->vcount = (uint8_t)((w10 & 0x7ffff) >> 16);
    p->ncolors = (uint8_t)((w10 & 0x7fffffff) >> 28);
    size_t need = 0x18 + 4u * p->ncolors + 4u * p->nuv + (p->textured ? 2u : 0u);
    if ((size_t)p->words * 4 < need || o + (size_t)p->words * 4 > size) return -1;
    if (p->nuv > 4 || p->ncolors > 4 || p->vcount == 2 || p->vcount > 4) return -1;

    for (int i = 0; i < 4; i++) p->index[i] = (int16_t)rd16(b, o + 4 + 2 * i);
    p->normal[0] = (int16_t)rd16(b, o + 0xc);
    p->normal[1] = (int16_t)rd16(b, o + 0xe);
    p->normal[2] = (int16_t)rd16(b, o + 0x10);
    p->cull_by_winding = p->normal[2] == 0x2000;
    p->fog = (uint8_t)((rd32(b, o + 0x14) & 0x7f) >> 1);
    p->size_w = (int16_t)rd16(b, o + 6);
    p->size_h = (int16_t)rd16(b, o + 8);
    size_t c = o + 0x18;
    for (int i = 0; i < p->ncolors; i++, c += 4) p->color[i] = rd32(b, c);
    /* the renderer loads vcount UV words for polygons and always 4 for sprites and beams; the page
     * index comes from the word after those, the blend mode from the word after the record's own
     * UV count (they differ only for the few beams with 3 UVs, where the page index is read from
     * the next record) */
    int nload = p->vcount >= 3 ? p->vcount : 4;
    for (int i = 0; i < p->nuv && i < nload; i++) { p->u[i] = rd16(b, c + 4 * i); p->v[i] = rd16(b, c + 4 * i + 2); }
    if (p->textured) {
        size_t pw = c + 4u * (size_t)nload;
        p->blend = (uint8_t)((rd16(b, c + 4u * p->nuv) >> 12) & 3);
        p->page = pw + 2 <= size ? (uint16_t)(rd16(b, pw) & 0x1ff) : 0;
    }
    return 0;
}

/* children of a relocated node, same table as the relocation walk in dpc.c */
static int node_children(const uint8_t* b, size_t size, size_t o, uint32_t ab, uint32_t* out, int cap) {
    int n = 0;
#define KID(off) do { if (n < cap && o + (off) + 4 <= size) out[n++] = rd32(b, o + (off)); } while (0)
    switch (b[o]) {
    case 1: for (int i = 0; i < b[o + 0x14]; i++) KID(0x18 + 4 * i); break;
    case 2:
        for (uint32_t i = 0; i < rd32(b, o + 0x10) && n < cap; i++) {
            uint32_t e = rd32(b, o + 0x14 + 4 * i) - ab;
            if ((size_t)e + 12 > size) return -1;
            out[n++] = rd32(b, e + 8);
        }
        break;
    case 3: for (int i = 0; i < b[o + 0xb]; i++) KID(0x20 + 4 * i); break;
    case 4: for (int i = 0; i < b[o + 0x12]; i++) KID(0x14 + 4 * i); break;
    case 5: for (int i = 0; i < b[o + 0x36]; i++) KID(0x38 + 4 * i); break;
    case 9: for (int i = 0; i < b[o + 6]; i++) KID(0x10 + 4 * i); break;
    case 0xb: KID(8); KID(0xc); break;
    case 0xc: for (int i = 0; i < rd16(b, o + 6); i++) KID(8 + 4 * i); break;
    case 0xf: for (int i = 0; i < b[o + 0xa]; i++) KID(0xc + 4 * i); break;
    default: break;
    }
#undef KID
    return n;
}

int dpc_for_each_mesh(const uint8_t* b, size_t size, uint32_t ab, DpcMeshFn fn, void* user) {
    DpcHeader h;
    if (dpc_read_header(b, size, &h) != 0) return -1;
    size_t list = dpc_reloc(h.root_list, h.base, ab) - ab;
    if (list + 4 > size) return -1;
    uint8_t* seen = calloc(size / 4 + 1, 1);
    size_t cap = 1024, top = 0;
    uint32_t* stack = malloc(cap * sizeof *stack);
    if (!seen || !stack) { free(seen); free(stack); return -1; }
    int meshes = 0, rc = 0;
    for (uint32_t i = 0; i < rd32(b, list) && list + 8 + 4 * (size_t)i <= size; i++) stack[top++] = rd32(b, list + 4 + 4 * i);
    while (top && rc == 0) {
        uint32_t v = stack[--top];
        if (v == 0) continue;
        size_t o = (size_t)(v - ab);
        if (o + 0x14 > size || (o & 3)) { rc = -1; break; }
        if (seen[o / 4]) continue;
        seen[o / 4] = 1;
        if (b[o] == 0) {
            DpcMesh m = { (uint32_t)o, rd16(b, o + 2), rd32(b, o + 4) - ab, rd32(b, o + 8) - ab,
                          rd32(b, o + 0xc) - ab, rd32(b, o + 0x10) };
            fn(b, &m, user);
            meshes++;
        }
        uint32_t kids[256];
        int n = node_children(b, size, o, ab, kids, 256);
        if (n < 0) { rc = -1; break; }
        for (int i = 0; i < n; i++) {
            if (top == cap) {
                uint32_t* ns = realloc(stack, cap * 2 * sizeof *ns);
                if (!ns) { rc = -1; break; }
                stack = ns; cap *= 2;
            }
            stack[top++] = kids[i];
        }
    }
    free(seen);
    free(stack);
    return rc ? rc : meshes;
}
