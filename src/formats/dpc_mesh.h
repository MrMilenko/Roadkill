/*
 * dpc_mesh.h: mesh nodes (type 0) inside a relocated DPC, as the polygon renderer 0x0048e9b8 reads
 * them. Layout is from reading that function; tools/emu/check_dpc_mesh.sh checks it against every
 * mesh on the disc (record sizes, index ranges, normals, texture pages). Not an emulator diff: the
 * renderer is float and DirectDraw bound.
 *
 * Mesh node:
 *   +0x00 u8 type 0, +0x01 u8 0xFE once relocated, +0x02 u16 id (699 animated water UVs, 700 drawn
 *   only in a special render mode)
 *   +0x04 ptr vertices, 8 bytes each: i16 x, y, z, pad. The node does not store a vertex count
 *   +0x08 ptr read by the renderer (0x009f7d3c), meaning not traced; can point outside the file
 *   +0x0C ptr first polygon record, +0x10 u32 polygon count
 *   +0x18 u32 bits 19..30: fixed depth when (+0x1A & 0x7FF8) != 0
 *
 * Polygon record (size in the record, the renderer steps by +0x02 * 4):
 *   +0x00 u8  bits 0..6: UV word count (0 = untextured)
 *   +0x01 u8  shading: < 2 flat, else per vertex colors
 *   +0x02 u8  record size in 32 bit words
 *   +0x04 i16 vertex index 0..3
 *   +0x0C i16 normal x, +0x0E normal y, +0x10 normal z (4096 = 1.0); z == 0x2000 means "no
 *             normal, cull by screen winding"
 *   +0x10 u32 bits 16..18 vertex count (0 sprite, 1 beam, 3 triangle, 4 quad), bits 28..30 color
 *             word count
 *   +0x14 u32 bits 1..6 fog distance (0x7E in bits 0..6 = never fogged), bits 7..12 signed value
 *   +0x18     colors: u32 0x??RRGGBB each (first one is the flat color)
 *             then UV words: u16 u, u16 v (texels) per vertex, 4 for sprites and beams
 *             then u16 texture page: bits 0..8 page index, bits 12..13 blend mode.
 *             Sprites and beams always load 4 UV words and take the page index after them, but
 *             the blend mode after the record's UV count; 6 beams in ROOF.DPC have 3 UVs, so
 *             their page index comes from the next record (original behavior, kept)
 *   Some records carry 1 to 3 more words after that; the renderer never reads them.
 * Sprite (count 0): centered on vertex 0, i16 width at +0x06, i16 height at +0x08.
 * Beam (count 1): vertex 0 extended by +/- the i16 at +0x08 along z.
 */
#ifndef TM2_DPC_MESH_H
#define TM2_DPC_MESH_H

#include <stddef.h>
#include <stdint.h>

#define DPC_POLY_SPRITE 0
#define DPC_POLY_BEAM 1

typedef struct {
    uint8_t vcount;        /* 0 sprite, 1 beam, 3, 4 */
    uint8_t shading;       /* record +1 */
    uint8_t textured;
    uint8_t ncolors;
    int16_t index[4];
    int16_t normal[3];
    uint8_t cull_by_winding;
    uint8_t fog;           /* bits 1..6 of +0x14, 0x3F = never */
    int16_t size_w, size_h; /* sprite / beam sizes */
    uint32_t color[4];
    uint8_t nuv;
    uint16_t u[4], v[4];
    uint16_t page;         /* texture page index (bits 0..8 of the page word) */
    uint8_t blend;         /* bits 12..13 of the page word */
    uint8_t words;         /* record size in words */
} DpcPoly;

typedef struct {
    uint32_t node;          /* buffer offset of the node */
    uint16_t id;
    uint32_t vertices;      /* buffer offsets */
    uint32_t aux;           /* +0x08, see above; may be outside the buffer */
    uint32_t polys;
    uint32_t poly_count;
} DpcMesh;

/* Decodes one polygon record at buffer offset off. Returns 0, or -1 if it does not fit or is
 * malformed (sizes that do not cover the fields). */
int dpc_poly_decode(const uint8_t* buf, size_t size, size_t off, DpcPoly* out);

/* Walks the node tree of a buffer already processed by dpc_relocate(buf, size, addr_base, ...) and
 * calls fn for each mesh node once. Returns the mesh count, or -1 on a bad pointer. */
typedef void (*DpcMeshFn)(const uint8_t* buf, const DpcMesh* mesh, void* user);
int dpc_for_each_mesh(const uint8_t* buf, size_t size, uint32_t addr_base, DpcMeshFn fn, void* user);

#endif /* TM2_DPC_MESH_H */
