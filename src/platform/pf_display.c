/*
 * pf_display.c: TM2.EXE's DirectDraw display wrappers, the game visible part (see pf_display.h,
 * docs/PLATFORM.md). Each function names the original it stands in for; the COM calls go through
 * PfDisplayBackend in the same order and with the same arguments.
 */
#include "pf_display.h"

#include <stddef.h>

/* 0x004b4418 / 0x004b44e4: target 0 -> the offscreen surface, then a target equal to 1 -> the
   primary (the two tests are in sequence, as in the original) */
static PfSurface* pick(const PfDisplay* d, PfSurface* t) {
    if (t == NULL) t = d->offscreen;
    if (t == (PfSurface*)1) t = d->primary;
    return t;
}

/* 0x004b4418 lock(eax display, edx target, ebx out pixels, ecx out pitch, [esp+4] rect), ret 4.
   Lock(target, rect, desc {dwSize 0x6c}, DDLOCK_WAIT, NULL). On failure returns the HRESULT and
   changes nothing. On success:
   - pixels -> *out_pixels, or the screen pointer 0x00a04674 when out_pixels is NULL;
   - the renderer is given the screen pointer 0x00a04674 (0x004ad82c) in both cases (with
     out_pixels set it is the old value: ORIGINAL BUG, harmless, the callers that pass
     out_pixels do not draw through the renderer);
   - pitch -> *out_pitch, or, when out_pitch is NULL and it differs from +0x60: 0x00a04670 and
     +0x60 and the renderer (0x004ad760);
   - locking the primary in a window (+0x70 full screen == 0) moves 0x00a04674 to the client rect, using
     +0x60 (even when the pointer went to *out_pixels: the global moves, ORIGINAL BUG).
   DECOMPILER TRAP: Ghidra swaps lpSurface (desc +0x24) and lPitch (desc +0x10). */
int32_t pf_display_lock(PfDisplay* d, PfSurface* target, uintptr_t* out_pixels, int32_t* out_pitch,
                        const void* rect) {
    PfSurface* s = pick(d, target);
    uintptr_t px = 0;
    int32_t pitch = 0;
    int32_t hr = d->be->lock(d->be->user, s, rect, &px, &pitch);
    if (hr != 0) return hr;
    if (out_pixels) *out_pixels = px;
    else d->screen_pixels = px;
    d->be->renderer_set_pixels(d->be->user, d->screen_pixels);
    if (out_pitch) {
        *out_pitch = pitch;
    } else if (pitch != d->pitch) {
        d->screen_pitch = pitch;
        d->pitch = pitch;
        d->be->renderer_set_pitch(d->be->user, pitch);
    }
    if (s == d->primary && d->full_screen == 0) {
        /* imul / add in 32 bits, as the original */
        uint32_t off = (uint32_t)d->pitch * (uint32_t)d->client.top + (uint32_t)d->client.left * 2u;
        d->screen_pixels += (uintptr_t)(intptr_t)(int32_t)off;
    }
    return hr;
}

/* 0x004b44e4 unlock(eax display, edx target, ebx rect): Unlock(target, rect); 0x00a04674 = 0
   whatever Unlock returned; returns the HRESULT. */
int32_t pf_display_unlock(PfDisplay* d, PfSurface* target, const void* rect) {
    PfSurface* s = pick(d, target);
    int32_t hr = d->be->unlock(d->be->user, s, rect);
    d->screen_pixels = 0;
    return hr;
}

/* 0x004b4508 restore(eax display): Restore() the primary if there is one, then the offscreen
   surface if there is one and it is not the back buffer (that one is restored with the
   primary's flip chain). With the hardware 3D device on (0x00c85008) 0x00497390 reloads the
   device's resources: its result is returned if nonzero, else 0. Without it the last Restore
   result is returned; with neither surface the original returns the caller's edx (ORIGINAL
   BUG: an unset register); the port returns 0. */
int32_t pf_display_restore(PfDisplay* d) {
    int32_t hr = 0;
    if (d->primary) hr = d->be->restore(d->be->user, d->primary);
    if (d->offscreen && d->offscreen != d->back) hr = d->be->restore(d->be->user, d->offscreen);
    if (d->device3d) {
        int32_t r = d->be->device_reload(d->be->user);
        return r;   /* nonzero -> r; zero -> edx = 0 */
    }
    return hr;
}

/* 0x004b4d98 fill(eax display, edx color, ebx surface or 0 = offscreen): E_FAIL without a
   surface; else Blt(surface, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx) with a DDBLTFX
   {dwSize 100, dwFillColor = color}. Returns the HRESULT. */
int32_t pf_display_fill(PfDisplay* d, uint32_t color, PfSurface* target) {
    PfSurface* s = target ? target : d->offscreen;
    if (!s) return PF_E_FAIL;
    return d->be->blt(d->be->user, s, NULL, NULL, NULL, PF_DDBLT_COLORFILL | PF_DDBLT_WAIT, color);
}

/* 0x004b5128 present(eax display). No primary: returns 0. IsLost(primary) == DDERR_SURFACELOST
   -> restore (0x004b4508, result ignored). Then by +0x6c (page flipping) and the byte +0x74:
     flip on:     0 Blt(primary, &client rect, offscreen, NULL, DDBLT_WAIT)
                  1 Blt(primary, NULL, offscreen, NULL, DDBLT_WAIT)
                  3 Blt(back, NULL, offscreen, NULL, DDBLT_WAIT) (result ignored), Flip(primary, NULL, DDFLIP_WAIT)
                  4 Flip(primary, NULL, DDFLIP_WAIT)
     flip off:    0 Blt(primary, &client rect, offscreen, NULL, DDBLT_WAIT); 1, 3, 4 Blt(primary, NULL, ...)
   The flip off jump table's cases push ecx (= +0x6c = 0) where the flip on ones push 0.
   Returns the last HRESULT. Modes 2 and > 4 do nothing and return the caller's esi (ORIGINAL
   BUG: an unset register); the port returns 0. */
int32_t pf_display_present(PfDisplay* d) {
    const PfDisplayBackend* be = d->be;
    if (!d->primary) return 0;
    if (be->is_lost(be->user, d->primary) == PF_DDERR_SURFACELOST) pf_display_restore(d);
    uint8_t m = d->present_mode;
    if (d->flip) {
        switch (m) {
        case 0: return be->blt(be->user, d->primary, &d->client, d->offscreen, NULL, PF_DDBLT_WAIT, 0);
        case 1: return be->blt(be->user, d->primary, NULL, d->offscreen, NULL, PF_DDBLT_WAIT, 0);
        case 3: be->blt(be->user, d->back, NULL, d->offscreen, NULL, PF_DDBLT_WAIT, 0);
                return be->flip(be->user, d->primary, NULL, PF_DDFLIP_WAIT);
        case 4: return be->flip(be->user, d->primary, NULL, PF_DDFLIP_WAIT);
        default: return 0;
        }
    }
    switch (m) {
    case 0: return be->blt(be->user, d->primary, &d->client, d->offscreen, NULL, PF_DDBLT_WAIT, 0);
    case 1: case 3: case 4:
        return be->blt(be->user, d->primary, NULL, d->offscreen, NULL, PF_DDBLT_WAIT, 0);
    default: return 0;
    }
}

/* 0x004b4558 set_mode_index(eax display, edx index): 0 <= index < count (signed) -> create the
   surfaces for mode record index (0x004b45a0 with width, height, bpp in edx, ebx, ecx and the
   4th value on the stack); else E_FAIL. On success the original returns the caller's edi, not
   the create result (ORIGINAL BUG: edi is only set on the failure path); the port returns the
   create result. */
int32_t pf_display_set_mode_index(PfDisplay* d, int32_t index) {
    if (index >= d->mode_count || index < 0) return PF_E_FAIL;
    const PfDisplayMode* m = &d->modes[index];
    return d->be->create_surfaces(d->be->user, m->width, m->height, m->bpp, m->extra);
}

int32_t pf_display_mode_count(const PfDisplay* d) { return d->mode_count; }   /* 0x004b454c: [0x00c848e0] */
int32_t pf_display_width(const PfDisplay* d) { return d->width; }             /* 0x004b5a14: +0x58 */
int32_t pf_display_height(const PfDisplay* d) { return d->height; }           /* 0x004b5a10: +0x5c */
uint32_t pf_display_field0(const PfDisplay* d) { return d->field0; }          /* 0x004b4554: [eax] */
