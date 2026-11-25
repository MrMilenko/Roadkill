/*
 * pf_display.h: the game visible logic of TM2.EXE's DirectDraw display wrappers (the display
 * object at 0x00c84840), ported over a backend interface instead of COM (docs/PLATFORM.md).
 *
 * Ported (emulator diff: tools/emu/check_pf_display.sh):
 *   0x004b4418 pf_display_lock         IDirectDrawSurface::Lock wrapper, screen pointer / pitch
 *   0x004b44e4 pf_display_unlock       IDirectDrawSurface::Unlock wrapper
 *   0x004b4508 pf_display_restore      IDirectDrawSurface::Restore on primary + offscreen
 *   0x004b4d98 pf_display_fill         IDirectDrawSurface::Blt with DDBLT_COLORFILL
 *   0x004b5128 pf_display_present      IsLost / Restore, then Blt (+ Flip) by present mode
 *   0x004b4558 pf_display_set_mode_index   mode table lookup -> create surfaces 0x004b45a0
 *   0x004b454c pf_display_mode_count, 0x004b5a14 pf_display_width, 0x004b5a10 pf_display_height,
 *   0x004b4554 pf_display_field0 (field getters)
 *
 * The display object fields used here (offsets in the original object):
 *   +0x00 u32  first field (only read by the getter 0x004b4554)
 *   +0x0c ptr  primary surface      +0x10 ptr back buffer      +0x14 ptr offscreen (draw) surface
 *   +0x58 i32  width                +0x5c i32  height          +0x60 i32  pitch of the last lock
 *   +0x6c u32  page flipping on (set by 0x004b50f8 ddraw_set_flip): selects the present table
 *   +0x70 i32  full screen (0 = window: a lock of the primary is moved to the client rect)
 *   +0x74 u8   present mode (0, 1, 3, 4; 2 and > 4 present nothing)
 *   +0x78 RECT client rect in screen coordinates (left, top, right, bottom)
 *   +0xa0 i32  number of display modes (read absolute as 0x00c848e0)
 *   +0xa4 16 byte mode records {width, height, bits per pixel, 4th value} (0x00c848e4)
 * Globals: 0x00a04674 screen pointer, 0x00a04670 screen pitch, 0x00c85008 hardware 3D device on.
 */
#ifndef PF_DISPLAY_H
#define PF_DISPLAY_H

#include <stdint.h>

#define PF_E_FAIL ((int32_t)0x80004005)            /* E_FAIL */
#define PF_DDERR_SURFACELOST ((int32_t)0x887601c2)
#define PF_DDBLT_WAIT 0x01000000u
#define PF_DDBLT_COLORFILL 0x00000400u
#define PF_DDFLIP_WAIT 1u
#define PF_DDLOCK_WAIT 1u

typedef struct PfSurface PfSurface;   /* opaque: a backend surface (the original's IDirectDrawSurface*) */

typedef struct { int32_t left, top, right, bottom; } PfRect;

typedef struct {
    int32_t width, height, bpp, extra;
} PfDisplayMode;

/* What the wrappers call. Every function returns an HRESULT style code (0 = ok) where the
   original's COM method did. */
typedef struct {
    void* user;
    /* IDirectDrawSurface::Lock(rect, &desc {dwSize 0x6c, rest 0}, DDLOCK_WAIT, NULL): fills the
       surface pointer (desc +0x24 lpSurface) and pitch (desc +0x10 lPitch) */
    int32_t (*lock)(void* user, PfSurface* s, const void* rect, uintptr_t* pixels, int32_t* pitch);
    int32_t (*unlock)(void* user, PfSurface* s, const void* rect);  /* IDirectDrawSurface::Unlock */
    int32_t (*is_lost)(void* user, PfSurface* s);                   /* IDirectDrawSurface::IsLost */
    int32_t (*restore)(void* user, PfSurface* s);                   /* IDirectDrawSurface::Restore */
    /* IDirectDrawSurface::Blt(dst rect, src, src rect, flags, DDBLTFX*). With DDBLT_COLORFILL the
       original passes a DDBLTFX {dwSize 100, dwFillColor (+0x50) = fill_color, rest 0}; without
       it the DDBLTFX pointer is NULL and fill_color is 0. */
    int32_t (*blt)(void* user, PfSurface* dst, const PfRect* dst_rect, PfSurface* src, const PfRect* src_rect,
                   uint32_t flags, uint32_t fill_color);
    int32_t (*flip)(void* user, PfSurface* s, PfSurface* target, uint32_t flags); /* IDirectDrawSurface::Flip */
    void (*renderer_set_pixels)(void* user, uintptr_t pixels);  /* 0x004ad82c */
    void (*renderer_set_pitch)(void* user, int32_t pitch);      /* 0x004ad760 */
    int32_t (*device_reload)(void* user);                       /* 0x00497390 (hardware 3D path) */
    /* 0x004b45a0 ddraw_create_surfaces(width, height, bpp, extra) */
    int32_t (*create_surfaces)(void* user, int32_t w, int32_t h, int32_t bpp, int32_t extra);
} PfDisplayBackend;

typedef struct {
    uint32_t field0;                      /* +0x00 */
    PfSurface* primary;                   /* +0x0c */
    PfSurface* back;                      /* +0x10 */
    PfSurface* offscreen;                 /* +0x14 */
    int32_t width, height;                /* +0x58, +0x5c */
    int32_t pitch;                        /* +0x60 */
    uint32_t flip;                        /* +0x6c */
    int32_t full_screen;                  /* +0x70 */
    uint8_t present_mode;                 /* +0x74 */
    PfRect client;                        /* +0x78 */
    int32_t mode_count;                   /* +0xa0 */
    PfDisplayMode modes[64];              /* +0xa4 */
    /* globals */
    uintptr_t screen_pixels;              /* 0x00a04674 */
    int32_t screen_pitch;                 /* 0x00a04670 */
    int32_t device3d;                     /* 0x00c85008 */
    const PfDisplayBackend* be;
} PfDisplay;

/* target: NULL = offscreen (+0x14), (PfSurface*)1 = primary (+0x0c), else that surface.
   out_pixels / out_pitch: NULL = the globals (see the .c for the exact quirks). */
int32_t pf_display_lock(PfDisplay* d, PfSurface* target, uintptr_t* out_pixels, int32_t* out_pitch,
                        const void* rect);
int32_t pf_display_unlock(PfDisplay* d, PfSurface* target, const void* rect);
int32_t pf_display_restore(PfDisplay* d);
int32_t pf_display_fill(PfDisplay* d, uint32_t color, PfSurface* target);
int32_t pf_display_present(PfDisplay* d);
int32_t pf_display_set_mode_index(PfDisplay* d, int32_t index);
int32_t pf_display_mode_count(const PfDisplay* d);
int32_t pf_display_width(const PfDisplay* d);
int32_t pf_display_height(const PfDisplay* d);
uint32_t pf_display_field0(const PfDisplay* d);

#endif /* PF_DISPLAY_H */
