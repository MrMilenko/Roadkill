/*
 * gpu.h: TM2.EXE's software PlayStation GPU (the PC renderer), ported from the machine code.
 * See docs/RENDERER.md for the map of the whole renderer; this header documents the state.
 *
 * How it draws: the screen is a grid of 8x8 pixel tiles, 0x54 columns by 0x40 rows, with a guard
 * band of two tiles on the top and left (tile (2, 2) is pixel (0, 0)). Every tile keeps three
 * 64 bit pixel masks (one byte per pixel row, bit n = pixel column n):
 *   open:  pixels not drawn yet this frame (reset by gpu_frame_clear),
 *   cover: pixels inside the primitive being drawn (each edge ANDs in its half plane),
 *   draw:  open & cover, what the span kernels fill; the primitive then takes them out of open.
 * So a frame is drawn front to back and every pixel is written at most once.
 * Primitive vertices are converted to 1/32 pixel units with the guard band added
 * ((x + 16) * 32 = x * 32 + 0x200), so a tile index is (v >> 8).
 *
 * The layout of each struct below is the original's memory layout (field offsets in the
 * comments), so the emulator diffs compare them byte for byte.
 */
#ifndef TM2_RENDER_GPU_H
#define TM2_RENDER_GPU_H

#include <stddef.h>
#include <stdint.h>

#include "../formats/tpc.h"

#define GPU_COLS 0x54
#define GPU_ROWS 0x40
#define GPU_TILES (GPU_COLS * GPU_ROWS)
#define GPU_EDGES 9                 /* slots -1..7 (0x00be5f10..0x00be6150) */
#define GPU_MASKS 0x8000            /* edge mask table entries */

/* One tile, 0x40 bytes: 0x00be6150 + 0x40 * (row * 0x54 + col). */
typedef struct {
    int32_t u, v;           /* +0x00 +0x04 texture coordinates at the tile (textured collect) */
    uint32_t open[2];       /* +0x08 +0x0c pixels still free this frame (rows 0..3, rows 4..7) */
    uint32_t cover[2];      /* +0x10 +0x14 coverage of the current primitive, ~0 when idle */
    uint32_t draw[2];       /* +0x18 +0x1c pixels the kernel fills (open & cover) */
    int32_t stamp;          /* +0x20 primitive stamp (textured collect) */
    int32_t screen;         /* +0x24 byte offset of the tile's pixel (0, 0) in the surface */
    uint32_t f28, f2c;      /* +0x28 +0x2c zeroed by the frame clear and the textured collect */
    int32_t noise;          /* +0x30 (3 * row + 7 * col) & 0xfc, set by gpu_init */
    int32_t f34;            /* +0x34 */
    int32_t clut;           /* +0x38 palette pointer of the textured primitive */
    int32_t f3c;            /* +0x3c */
} GpuTile;

/* One edge, 0x40 bytes: 0x00be5f50 + 0x40 * i (slot -1 = 0x00be5f10 is used by an n-gon with
 * n = 0). Built by gpu_edge_setup (0x004af164), walked by gpu_edge_walk (0x004c6000). */
typedef struct {
    uint32_t octant;        /* +0x00 bit 0 dx < 0, bit 1 dy < 0, bit 2 |dx| < |dy| */
    int32_t slope;          /* +0x04 minor / major << 16 (gpu_slope) */
    int32_t xmajor;         /* +0x08 1 when |dx| >= |dy| */
    int32_t up;             /* +0x0c dy < 0: the edge sets the row's left end, else the right */
    int32_t minor_tile;     /* +0x10 tile index step when the error passes 0x1000000 */
    int32_t major_tile;     /* +0x14 tile index step of every other iteration */
    int32_t major_dcol;     /* +0x18 column count step on a major step (row step on a minor one) */
    int32_t major_drow;     /* +0x1c row step on a major step (column count step on a minor one) */
    int32_t col0, row0;     /* +0x20 +0x24 start tile (the upper end for a y major edge) */
    int32_t col1, row1;     /* +0x28 +0x2c end tile */
    int32_t end_tile;       /* +0x30 row1 * 0x54 + col1 */
    int32_t err;            /* +0x34 error term; bits 19.. index the mask table */
    int32_t err_step;       /* +0x38 slope << 8 */
    int32_t f3c;
} GpuEdge;

/* Row span of the current primitive, 0x10 bytes: 0x00c3f550 + 0x10 * row. */
typedef struct {
    int32_t lo, hi;         /* +0x00 +0x04 leftmost / rightmost tile index touched */
    int32_t lo_stamp, hi_stamp; /* +0x08 +0x0c primitive stamp when lo / hi were set */
} GpuSpan;

/* Viewport ("draw area"), 0x1958 bytes: 0x00c7fa60 + 0x1958 * i, i = 0, 1 (split screen) and
 * 2 (whole screen, set up by every gpu_viewport_setup call). */
typedef struct {
    int32_t width, height;      /* +0x00 +0x04 screen size */
    int32_t row0, row1;         /* +0x08 +0x0c tile rows (inclusive), (y * 32 >> 8) + 2 */
    int32_t col0, col1;         /* +0x10 +0x14 tile columns */
    int32_t ncols;              /* +0x18 col1 - col0 + 1 */
    int32_t tile0, tile1;       /* +0x1c +0x20 first / last tile index */
    int32_t x0, x1, y0, y1;     /* +0x24..+0x30 pixel rectangle (inclusive) */
    int32_t sx0, sx1, sy0, sy1; /* +0x34..+0x40 same, 1/32 pixel with the guard band */
    struct {
        int32_t hi;             /* +0x44 + 0x10 * row: row * 0x54 + col1 + 1 */
        int32_t lo;             /* +0x48: row * 0x54 + col0 - 1 */
        int32_t pad[2];
    } rows[0x41];               /* the 0x41st is never written */
    uint8_t enable[GPU_TILES];  /* +0x454 1 for tiles inside the rectangle */
    uint8_t pad[4];
} GpuViewport;

/* The renderer's globals (0x00c84668..0x00c84810 and 0x004e85c0..), named by use. */
typedef struct {
    uint32_t color64[2];    /* 0x00c84668 / 0x00c8466c flat color, 4 pixels (MMX kernel) */
    int32_t pitch_px;       /* 0x00c846f0 pitch / 2 */
    int32_t pitch_bytes;    /* 0x00c846a0 (pitch / 2) * 2 */
    int32_t pitch_m16;      /* 0x00c84694 pitch_bytes - 0x10 */
    int32_t pitch_q;        /* 0x00c84688 pitch / 4 */
    int32_t pitch_e;        /* 0x00c84690 pitch / 8 */
    int32_t pitch_px_m8;    /* 0x00c846b4 pitch_px - 8 */
    int32_t pitch_x4;       /* 0x00c8469c pitch_px * 4 */
    int32_t pitch_x8;       /* 0x00c84698 pitch_px * 8 */
    uint32_t color32;       /* 0x00c846c8 flat color, 2 pixels */
    uint32_t color24;       /* 0x00c847c8 0x00RRGGBB from the primitive */
    uint16_t color16;       /* 0x00c8480e flat color */
    uint16_t half_mask;     /* 0x00c8480c 0x7bde (555) / 0xf7de (565), set by gpu_frame_begin */
    int32_t row_min;        /* 0x00c847c4 first tile row touched (1000 before) */
    int32_t row_max;        /* 0x00c847f8 last tile row touched */
    int32_t hit;            /* 0x00c84704 0xff once an edge touched an enabled tile */
    int32_t ntiles;         /* 0x00c847cc tiles in the draw list */
    int32_t f7fc;           /* 0x00c847fc zeroed per primitive */
    int32_t stamp;          /* 0x004e85d4 primitive counter, +1 per gpu_dispatch */
    int32_t flip;           /* 0x004e85ec toggled by gpu_frame_begin */
    int32_t vp_index;       /* 0x00c847d0 */
    int32_t vp;             /* 0x00c847e8 byte offset of the selected viewport from vp[0] */
    int32_t vp_enable;      /* 0x00c846e4 byte offset of its enable map (DECOMPILER / ORIGINAL quirk: + index) */
    int32_t vp_rows;        /* 0x00c846e0 byte offset of its row ranges (+ 0x10 * index) */
    int32_t mmx;            /* 0x004e85c4 nonzero: MMX kernels (CPUID at init) */
    int32_t cpu_mmx;        /* 0x004e85c0 the CPUID result itself (-1 MMX, 0 none) */
    int32_t tex_mode;       /* 0x004e861c texture kernel choice (subsys_render_flag_a 0x004ad21c) */
    uint8_t format;         /* 0x00a2dba5 0 = 555, 1 = 565, else 2 */
    /* textured primitives (gpu_tex.c) */
    int32_t render_flags;   /* 0x00c846d8 (bit 3: transparent texels) */
    uint32_t tex_pixels;    /* 0x00c846f8 page pixels (address, see TM2Gpu.tex_mem) */
    uint32_t tex_clut;      /* 0x00c846bc page palette (+ 2 * palette row for 0x90 with fog) */
    uint32_t tex_w, tex_h;  /* 0x00c8474c / 0x00c84748 */
    int32_t tex_shift;      /* 0x00c84724 log2 width (8 when the width is 0), then 12 - that */
    int32_t tex_log_a;      /* 0x00c846c4 log2 width */
    int32_t tex_log_b;      /* 0x00c846dc log2 width */
    int32_t umask, vmask;   /* 0x00c846ec w - 1 / 0x00c846e8 (h - 1) << log */
    int32_t umask12, vmask12; /* 0x00c846c0 / 0x00c846b0 the same << 12 */
    int32_t du_dx, du_dy;   /* 0x00c84684 / 0x00c84680 .12 per pixel */
    int32_t dv_dx, dv_dy;   /* 0x00c84678 / 0x00c8467c .12, pre shifted by log */
    int32_t du8x, du8y;     /* 0x00c847f0 / 0x00c847ec per tile (<< 3) */
    int32_t dv8x, dv8y;     /* 0x00c847d4 / 0x00c847d8 */
    int32_t org_x, org_y;   /* 0x00c8471c / 0x00c84728 pixel (guard band) where u0, v0 apply */
    int32_t u0, v0;         /* 0x00c8473c / 0x00c8472c */
    int32_t fog;            /* 0x004e85f8 (read only here): 0x90 palette rows */
} GpuRegs;

typedef struct {
    GpuEdge edge[GPU_EDGES];        /* edge[0] is slot -1 */
    GpuTile tile[GPU_TILES];
    uint32_t list[GPU_TILES];       /* 0x00c3a150 tile indices to draw (the original keeps pointers) */
    GpuSpan span[GPU_ROWS];         /* 0x00c3f550 */
    uint32_t mask[GPU_MASKS][2];    /* 0x00c3f960 edge half plane masks */
    int32_t row_start[GPU_ROWS];    /* 0x00c7f960 row * 0x54 */
    GpuViewport vp[3];              /* 0x00c7fa60 */
    uint8_t vp_slack[0x100];        /* reads past vp[2] (see gpu_viewport_select) land here */
    uint32_t noise_a[0x80][2];      /* 0x00bdff50 from rand() at init */
    uint32_t noise_b[0x80];         /* 0x00bdfd50 zeroed at init */
    GpuRegs r;
    /* the locked surface (0x00a04674 pointer, 0x00a04670 pitch) */
    uint8_t* screen;
    int32_t pitch;
    size_t screen_bytes;            /* port: writes outside [screen, screen + screen_bytes) are dropped */
    /* texture pages: the table of tpc_register (0x00a30c48) and the memory its 32 bit addresses
     * point into: address a is tex_mem[a - tex_addr] (reads outside tex_size give 0) */
    const TpcPage* pages;
    const uint8_t* tex_mem;
    uint32_t tex_addr;
    size_t tex_size;
    /* the 3D float n-gon (code 0xa0, src/render/gpu3d.c): its state and the light table
     * 0x00c85c68. NULL = not set up: code 0xa0 then draws nothing (gpu_dispatch returns -1). */
    struct Gpu3d* g3d;
    const void* lights;             /* const Gpu3dLights* */
} TM2Gpu;

#define GPU_EDGE(g, i) (&(g)->edge[(i) + 1])

/* --- setup (src/render/gpu_init.c) --- */

/* 0x004ad360 minus the CPUID test: builds the mask table (0x004b1b90), the noise tables and tile
 * noise (0x004b2004, five rand() calls per entry, rand is the Watcom rand 0x004899c2), the row
 * table, the tile cover masks and stamps (0x004ad19c) and points 0x00c847e8 at viewport 1 (0x00c813b8).
 * mmx: what CPUID gave (0 or nonzero). */
void gpu_init(TM2Gpu* g, int mmx, int (*rand_fn)(void* user), void* rand_user);

/* 0x004b1b90 alone: the edge mask table. */
void gpu_build_masks(uint32_t (*mask)[2]);

/* 0x004ad760: tile screen offsets for 80 x 60 tiles from the pitch in bytes, plus the pitch
 * globals. Called by the lock wrapper when the surface pitch changes. */
void gpu_set_pitch(TM2Gpu* g, int32_t pitch);

/* 0x004ad3d4: viewport i for a w x h screen and the pixel rectangle x0..x1, y0..y1 (inclusive);
 * also rebuilds viewport 2 as the whole w x h screen. */
void gpu_viewport_setup(TM2Gpu* g, int i, int32_t w, int32_t h, int32_t x0, int32_t x1,
                        int32_t y0, int32_t y1);

/* 0x004ad5e0: selects viewport i. */
void gpu_viewport_select(TM2Gpu* g, int i);

/* 0x004ad084: frame clear: open masks of the whole screen (viewport 2) set, the partial last
 * column / row cut to the screen size, +0x28 / +0x2c zeroed. */
void gpu_frame_clear(TM2Gpu* g);

/* 0x004ad848: half mask for the pixel format, frame clear, toggles flip. */
void gpu_frame_begin(TM2Gpu* g);

/* --- primitives (src/render/gpu_edge.c, gpu_flat.c, gpu_dispatch.c) --- */

/* 0x004af118 */
int32_t gpu_slope(int32_t minor, int32_t major, int32_t one);
/* 0x004af164: edge from p0 to p1 (x, y in 1/32 pixel + guard band) */
void gpu_edge_setup(GpuEdge* e, const int32_t* p0, const int32_t* p1);
/* 0x004c6000 */
void gpu_edge_walk(TM2Gpu* g, GpuEdge* e);
/* 0x0049653c: 0x00RRGGBB to the screen format */
uint32_t gpu_rgb24_to_16(uint8_t format, uint32_t rgb);
/* 0x00496590 */
uint32_t gpu_rgb_to_16(uint8_t format, uint8_t r, uint8_t g, uint8_t b);
/* 0x004af694: flat primitive: tiles into the draw list, then the fill kernel */
void gpu_flat_collect(TM2Gpu* g);
/* 0x004c6400 (2 pixel stores) and 0x004c6280 (MMX, 4 pixel stores): same pixels */
void gpu_flat_fill(TM2Gpu* g);
/* 0x004ad030 / 0x004acfe0: open bit of pixel (x, y) / clear it */
uint32_t gpu_pixel_open(const TM2Gpu* g, int32_t x, int32_t y);
void gpu_pixel_take(TM2Gpu* g, int32_t x, int32_t y);
/* 0x004b17bc: line clipped to the viewport rectangle, drawing only open pixels and taking them */
void gpu_line(TM2Gpu* g, int32_t x0, int32_t y0, int32_t x1, int32_t y1);
/* 0x004b1934: line with no clipping and no masks */
void gpu_line_raw(TM2Gpu* g, int32_t x0, int32_t y0, int32_t x1, int32_t y1);

/* 0x004af7e8: textured primitive: tiles into the draw list with their texture coordinates,
 * then the kernel (gpu_tex_fill) */
void gpu_tex_collect(TM2Gpu* g);
/* 0x004c8820 (plain textured kernel) or 0x004c71f0 (MMX) by r.mmx */
void gpu_tex_fill(TM2Gpu* g);
/* the setups of the textured codes inside 0x004ad874: 0x24, 0x2c, 0x74, 0x7c, 0x90 */
void gpu_tex_prim(TM2Gpu* g, const uint8_t* prim, uint8_t code);

/* 0x004ad874 command_dispatch: one primitive (the record's command byte at +7, bit 1 ignored).
 * Ported command codes: see gpu_dispatch.c. Returns 0, or -1 for a code that is not ported yet
 * (nothing is drawn then; the original would have drawn it). */
int gpu_dispatch(TM2Gpu* g, const uint8_t* prim);

#endif /* TM2_RENDER_GPU_H */
