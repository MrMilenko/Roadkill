/*
 * host/qol/display.h: Roadkill's display switches (docs/QOL.md), both off by default. Port code, not
 * from the original game.
 *
 *   display.widescreen      0 (the original 4:3), 1 (16:9) or an aspect "w:h" (e.g. 16:10, 21:9).
 *                           The game's 4:3 display modes get a wider width at the same height
 *                           (640 x 480 -> 854 x 480 at 16:9) where the game picks a mode from its
 *                           list (the match: its display settings), so matches show more of the
 *                           arena left and right; the field of view keeps the original's vertical
 *                           angle (en10_viewport.c). Menus set 640 x 480 themselves and stay 4:3,
 *                           pillarboxed in the wide window.
 *   display.internal_scale  1 (the original), 2, 3, 4: the renderer (src/render, built a second
 *                           time with a larger tile grid, gpu_hi.h) draws every primitive at this
 *                           multiple of the game's resolution into a host buffer; the game's own
 *                           surface keeps the 640 x 480 picture (drawn as before) for everything
 *                           that is not the renderer (backgrounds, fills, GDI text), and the
 *                           picture shown is the large one wherever the renderer drew
 *                           (host/qol/internal_scale.c).
 *
 * Read through settings.h (roadkill.ini; TM2_WIDESCREEN, TM2_INTERNAL_SCALE override). With both
 * off nothing here changes what the game does: the original renderer, view setup and display
 * sizes run as they always did.
 */
#ifndef TM2_HOST_QOL_DISPLAY_H
#define TM2_HOST_QOL_DISPLAY_H

#include <stddef.h>
#include <stdint.h>

/* widescreen: 0 off, else the aspect as width / height (e.g. 16 / 9) */
double qol_display_aspect(void);
/* internal scale: 1 (off) .. QOL_DISPLAY_MAX_SCALE */
int qol_display_scale(void);
#define QOL_DISPLAY_MAX_SCALE 4
/* either switch on: the renderer runs in its GPU_HI build (host/qol/internal_scale.c) */
int qol_display_active(void);

/* the width a 4:3 display mode w x h gets (w itself when widescreen is off or the mode is not
 * 4:3): h * aspect, even */
int qol_display_wide_width(int w, int h);

/* ---- host/qol/internal_scale.c: the renderer in its GPU_HI build (called by src/rt/bind_render.c) ---- */

/* the inputs every renderer call reads (bind_render.c's sync_in) */
typedef struct {
    uint8_t* screen;            /* the locked game surface */
    size_t screen_bytes;
    int32_t pitch;
    const void* pages;          /* const TpcPage* */
    const uint8_t* tex_mem;
    uint32_t tex_addr;
    size_t tex_size;
    const uint8_t* lights;      /* light table 0x00c85c68 */
    uint32_t lights_count;
    int32_t dither, special;
} QolGpuIn;

/* regs: the renderer's scalars (GpuRegs, gpu.h) as the game has them; the calls that draw
 * write them back as the original would leave them (pointer and pitch registers excepted) */
void qol_gpu_init(const void* regs, const uint32_t noise_a[0x80][2], const uint32_t noise_b[0x80]);
void qol_gpu_set_pitch(int32_t pitch);
void qol_gpu_viewport_setup(int i, int32_t w, int32_t h, int32_t x0, int32_t x1, int32_t y0, int32_t y1);
void qol_gpu_viewport_select(int i);
void qol_gpu_frame_begin(void* regs, const QolGpuIn* in, int clear_only);
int qol_gpu_dispatch(void* regs, const QolGpuIn* in, const uint8_t* prim);
void qol_gpu_line(void* regs, const QolGpuIn* in, int raw, int32_t x0, int32_t y0, int32_t x1, int32_t y1);
uint32_t qol_gpu_pixel_open(int32_t x, int32_t y);
void qol_gpu_pixel_take(int32_t x, int32_t y);

/* the host side (dx_ddraw.c) at internal scale > 1:
 * a rectangle of the game surface (surf_w x surf_h pixels) was written by something other than the
 * renderer (Blt, GDI): what the renderer drew there so far this frame no longer shows */
void qol_gpu_overwritten(int x, int y, int w, int h, int surf_w, int surf_h);
/* the picture to show for the game surface px (lw x lh, pitch bytes): the renderer's large
 * picture where it drew this frame, the game surface scaled elsewhere. Returns the buffer
 * (16 bit pixels, *ow x *oh, *opitch bytes) or NULL: show the game surface as it is. */
const uint16_t* qol_gpu_compose(const uint8_t* px, int lw, int lh, int pitch, int* ow, int* oh, int* opitch);

#endif /* TM2_HOST_QOL_DISPLAY_H */
