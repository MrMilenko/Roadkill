/*
 * gpu_hi.h: the renderer built a second time for Roadkill's display switches (docs/QOL.md:
 * display.widescreen, display.internal_scale). Port code, not from the original.
 *
 * The original's tile grid (0x54 x 0x40 tiles, gpu.h) addresses at most 640 x 480 pixels. The
 * gpu_hi_*.c files compile the same renderer sources with this header first: a larger grid
 * (GPU_HI_MAX_W x GPU_HI_MAX_H pixels), every exported name and the state types renamed
 * (gpuhi_*, TM2GpuHi, GpuHi3d) so both builds link into one program, and GPU_HI defined. The
 * sources differ under GPU_HI only where the grid's size shows through:
 *   - the octant signs of an edge from bit 31, not bit 15 (gpu_edge.c: the original's bit 15
 *     test puts edges of 1024 pixels and more in the wrong octant);
 *   - the frame clear's tile counters wider than a byte (gpu_init.c);
 *   - the octant table's tile steps wider than a byte (gpu_edge.c);
 *   - a 0x74 sprite's texel step 1 / hi_scale (gpu_tex.c: the caller scales the record).
 * The original's build (no GPU_HI) is unchanged; src/host/qol/internal_scale.c drives this one.
 */
#ifndef TM2_RENDER_GPU_HI_H
#define TM2_RENDER_GPU_HI_H

#define GPU_HI 1
#define GPU_HI_MAX_W 3840
#define GPU_HI_MAX_H 1920
#define GPU_COLS (GPU_HI_MAX_W / 8 + 6)
#define GPU_ROWS (GPU_HI_MAX_H / 8 + 6)

#define TM2Gpu TM2GpuHi
#define Gpu3d GpuHi3d
#define Gpu3dLights GpuHi3dLights

#define gpu_build_masks gpuhi_build_masks
#define gpu_dispatch gpuhi_dispatch
#define gpu_edge_setup gpuhi_edge_setup
#define gpu_edge_walk gpuhi_edge_walk
#define gpu_flat_collect gpuhi_flat_collect
#define gpu_flat_fill gpuhi_flat_fill
#define gpu_frame_begin gpuhi_frame_begin
#define gpu_frame_clear gpuhi_frame_clear
#define gpu_init gpuhi_init
#define gpu_line gpuhi_line
#define gpu_line_raw gpuhi_line_raw
#define gpu_pixel_open gpuhi_pixel_open
#define gpu_pixel_take gpuhi_pixel_take
#define gpu_rgb_to_16 gpuhi_rgb_to_16
#define gpu_rgb24_to_16 gpuhi_rgb24_to_16
#define gpu_set_pitch gpuhi_set_pitch
#define gpu_slope gpuhi_slope
#define gpu_tex_collect gpuhi_tex_collect
#define gpu_tex_fill gpuhi_tex_fill
#define gpu_tex_prim gpuhi_tex_prim
#define gpu_viewport_select gpuhi_viewport_select
#define gpu_viewport_setup gpuhi_viewport_setup
#define gpu3d_collect_fog gpuhi3d_collect_fog
#define gpu3d_collect_light gpuhi3d_collect_light
#define gpu3d_collect_plain gpuhi3d_collect_plain
#define gpu3d_dispatch gpuhi3d_dispatch
#define gpu3d_dispatch_all gpuhi3d_dispatch_all
#define gpu3d_fog_fill gpuhi3d_fog_fill
#define gpu3d_lowdetail_fill gpuhi3d_lowdetail_fill
#define gpu3d_overlay_fill gpuhi3d_overlay_fill
#define gpu3d_overlay_mark gpuhi3d_overlay_mark
#define gpu3d_prim gpuhi3d_prim
#define gpu3d_tex_kernel gpuhi3d_tex_kernel

#include "render/gpu.h"
#include "render/gpu3d.h"

#endif /* TM2_RENDER_GPU_HI_H */
