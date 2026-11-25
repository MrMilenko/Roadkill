# The renderer of TM2.EXE: a software PlayStation GPU

The PC build kept the PS1 drawing model: game code builds PS1 style GPU primitives and passes
them to 0x004967f4 (the PC's AddPrim). Outside split screen that locks the surface (0x004961b8:
pointer 0x00a04674, pitch 0x00a04670), rasterizes the primitive at once with `command_dispatch`
0x004ad874 and unlocks (0x004961d4); in split screen it queues the primitive (0x004963b4) and
0x0049681c flushes the queue later (0x1000 ordering table buckets, each a linked list,
dispatching every primitive between one lock / unlock).

Everything below is from the machine code. Port: `src/render/` (see "Port status").

## How it rasterizes: tiles and coverage masks, front to back

Not a span renderer. The screen is a grid of 8x8 pixel **tiles**, 0x54 columns x 0x40 rows,
0x40 bytes each at 0x00be6150 (`GpuTile` in src/render/gpu.h). Tiles 0..1 of every row and
column are a guard band: tile (2, 2) is pixel (0, 0); 80 x 60 tiles get a screen offset
(+0x24) from the pitch setup 0x004ad760, so at most 640 x 480 pixels are addressable.

Every tile has three 64 bit pixel masks (byte = pixel row, bit n = pixel column n):

| Tile field | Mask | Life |
|---|---|---|
| +0x08 / +0x0c | **open**: pixels nobody drew this frame | set by the frame clear 0x004ad084 (cut to the screen size on the last column / row) |
| +0x10 / +0x14 | **cover**: inside the current primitive | ~0 when idle; each edge ANDs in its half plane; reset by the collect |
| +0x18 / +0x1c | **draw** = open & cover | written by the collect, read by the span kernel; open &= ~cover |

So each frame is drawn front to back (the game's ordering table is walked nearest first) and
every pixel is written at most once; a back facing polygon gets its row ends swapped and
covers nothing, which culls it.

A primitive goes through four stages:

1. **Vertices** to 1/32 pixel with the guard band: `v * 32 + 0x200` (so `v >> 8` is the tile).
   0xb0 / 0x90 / 0xa0 records carry vertices already in that unit.
2. **Edge setup** 0x004af164 per edge into a 0x40 byte edge slot (0x00be5f50 + 0x40 * i, nine
   slots incl. one before at 0x00be5f10): octant, 16.16 slope (0x004af118), tile steps, end tile,
   an error term whose bits 19.. pick the sub tile position.
3. **Edge walk** 0x004c6000: steps the edge tile by tile (a DDA in tile units). For every tile:
   if the selected viewport enables it (byte map), AND the tile's cover with a precomputed half
   plane mask `mask[(err >> 19) | octant << 12 | (slope >> 10 & 63) << 6]` (0x8000 x 8 bytes at
   0x00c3f960, built at init by 0x004b1b90); set the hit flag 0x00c84704; and extend the row's
   span (0x00c3f550, 0x10 per tile row: leftmost tile from upward edges, rightmost from downward
   edges, each with the primitive stamp 0x004e85d4 that marks them as current). Rows touched go
   into 0x00c847c4 / 0x00c847f8 (clamped to the viewport).
4. **Collect + kernel**: for each row between them whose two ends are current, every tile
   between the ends (clamped to the viewport row) moves cover -> draw and is listed in the draw
   list (0x00c3a150, tile pointers, count 0x00c847cc); then a span kernel fills the draw masks.

## command_dispatch 0x004ad874

Per call: clears 0x00c847cc, 0x00c847fc, 0x00c84704; increments the stamp 0x004e85d4; code =
byte +7 & 0xfd (bit 1 = PS1 semi transparency, ignored here). Records are PS1 like but not
identical (the PC moved several fields):

| Code | Primitive | Record (beyond +4..+6 color, +7 code) | Path |
|---|---|---|---|
| 0x20 | flat triangle | i16 x, y at +8, +0xc, +0x10 | edges 0-1, 1-2, 2-0; color 0x0049653c(r<<16 \| g<<8 \| b); flat collect 0x004af694 |
| 0x28 | flat quad | +8, +0xc, +0x10, +0x14 | edges 0-1, 1-3, 3-2, 2-0 (PS1 quad order) |
| 0xb0 | flat n-gon | +4 u16 screen color, +6 u8 n, +8 n x (i32 x, i32 y) in edge units | edges i -> i + 1, closing edge n-1 -> 0 |
| 0x24 | textured triangle | xy +8 / +0x10 / +0x18, u8 uv +0xc / +0x14 / +0x1c, +0x20 page | page from the TPC table 0x00a30c48 (0x30 per page), affine u / v gradients by integer division; textured collect 0x004af7e8 |
| 0x2c | textured quad | xy +8 / +0x10 / +0x18 / +0x20, uv +0xc / +0x14 / +0x1c / +0x24, +0x28 page | ORIGINAL QUIRK: drawn as the triangle 0, 1, 3 (gradients and edges); vertex 2 at +0x18 is never read |
| 0x74 | sprite, texel = pixel | +8 x, +0xa y, +0xc u16 w, +0xe u16 h, +0x10 page | u = v = 0 at the corner, step 1 |
| 0x7c | scaled sprite | +8 x, +0xa y, +0xc w, +0xe h, +0x10 u0, +0x11 v0, +0x12 u1, +0x13 v1, +0x14 page | du = (u1 - u0) * 0x1000 / w, dv likewise; the font's 24 byte sprite record (src/game/font.h) is this layout |
| 0x90 | textured n-gon, gradients given | +4 u16 flags (render flags), +6 n, +8 i32 palette row, +0xc / +0x14 / +0x1c i32 u and +0x10 / +0x18 / +0x20 i32 v (.12) at three points +0x24 / +0x2c / +0x34 (i32 x, y in edge units), +0x44 page, +0x48 n x (i32 x, i32 y) | edges first, texture setup only when something was hit; integer gradients from the three points (shifted >> 5 to pixels) |
| 0xa0 | 3D n-gon (float) | +4 u16 flags, +6 n, +8 color, +0xc page, +0xe light index, +0x10 float z, +0x14..+0x74 float gradients, +0x78 n x (i32 x, i32 y) | see below |
| 0x40 | line | +8 x0, y0, +0xc x1, y1 (i16) | 0x004b17bc: Bresenham, clipped to the viewport rectangle, only open pixels, which it then takes (0x004ad030 / 0x004acfe0) |
| 0x50 | line (PS1 gouraud line) | +8 x0, y0, +0x10 x1, y1 | same, ORIGINAL QUIRK: always color 0x00fe |
| 0xc0 | line | as 0x40 | 0x004b1934: no clipping, no masks (debug) |
| other | | | nothing |

### Flat path (ported, verified)

Color: 0x0049653c turns 0x00RRGGBB into the screen format (0x00a2dba5: 0 = 555, 1 = 565,
else 0): red on top. The flat color is kept as 16 bit (0x00c8480e), 32 bit (0x00c846c8) and
64 bit (0x00c84668 / 0x00c8466c). Kernel: 0x004c6400 (32 bit stores) or 0x004c6280 (MMX, 64
bit stores, ends in emms) by 0x004e85c4 (CPUID MMX bit at init); same pixels.

### Textured paths (ported, verified)

Setup shared by 0x24 / 0x2c / 0x74 / 0x7c / 0x90 (and 0xa0 with flag bit 0): page p = word &
0xfff, TPC entry 0x00a30c48 + 0x30 * p: +0 pixels -> 0x00c846f8, +0xc palette -> 0x00c846bc
(0x90 / 0xa0: + 2 * the record's +8 when 0x004e85f8 is set and flag 0x40 is clear: a palette
row, fog or shading by palette), +4 width -> 0x00c8474c, +8 height -> 0x00c84748. Texture
shift 0x00c84724 = log2(width) (8 when 0), then 12 - it; u mask 0x00c846ec = width - 1, v mask
0x00c846e8 = (height - 1) << shift, both << 12 into 0x00c846c0 / 0x00c846b0. Gradients
0x00c84684 du/dx, 0x00c84680 du/dy, 0x00c84678 dv/dx, 0x00c8467c dv/dy (.12 fixed, v pre
shifted by the texture shift; all 0 when the determinant is 0), times 8 per tile in 0x00c847f0,
0x00c847ec, 0x00c847d4, 0x00c847d8; origin 0x00c8471c / 0x00c84728 (pixel), start 0x00c8473c /
0x00c8472c. 0x24 / 0x2c / 0x74 / 0x7c set 0x00c846d8 (render flags) = 9.

Textured collect 0x004af7e8: for each row with both ends current (clamped to the viewport row),
tile corner u = du/dy * (row * 8 - org_y) + du/dx * (col * 8 - org_x) + u0 (v likewise), then
+du8x / +dv8x per tile. Listed tiles (draw = open & cover nonzero) get u, v, stamp (+0x20) and
palette (+0x38), and so do their right and lower neighbours (u + du8x / du8y ...), so the kernel
can interpolate inside the tile. Unlisted tiles get cover reset; rows without both ends reset
cover and zero +0x28 / +0x2c. Open is not touched here: the kernel does it.

Kernel by 0x004e85c4 (MMX) and 0x004e861c (set by the pause menu's detail options through
0x004ad21c): 0x004c8820 (plain), 0x004c71f0 (MMX), 0x004c7c70 (flag), 0x004c6590 (MMX, flag),
hand written assembly of 0x600..0xacc bytes each. Per tile the kernel interpolates u, v linearly
from the three corners (step (right - corner) >> 3 per pixel, (lower - corner) >> 3 per row),
fetches texel index pixels[((u & umask12) + (v & vmask12)) >> 12] and writes palette[index].
**Texture transparency** (render flag 8, set by 0x24 0x2c 0x74 0x7c; 0x90 from its record):
see through pixels are not written and stay open (open &= ~cover | see through), so a farther
primitive can fill them later. What is see through depends on the kernel:
- plain 0x004c8820: index 0xff in rows whose 8 pixels are all drawn, but palette color 0x0000
  in partly covered rows (two different rules in one kernel);
- MMX 0x004c71f0 (the one a Pentium MMX or later runs): index 0xff everywhere.
- ORIGINAL BUG (both): a see through pixel alone in its pixel pair also gives back its
  neighbour, which the primitive did not draw and which may hold an earlier primitive's pixel
  (plain: either side; MMX: only when it is the right pixel).
The two flag kernels (0x004e861c = 1) differ from both in some pixels (by the emulator diff);
not ported. By reading 0x004c7c70 they are a low detail mode: in fully covered rows the texels
of one row are written to two rows (each texel pair stored at the row and the one below,
alternate rows by a bit of the row counter), i.e. half vertical texture resolution.

### 3D path 0xa0 (mapped, not ported) and its floats

Flags +4 (copied to 0x00c846d8): bit 0 textured, 0x4000 gouraud (three float colors per
gradient, times 256.0 = 0x004e35ec, plus a light entry 0x00c85c68 + 0x78 * (+0xe): float
squared into 0x00c84738, +0x0c, +0x00 more), 0x80 a fourth interpolant (+0x28 / +0x44 / +0x60,
times 256.0 from the double 0x004e35e4), 0x40 no fog, 0x02 picks the third kernel.
Fog: 0x00c84670 = 4096.0 (0x004e35e0) / float +0x10 when 0x004e85f8 is set and 0x40 is clear,
else 0.0. Texture gradients: floats +0x14, +0x2c, +0x30, +0x48, +0x18, +0x34, +0x50, +0x4c
times 256.0 and times (256.0 << texture shift); the raw floats are kept as well. Collect /
kernel: 0x004b07ec when (flags & 0x40) and not (flags & 0x4080), or flags & 2; else
0x004b029c when 0x00c84ef0-ish `g_special_render_mode` is 0 and not (flags & 0x4080); else
0x004afadc. Those collect functions are float too (x87) and call 0x004b0cb4, 0x004b10ec,
0x004b1554 and the textured kernels.

Other textured findings: the page index is 12 bits (word & 0xfff) but the table has 512
entries; a page of width 0 gets texture shift 8 and u mask 0xfffff000 (any u is an index); 0x7c
with width or height 0 divides by zero (a CPU fault in the original).

**Floats in the renderer**: the 0xa0 setup (fmul by 256.0, the 4096.0 / z divide, the light
term squared), the three 0xa0 collect functions, and the mask table builder 0x004b1b90 at init
(doubles, truncation by 0x004979a2 = frndint in chop mode). The flat and integer textured paths
(0x20 0x28 0xb0 0x24 0x2c 0x74 0x7c 0x90, lines) have no floating point. The mask table port
uses plain doubles: over all 64 x 64 x 8 inputs the truncated value is never within 6.8e-5 of
an integer, so 53 and 64 bit precision give the same table (and the emulator diff checks all of
it).

## State

| Address | What | Port |
|---|---|---|
| 0x00be5f10 | 9 edge slots x 0x40 (slot -1 used by an n-gon with n = 0) | `edge` |
| 0x00be6150 | 0x1500 tiles x 0x40 | `tile` |
| 0x00c3a150 | draw list, tile pointers, up to 0x1500 | `list` (indices) |
| 0x00c3f550 | row spans, 0x40 x {lo, hi, lo stamp, hi stamp} | `span` |
| 0x00c3f960 | edge masks 0x8000 x 8 (index: bits 0..5 error, 6..11 slope, 12..14 octant) | `mask` |
| 0x00c7f960 | row table row * 0x54 | `row_start` |
| 0x00c7fa60 | 3 viewports x 0x1958 (0, 1 split screen, 2 whole screen) | `vp` |
| 0x00bdff50 / 0x00bdfd50 | noise from rand() / zeros (init) | `noise_a` / `noise_b` |
| 0x00c846e4 / 0x00c846e0 / 0x00c847e8 | selected viewport's enable map / row ranges / base | `r.vp_*` (offsets) |
| 0x004e85c8, cc, d0, dc, e0 | constant pointers to the tiles, spans, list, row table, masks | (arrays) |
| 0x004e85d4 | primitive stamp | `r.stamp` |
| 0x00c847c4 / 0x00c847f8 | first / last tile row touched | `r.row_min` / `r.row_max` |
| 0x00c84704 | an edge touched an enabled tile | `r.hit` |
| 0x00c846a0.. | pitch derived values (0x004ad760) | `r.pitch_*` |
| 0x00c846d8, 0x00c846f8.., 0x00c84678.. | render flags, texture page, masks, gradients, origin (textured) | `r.render_flags`, `r.tex_*`, `r.du_dx` ... |
| 0x004e85f8 | 0x90 / 0xa0 palette rows and fog on | `r.fog` |
| 0x00c8480c | half mask 0x7bde / 0xf7de (semi transparency, set per frame by 0x004ad848) | `r.half_mask` |
| 0x00a2dba5 | pixel format 0 555, 1 565, 2 other | `r.format` |

Viewport (0x1958 bytes): +0 / +4 screen size; +8 / +0xc tile rows; +0x10 / +0x14 tile
columns ((p * 32 >> 8) + 2); +0x18 columns; +0x1c / +0x20 first / last tile; +0x24..+0x30 pixel
rectangle; +0x34..+0x40 the same in edge units; +0x44 0x40 row ranges {row * 0x54 + col1 + 1,
row * 0x54 + col0 - 1, -, -}; +0x454 0x1500 enable bytes. Setup 0x004ad3d4(i, w, h, x0, x1, y0,
y1) also rebuilds viewport 2 as the whole w x h screen; select 0x004ad5e0(i).

Frame: 0x004ad848 frame begin (half mask, frame clear 0x004ad084 over viewport 2's tiles,
toggles 0x004e85ec). Init 0x004ad360 (from 0x0049628c, the display setup): CPUID MMX test into
0x004e85c0 / 0x004e85c4, masks 0x004b1b90, noise 0x004b2004 (five rand() per entry, the first
thrown away; tile +0x30 = (3 * row + 7 * col) & 0xfc), row table, cover / stamp reset 0x004ad19c,
0x00c847e8 = viewport 1.

## Original quirks and bugs (kept by the port)

- **Viewport select offsets** (0x004ad5e0): the enable map pointer is base + 0x454 + i and the
  row range pointer base + 0x44 + 0x10 * i, so viewport 1 (split screen player 2) tests each
  tile against its right neighbour's enable byte (its drawable area is one tile left of its
  rectangle) and each row against the next row's range; viewport 2 two over. Ghidra prints this
  as the multipliers 0x1959 / 0x1968 (DECOMPILER TRAP: looks like a typo, it is real).
- **Edge octant from bit 15** (0x004af164): signs are taken with `>> 14 & 2`, `>> 15 & 1`,
  `>> 13 & 4`, i.e. bit 15, so coordinate differences of 0x8000 edge units (1024 pixels) or more
  get the wrong octant.
- **Row range test in the edge walk** (0x004c6000) compares the loop test value
  `(row1 - row) | (count - k)` against the row's tile range instead of the tile: it practically
  never rejects; the enable map does the clipping.
- **Flat collect row drift** (0x004af694): on a row whose ends are not both current, the span
  and viewport row pointers advance two rows while the row counter advances one. Only degenerate
  primitives have such rows (n-gons with one or two vertices, zero length edges). The textured
  collect does not have it.
- **0x50 lines** are always drawn in 0x00fe.
- **n-gon limits**: n > 8 (0xb0 / 0x90 / 0xa0) writes edge slots into the tile grid (the port
  caps n at 8); n = 0 sets up one edge in slot -1 from the record's header bytes and walks none.
- Coordinates outside tile rows 0..63 / columns 0..83 make the edge walk read and write past
  its tables (row spans, enable map, row table). The port keeps every access inside its own
  arrays instead (reads 0, drops writes); the diffs stay inside the tables.

## Port status (src/render)

| Original | Port | Verified |
|---|---|---|
| 0x004ad360, 0x004b1b90, 0x004b1a6c, 0x004b1aec, 0x004b2004, 0x004ad19c | gpu_init.c `gpu_init`, `gpu_build_masks` | tools/emu/check_render_init.sh |
| 0x004ad760, 0x004ad3d4 (+0x004ad1c4, 0x004ad714), 0x004ad5e0, 0x004ad084, 0x004ad848 | gpu_init.c | check_render_flat.sh |
| 0x004af118, 0x004af164, 0x004c6000 | gpu_edge.c | check_render_flat.sh |
| 0x0049653c, 0x00496590, 0x004af694, 0x004c6400, 0x004c6280, 0x004ad030, 0x004acfe0, 0x004b17bc, 0x004b1934 | gpu_flat.c | check_render_flat.sh (0x00496590 not called by these paths: by reading) |
| 0x004ad874 codes 0x20 0x28 0xb0 0x40 0x50 0xc0, unknown codes | gpu_dispatch.c | check_render_flat.sh |
| 0x004ad874 codes 0x24 0x2c 0x74 0x7c 0x90, 0x004af7e8, kernels 0x004c8820 and 0x004c71f0 | gpu_tex.c | check_render_tex.sh (`--kernel mmx` for 0x004c71f0) |
| kernels 0x004c7c70, 0x004c6590 (0x004e861c = 1) | not ported (the port draws as 0x004c8820 / 0x004c71f0) | differ, see above |
| 0x004ad874 code 0xa0, 0x004afadc, 0x004b029c, 0x004b07ec and their helpers / kernels | not ported (gpu_dispatch returns -1) | |


## Update (2026-11-21, later)

The 3D float n-gon (code 0xa0), its collects and helpers, the low detail kernels and 0x00496590 are
ported and verified: see docs/RENDERER_3D.md. gpu_dispatch now calls gpu3d_prim for 0xa0 when
the 3D state (TM2Gpu.g3d) is set up, and gpu_tex_fill routes low detail mode (tex_mode != 0) to
the low detail kernels.
