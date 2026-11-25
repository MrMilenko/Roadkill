# The 3D path of TM2.EXE's software GPU (command 0xa0) and the low detail kernels

Addendum to docs/RENDERER.md (tiles, masks, edges, the integer paths). Everything here is from
the machine code and verified by emulator diffs. Port: `src/render/gpu3d*.c`.

## Command 0xa0: the perspective textured n-gon

Record (beyond +4 u16 flags, +6 u8 n, +7 code 0xa0 / 0xa2):

| Offset | | |
|---|---|---|
| +0x08 | u32 color | copied to 0x00c847c0, bytes to 0x00c847e4 (>> 16), 0x00c847f4 (>> 8), 0x00c847dc; not drawn by any 0xa0 kernel |
| +0x0c | u16 page | & 0xfff, the TPC table 0x00a30c48 (as the integer textured codes) |
| +0x0e | u16 light | entry of the light table 0x00c85c68 (0x78 bytes each): +0 float radius (squared into 0x00c84738), +4 float strength (copied as bits, 0x00c84744), +0xc float falloff (0x00c84718) |
| +0x10 | float z | 0x00c84674; fog constant 0x00c84670 = 4096.0 / z when 0x004e85f8 is set and flag 0x40 is clear, else 0 |
| +0x14 +0x30 +0x4c | u * w plane: d/dx, d/dy, value at the origin | |
| +0x18 +0x34 +0x50 | v * w plane (multiplied by 2^log2 width at setup) | |
| +0x2c +0x48 +0x64 | w (1/z) plane | |
| +0x1c +0x38 +0x54, +0x20 +0x3c +0x58, +0x24 +0x40 +0x5c | three light planes (flag 0x4000): the surface point relative to the light, times w | |
| +0x28 +0x44 +0x60 | a fourth plane (flag 0x80): set up, never read by any collect | |
| +0x68 / +0x6c | float clamp of 1/w (min, max) | |
| +0x70 / +0x74 | float origin x, y | edge units (1/32 pixel with the guard band): tile corner (col, row) is (col * 256, row * 256) |
| +0x78 | n x (i32 x, i32 y) | vertices in edge units; edges as 0xb0 / 0x90 (n = 0: slot -1 from +0x70 to +0x78) |

The planes are in edge units: the value at a point is `v0 + dx * (x - org_x) + dy * (y - org_y)`. The setup also keeps per tile steps (`* 256.0`, the double 0x004e35e4 / the float
0x004e35ec), for v times (256 << log2 width).

Flags (0x00c846d8): 1 textured (page setup; without it the previous page stays), 2 overlay
(see 0x004b1554), 0x40 no fog, 0x80 fourth plane, 0x4000 light. Collect:
`(flags & 0x40 && !(flags & 0x4080)) || flags & 2` -> 0x004b07ec; else
`g_special_render_mode (0x004e82cc) == 0 && !(flags & 0x4080)` -> 0x004b029c; else 0x004afadc.

### The collects

All three walk the rows like the textured collect 0x004af7e8 (rows with both span ends current,
clamped to the viewport's row range read at `vp base + 0x44 + 0x10 * row`, NOT at the vp_rows
pointer; other rows: cover reset over the viewport row, and 0x004afadc / 0x004b029c also zero
+0x28 / +0x2c). At each row start the three (six with light) planes are evaluated at the first
tile corner and stored as floats; they step by the per tile constants. For every tile with
draw = open & cover nonzero, the tile corner, its lower neighbour (always) and its right
neighbour (if its stamp is not current) get:

- `r = 1 / W` (double), stored as a float, clamped: `!(r_double >= min)` -> min, else
  `r_float > max` -> max (the two compares use different precisions);
- `u = fistp(U * r)`, `v = fistp(V * r)` (+0x00 / +0x04);
- 0x004b029c / 0x004afadc: fog `f = float(r * fogk + 4096.0)`, clamped to 8191.0 by comparing
  the float's bits as a signed int (`> 0x45fff800`; negative values and negative NaNs pass);
  0x004afadc with flag 0x4000: `t = (g0 r)^2 + (g1 r)^2 + (g2 r)^2`; when `!(t >= radius^2)`,
  `f = float(f - (1 - float(t) * falloff) * strength)`; `+0x34 = fistp(f)`, `+0x38 = page
  palette - 0x2000 + (f34 & 0x1f00) * 2` (a palette row per 256 fog units);
- 0x004b07ec: `+0x38 = page palette`.

The lower neighbour uses `W + w_ty`, `U + u_ty` ..., summed in double and not stored.
0x004afadc evaluates the row start of u, v and the light planes with x stored as a float first
(w uses the double x); the other two use the double x for all.

Lists: 0x004b029c lists every tile with draw nonzero. 0x004afadc and 0x004b07ec also put tiles
whose draw meets the overlay mask (+0x28 / +0x2c) into the overlay list 0x00be0b50 (count
0x00c847fc), take the overlay pixels out of draw (0x004b07ec: not for flag 2), and list a tile
only when draw is still nonzero. Then: 0x004afadc: overlay fill 0x004b10ec if any, then the
dither kernel 0x004b0cb4 if 0x004e85f4 is set, else the textured kernel; 0x004b029c: the
textured kernel; 0x004b07ec: for flag 2 with tiles listed, 0x004b1554 alone; else overlay fill,
then the textured kernel. Textured kernel: 0x004c8820 / 0x004c71f0 / 0x004c7c70 / 0x004c6590 by
0x004e85c4 (MMX) and 0x004e861c (low detail).

Dead branch: 0x004b0474 (`je` right after a `je` on the same flags) is never taken.

### Helpers (src/render/gpu3d_helpers.c)

- 0x004b0cb4 (dither kernel): textured kernel over the draw list that also interpolates the fog
  value per pixel (corner, right and lower neighbour +0x34, steps 0x00c846a4 / 0x00c846a8) and,
  with fog on and 0x004e85f4 set, takes the palette row from `(fog - noise16) & 0x1f00` (noise:
  the u16 table 0x00bdff50 from the tile's +0x30): dithered fog bands. Texel 0xff stays open.
- 0x004b10ec (overlay fill): over the overlay list, on pixels open & overlay & cover, writes
  `(palette[texel] & half mask) >> 1` (row mask 0x3f00 here), then updates cover, open and the
  overlay masks.
- 0x004b1554 (overlay mark, flag 2): ORs the primitive's drawn non see through pixels into the
  overlay mask +0x28 / +0x2c and resets cover; draws nothing. So a flag 2 primitive (water,
  glass) marks its pixels and whatever is drawn behind it later through those pixels comes out
  at half intensity.
- DECOMPILER TRAPs: with fog on the texel address ORs the masked u and v (adds without fog);
  0x00c847f0 / d4 / ec / d8 hold raw corner differences there (not * 8).

## Floats: how the port matches the x87 bit for bit

- **Control word**: the Watcom startup 0x004c15f8 / 0x004c15bd runs `fninit` and loads
  0x004e94dc = **0x127f**: precision control 53 bit, round to nearest even, all exceptions
  masked. The only other fldcw in the game is 0x004979a2 (frndint in chop mode, restoring the
  old word). The game creates no Direct3D device (imports: DirectDrawCreate, DirectSoundCreate,
  DirectInputCreateA), so nothing switches the FPU to 24 bit. The diffs load 0x127f before every
  primitive (the emulator starts with 0x037f); Unicorn 2.1.4 honours precision control (checked:
  1/3 gives a 53 bit significand at 0x127f / 0x027f, 24 at 0x007f).
- **Precision**: at 53 bit precision an x87 add / sub / mul / div is the IEEE double operation
  except for the exponent range. Every operand in these functions is a float, an int below 2^31
  or 1.0, at most four multiplications deep: magnitudes stay within [2^-596, 2^514] or are 0 /
  inf / NaN, inside the normal double range, so the double result equals the x87 register
  bit for bit (no double denormals, no overflow). The port computes in double in the original's
  order (stack traced instruction by instruction: which values stay in registers, which are
  stored as floats, x stored as float in 0x004afadc), compiled with `-ffp-contract=off`.
- **Stores**: `fst m32` = `(float)double` (round to nearest, gradual underflow; macOS arm64 has
  flush to zero off). fld / fstp copies make a signalling NaN quiet (done on the bits, as a
  compiler folds `(float)(double)f` to `f`).
- **fistp**: round to nearest even (the control word's rounding, `rint`); NaN or out of int32 ->
  0x80000000 (integer indefinite).
- **NaN**: invalid operations give the negative real indefinite (ARM gives a positive NaN);
  with NaN operands the larger significand wins, made quiet. It matters: the fog clamp
  compares bits as a signed int.
- **Compares**: fcomp + sahf: unordered makes `jae` fall through and `jbe` jump; the C
  conditions are written as `!(a >= b)` / `a > b`.
- Checked twice: diff_render3d_x87.py runs the model against the emulated x87 at 0x127f on
  200 000 random operations (floats, products, sums, int - float, reciprocals, zeros,
  denormals, infinities, quiet NaNs, fistp limits), and the n-gon diff compares every float
  global and every tile as bits.
- **Emulator difference**: Unicorn's `fld m32` keeps a signalling NaN signalling (a real x87
  makes it quiet: Intel SDM, FLD, #IA masked response). The port follows the hardware; the diffs
  give no signalling NaNs (game data has none).

## Low detail kernels (0x004e861c = 1; src/render/gpu3d_lowdetail.c)

0x004c7c70 (no MMX) is 0x004c8820 and 0x004c6590 (MMX) is 0x004c71f0 with one change: a row
whose 8 pixels are all drawn, with render flag 8 (see through texels) clear, at an even tile row
y (0, 2, 4, 6), is written to rows y and y + 1 from row y's texels and row y + 1 is skipped:
half vertical texture resolution and half the texel fetches in opaque, fully covered areas.
ORIGINAL BUG: the doubled row y + 1 is written whatever its own draw mask says, over pixels a
nearer primitive took, and one row past the primitive's lower edge (those pixels stay open).

## Port status

| Original | Port | Verified |
|---|---|---|
| 0x004ad874 code 0xa0 | gpu3d.c `gpu3d_dispatch` / `gpu3d_prim` | check_render3d_ngon.sh |
| 0x004afadc, 0x004b029c, 0x004b07ec | gpu3d.c `gpu3d_collect_light` / `_fog` / `_plain` | check_render3d_ngon.sh |
| 0x004b0cb4, 0x004b10ec, 0x004b1554 | gpu3d_helpers.c | check_render3d_helpers.sh, and in check_render3d_ngon.sh |
| 0x004c7c70, 0x004c6590 | gpu3d_lowdetail.c `gpu3d_lowdetail_fill` | check_render3d_lowdetail.sh, and in check_render3d_ngon.sh |
| 0x00496590 | gpu_flat.c `gpu_rgb_to_16` (not on the 0xa0 path; called by 0x004ad662 and game code) | check_render3d_rgb.sh (whole domain) |
| x87 model | gpu3d_float.h | check_render3d_x87.sh |

Hookups not done (existing files): gpu_dispatch.c case 0xa0 should call `gpu3d_prim(g, g3d,
lights, p)` (it needs a `Gpu3d*` and the light table: e.g. two new TM2Gpu fields), and
gpu_tex.c's `gpu_tex_fill` should start with `if (g->r.tex_mode != 0) { gpu3d_lowdetail_fill(g);
return; }`. Until then `gpu3d_dispatch_all` (gpu3d.c) is the dispatcher with both.
