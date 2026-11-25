/*
 * pf2_dd.c: TM2.EXE's small DirectDraw / Direct3D / window wrappers (docs/PLATFORM.md Part A / B),
 * ported over Pf2Env (pf2_env.h): the COM and Win32 calls in the original's order with the
 * original's arguments, the logic around them ported. Emulator diff: tools/emu/check_pf2_dd.sh.
 *
 * Arguments are the original's registers in order (eax, edx, ebx, ecx) then its stack words.
 * D = the display object 0x00c84840: +0x08 IDirectDraw2, +0x0c primary, +0x10 back buffer,
 * +0x14 offscreen surface, +0x20 IDirectDrawClipper, +0x28 IDirect3DDevice2, +0x4c material
 * handle, +0x54 HWND, +0x58 / +0x5c width / height, +0x68 refresh, +0x6c page flipping wanted,
 * +0x70 full screen, +0x74 present mode (byte), +0x78 client rect, +0x90 last ShowCursor value.
 * Globals: 0x00c85004 live surface count, 0x00c85008 hardware 3D on, 0x00c84da0 / 0x00c84da8 caps
 * flags, 0x00c84f74.. render state cache, 0x00a2dc4c texture 0's color key count.
 */
#include "pf2_env.h"

#define R32 pf2_r32
#define W32 pf2_w32
#define SURFCOUNT 0x00c85004u
#define HW3D 0x00c85008u

/* Win32 imports (IAT slots) */
#define I_ClipCursor 0x00c90474u
#define I_ShowCursor 0x00c90580u
#define I_GetWindowLongA 0x00c904ecu
#define I_SetWindowLongA 0x00c90578u
#define I_SetRect 0x00c90574u
#define I_GetMenu 0x00c904d0u
#define I_AdjustWindowRect 0x00c90464u
#define I_MoveWindow 0x00c90528u
#define I_GetClientRect 0x00c904bcu

/* game functions outside this file */
#define F_MEMSET 0x004bf830u        /* crt_memset(eax dst, dl byte, ebx count): done here as memset */
#define F_ERROR_BOX 0x004bc660u     /* win32_error_box(code, hr & 0xfff), cdecl */
#define F_FATAL 0x004a8f24u         /* fatal_quit: does not return (the port returns 0 after it) */
#define F_FOG_COLOR 0x00494dc0u     /* fog color (0x00rrggbb) */
#define F_COLOR16 0x0049653cu       /* 24 bit color -> the surface's 16 bit pixel */

static void error_box(Pf2Env* e, uint32_t code, uint32_t hr) { pf2_call2(e, F_ERROR_BOX, code, hr & 0xfff); }

/* 0x004b42b8 cursor_unclip: ClipCursor(NULL); returns 0 */
uint32_t pf2_cursor_unclip(Pf2Env* e) {
    pf2_call1(e, I_ClipCursor, 0);
    return 0;
}

/* 0x004b4c34 detach_clipper(D): primary: GetClipper(&local) and, if that succeeded,
   SetClipper(NULL); the clipper +0x20: Release, +0x20 = 0. Returns the last of GetClipper /
   SetClipper; without a primary the original returns esi as the caller left it (ORIGINAL BUG,
   unset register; the port returns 0). */
uint32_t pf2_ddraw_detach_clipper(Pf2Env* e, uint32_t d) {
    uint32_t r = 0;
    uint32_t p = R32(e, d + 0xc);
    if (p) {
        r = PF2_C1(e, PF2_ISURF, 0x3c, p, e->scratch);
        if (r == 0) r = PF2_C1(e, PF2_ISURF, 0x70, R32(e, d + 0xc), 0);
    }
    uint32_t c = R32(e, d + 0x20);
    if (c) {
        PF2_C0(e, PF2_ICLIP, 8, c);
        W32(e, d + 0x20, 0);
    }
    return r;
}

/* 0x004b4374 release_surfaces(D): detach_clipper; offscreen +0x14 if set and not the back
   buffer: Release, count--; primary +0x0c if set: Release, count--. If the last Release returned
   0 (no references left) +0x0c, +0x10, +0x14 = 0. ORIGINAL BUG: with neither surface set the test
   is on edx as the caller left it (edx_in here). Returns the last Release result, else
   detach_clipper's. */
uint32_t pf2_ddraw_release_surfaces(Pf2Env* e, uint32_t d, uint32_t edx_in) {
    uint32_t r = pf2_ddraw_detach_clipper(e, d), last = edx_in;
    uint32_t o = R32(e, d + 0x14);
    if (o && o != R32(e, d + 0x10)) {
        r = last = PF2_C0(e, PF2_ISURF, 8, o);
        W32(e, SURFCOUNT, R32(e, SURFCOUNT) - 1);
    }
    uint32_t p = R32(e, d + 0xc);
    if (p) {
        r = last = PF2_C0(e, PF2_ISURF, 8, p);
        W32(e, SURFCOUNT, R32(e, SURFCOUNT) - 1);
    }
    if (last == 0) {
        W32(e, d + 0xc, 0);
        W32(e, d + 0x10, 0);
        W32(e, d + 0x14, 0);
    }
    return r;
}

/* 0x004b480c flip_to_gdi(D): no IDirectDraw2 -> 0; IDirectDraw2::FlipToGDISurface */
uint32_t pf2_ddraw_flip_to_gdi(Pf2Env* e, uint32_t d) {
    uint32_t dd = R32(e, d + 8);
    if (!dd) return 0;
    return PF2_C0(e, PF2_IDD2, 0x28, dd);
}

/* 0x004b4828 create_surface(eax D, edx out, ebx width, ecx height, stack caps, back buffers;
   ret 8): DDSURFACEDESC (0x6c bytes, zeroed): dwSize 0x6c, ddsCaps (+0x68) = caps,
   dwBackBufferCount (+0x14) = back; width and height both 0: flags DDSD_CAPS (1), else
   CAPS | HEIGHT | WIDTH (7) with +0x08 height, +0x0c width; back != 0 adds DDSD_BACKBUFFERCOUNT
   (0x20). IDirectDraw2::CreateSurface(&desc, out, NULL); success: live surface count + 1. */
uint32_t pf2_ddraw_create_surface(Pf2Env* e, uint32_t d, uint32_t out, uint32_t w, uint32_t h, uint32_t caps,
                                  uint32_t back) {
    uint32_t s = e->scratch;
    pf2_zero(e, s, 0x6c);
    W32(e, s + 0x68, caps);
    W32(e, s, 0x6c);
    W32(e, s + 0x14, back);
    uint32_t fl;
    if (h == 0 && w == 0) {
        fl = 1;
    } else {
        W32(e, s + 0xc, w);
        W32(e, s + 8, h);
        fl = 7;
    }
    if (back) fl |= 0x20;
    W32(e, s + 4, fl);
    uint32_t r = PF2_C3(e, PF2_IDD2, 0x18, R32(e, d + 8), s, out, 0);
    if (r == 0) W32(e, SURFCOUNT, R32(e, SURFCOUNT) + 1);
    return r;
}

/* 0x004b48b8 release_surface(eax D, edx pointer to a surface field): no IDirectDraw2 -> 0;
   a surface there: Release, count - 1 (returns Release's result), else 0; the field = 0 */
uint32_t pf2_ddraw_release_surface(Pf2Env* e, uint32_t d, uint32_t field) {
    if (R32(e, d + 8) == 0) return 0;
    uint32_t r = 0, s = R32(e, field);
    if (s) {
        r = PF2_C0(e, PF2_ISURF, 8, s);
        W32(e, SURFCOUNT, R32(e, SURFCOUNT) - 1);
    }
    W32(e, field, 0);
    return r;
}

/* 0x004b48f4 create_flip_chain(D): unless 0x00c84da8 (no 2D during 3D) is set: primary +0x0c with
   caps 0x4218 (| 0x2000 3DDEVICE when hardware 3D) and 2 back buffers; failing (or skipped:
   E_FAIL) the same with 1 back buffer, then caps 0x218 (any memory) with 1; failing again:
   release_surfaces, error box 0xd, fatal_quit. GetAttachedSurface(primary, &{caps 4 BACKBUFFER},
   +0x10) (failure: release_surfaces, error 0xb, fatal_quit); +0x14 = +0x10, present mode 4;
   GetCaps(back buffer, &caps): in video memory (caps & 0x4000 DDSCAPS_VIDEOMEMORY set; docs/PLATFORM.md
   had this reversed) and 0x00c84da0 set: a system memory W x H surface (caps 0x800) at +0x14 to
   draw into, on success present mode 3.
   Returns 0. */
uint32_t pf2_ddraw_create_flip_chain(Pf2Env* e, uint32_t d) {
    uint32_t r;
    uint32_t hw = R32(e, HW3D) ? 0x2000u : 0;
    if (R32(e, 0x00c84da8u) == 0) r = pf2_ddraw_create_surface(e, d, d + 0xc, 0, 0, hw | 0x4218, 2);
    else r = 0x80004005u;
    if (r != 0) {
        hw = R32(e, HW3D) ? 0x2000u : 0;
        r = pf2_ddraw_create_surface(e, d, d + 0xc, 0, 0, hw | 0x4218, 1);
        if (r != 0) {
            hw = R32(e, HW3D) ? 0x2000u : 0;
            r = pf2_ddraw_create_surface(e, d, d + 0xc, 0, 0, hw | 0x218, 1);
            if (r != 0) {
                pf2_ddraw_release_surfaces(e, d, r & 0xfff);
                error_box(e, 0xd, r);
                pf2_call0(e, F_FATAL);
                return 0;   /* fatal_quit does not return */
            }
        }
    }
    uint32_t c = e->scratch;
    W32(e, c, 4);
    r = PF2_C2(e, PF2_ISURF, 0x30, R32(e, d + 0xc), c, d + 0x10);
    if (r != 0) {
        pf2_ddraw_release_surfaces(e, d, r & 0xfff);
        error_box(e, 0xb, r);
        pf2_call0(e, F_FATAL);
        return 0;   /* fatal_quit does not return */
    }
    W32(e, d + 0x14, R32(e, d + 0x10));
    pf2_w8(e, d + 0x74, 4);
    PF2_C1(e, PF2_ISURF, 0x38, R32(e, d + 0x10), c);
    if ((pf2_r8(e, c + 1) & 0x40) && R32(e, 0x00c84da0u) != 0) {
        if (pf2_ddraw_create_surface(e, d, d + 0x14, R32(e, d + 0x58), R32(e, d + 0x5c), 0x800, 0) == 0)
            pf2_w8(e, d + 0x74, 3);
    }
    return 0;
}

/* 0x004b4a54 show_cursor(eax D, edx show): when show differs from +0x90: +0x90 = show,
   ShowCursor(show) (returns its result); else returns D (eax unchanged) */
uint32_t pf2_ddraw_show_cursor(Pf2Env* e, uint32_t d, uint32_t show) {
    if (show == R32(e, d + 0x90)) return d;
    W32(e, d + 0x90, show);
    return pf2_call1(e, I_ShowCursor, show);
}

/* 0x004b4b18 create_window_surfaces(D): primary (caps 0x200 | 0x2000 with hardware 3D, no size);
   failure: error box 0xc, fatal_quit. Offscreen +0x14: W x H (+0x58, +0x5c), caps 0x800 (| 0x2000);
   failure: error box 0xf, fatal_quit. Present mode +0x74 = (full screen +0x70 != 0). Returns the
   offscreen create result. */
uint32_t pf2_ddraw_create_window_surfaces(Pf2Env* e, uint32_t d) {
    uint32_t hw = R32(e, HW3D) ? 0x2000u : 0;
    uint32_t r = pf2_ddraw_create_surface(e, d, d + 0xc, 0, 0, hw | 0x200, 0);
    if (r != 0) {
        error_box(e, 0xc, r);
        pf2_call0(e, F_FATAL);
        return 0;   /* fatal_quit does not return */
    }
    hw = R32(e, HW3D) ? 0x2000u : 0;
    r = pf2_ddraw_create_surface(e, d, d + 0x14, R32(e, d + 0x58), R32(e, d + 0x5c), hw | 0x800, 0);
    if (r != 0) {
        error_box(e, 0xf, r);
        pf2_call0(e, F_FATAL);
        return 0;   /* fatal_quit does not return */
    }
    pf2_w8(e, d + 0x74, R32(e, d + 0x70) != 0);
    return r;
}

/* 0x004b4bb4 attach_clipper(D): no clipper +0x20 yet: with an IDirectDraw2,
   CreateClipper(0, &+0x20, NULL) (failure: error box 0x10, return the HRESULT); then
   IDirectDrawClipper::SetHWnd(0, hwnd +0x54) (failure: error box 0x11, return it). Then, the
   primary set: SetClipper(primary, +0x20) and its result is returned. ORIGINAL BUG: with a clipper
   already there and no primary the original returns esi as the caller left it (the port: 0). */
uint32_t pf2_ddraw_attach_clipper(Pf2Env* e, uint32_t d) {
    uint32_t r = 0;
    if (R32(e, d + 0x20) == 0) {
        uint32_t dd = R32(e, d + 8);
        if (dd) {
            r = PF2_C3(e, PF2_IDD2, 0x10, dd, 0, d + 0x20, 0);
            if (r != 0) {
                error_box(e, 0x10, r);
                return r;
            }
        }
        r = PF2_C2(e, PF2_ICLIP, 0x20, R32(e, d + 0x20), 0, R32(e, d + 0x54));
        if (r != 0) {
            error_box(e, 0x11, r);
            return r;
        }
    }
    uint32_t p = R32(e, d + 0xc);
    if (p) r = PF2_C1(e, PF2_ISURF, 0x70, p, R32(e, d + 0x20));
    return r;
}

/* 0x004b4e38 fill_rect_fog(eax D, edx rect): no offscreen +0x14 -> E_FAIL; the fog color
   0x00494dc0 -> 16 bit 0x0049653c (low word) as the fill color of a DDBLTFX (100 bytes, zeroed,
   dwSize 100, +0x50 fill color); Blt(offscreen, rect, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT) */
uint32_t pf2_ddraw_fill_rect_fog(Pf2Env* e, uint32_t d, uint32_t rect) {
    uint32_t s = R32(e, d + 0x14);
    if (!s) return 0x80004005u;
    uint32_t c = pf2_call0(e, F_FOG_COLOR);
    uint32_t fx = e->scratch;
    pf2_zero(e, fx, 0x64);
    W32(e, fx, 0x64);
    W32(e, fx + 0x50, pf2_call1(e, F_COLOR16, c) & 0xffff);
    return PF2_C5(e, PF2_ISURF, 0x14, s, rect, 0, 0, 0x1000400, fx);
}

/* 0x004b50a4 get_dc(eax D, edx out HDC*, ebx surface (0 = offscreen +0x14)): GetDC */
uint32_t pf2_ddraw_get_dc(Pf2Env* e, uint32_t d, uint32_t out, uint32_t s) {
    if (!s) s = R32(e, d + 0x14);
    return PF2_C1(e, PF2_ISURF, 0x44, s, out);
}

/* 0x004b50b8 release_dc(eax D, edx HDC, ebx surface (0 = offscreen)): ReleaseDC */
uint32_t pf2_ddraw_release_dc(Pf2Env* e, uint32_t d, uint32_t hdc, uint32_t s) {
    if (!s) s = R32(e, d + 0x14);
    return PF2_C1(e, PF2_ISURF, 0x68, s, hdc);
}

/* 0x004b50cc wait_vblank(D): no IDirectDraw2 -> 0; WaitForVerticalBlank(DDWAITVB_BLOCKBEGIN, 0) */
uint32_t pf2_ddraw_wait_vblank(Pf2Env* e, uint32_t d) {
    uint32_t dd = R32(e, d + 8);
    if (!dd) return 0;
    return PF2_C2(e, PF2_IDD2, 0x58, dd, 1, 0);
}

/* 0x004b50f8 set_flip(eax D, edx v): +0x6c = v; returns D */
uint32_t pf2_ddraw_set_flip(Pf2Env* e, uint32_t d, uint32_t v) {
    W32(e, d + 0x6c, v);
    return d;
}

/* 0x004b584c mode_compare(eax a, edx b): mode records {w, h, bpp, refresh}: bpp difference, then
   width, then height (32 bit subtraction) */
uint32_t pf2_mode_compare(Pf2Env* e, uint32_t a, uint32_t b) {
    uint32_t r = R32(e, a + 8) - R32(e, b + 8);
    if (r) return r;
    r = R32(e, a) - R32(e, b);
    if (r) return r;
    return R32(e, a + 4) - R32(e, b + 4);
}

/* 0x004b5874 get_caps(eax D, edx block): two DDCAPS (0x16c bytes, zeroed, dwSize 0x16c) at
   +0x1e4 (hardware) and +0x350 (emulation); IDirectDraw2::GetCaps(hw, hel); failure: its result.
   Then +0x4bc = dwCaps & 1 (3D), +0x4c0 = dwCaps bit 27 (bank switched), +0x4c4 = bit 9
   (stretch), +0x4c8 = dwCaps2 bit 1 (no 2D during 3D scene); returns 0. */
uint32_t pf2_ddraw_get_caps(Pf2Env* e, uint32_t d, uint32_t b) {
    pf2_zero(e, b + 0x1e4, 0x16c);
    W32(e, b + 0x1e4, 0x16c);
    pf2_zero(e, b + 0x350, 0x16c);
    W32(e, b + 0x350, 0x16c);
    uint32_t r = PF2_C2(e, PF2_IDD2, 0x2c, R32(e, d + 8), b + 0x1e4, b + 0x350);
    if (r) return r;
    W32(e, b + 0x4bc, (pf2_r8(e, b + 0x1e8) & 1) != 0);
    W32(e, b + 0x4c0, (pf2_r8(e, b + 0x1eb) & 8) != 0);
    W32(e, b + 0x4c4, (pf2_r8(e, b + 0x1e9) & 2) != 0);
    W32(e, b + 0x4c8, (pf2_r8(e, b + 0x1ec) & 2) != 0);
    return 0;
}

/* 0x004b592c enum_modes_cb(stack desc, list; stdcall, ret 8): 16 bit modes (desc +0x54
   dwRGBBitCount == 16) no wider than 640 (800 with hardware 3D; unsigned compare): appended to
   the list {count, then 16 byte {width +0x0c, height +0x08, bpp, refresh +0x18}}. Returns 1. */
uint32_t pf2_ddraw_enum_modes_cb(Pf2Env* e, uint32_t desc, uint32_t list) {
    if (R32(e, desc + 0x54) == 0x10) {
        uint32_t maxw = R32(e, HW3D) ? 0x320u : 0x280u;
        uint32_t w = R32(e, desc + 0xc);
        if (maxw >= w) {
            uint32_t n = R32(e, list), p = list + (n << 4);
            W32(e, p + 4, w);
            W32(e, p + 8, R32(e, desc + 8));
            W32(e, p + 0xc, R32(e, desc + 0x54));
            W32(e, p + 0x10, R32(e, desc + 0x18));
            W32(e, list, R32(e, list) + 1);
        }
    }
    return 1;
}

uint32_t pf2_ddraw_get_refresh_rate(Pf2Env* e, uint32_t d) { return R32(e, d + 0x68); }   /* 0x004b5988 */

/* 0x004b5a20 get_pixel_masks(eax D, edx r out, ebx g out, ecx b out, stack alpha out; ret 4):
   DDSURFACEDESC (0x6c, zeroed, dwSize 0x6c); primary GetSurfaceDesc (result ignored); the masks
   desc +0x58 / +0x5c / +0x60 / +0x64 to the outs that are not NULL; returns 0 */
uint32_t pf2_ddraw_get_pixel_masks(Pf2Env* e, uint32_t d, uint32_t ro, uint32_t go, uint32_t bo, uint32_t ao) {
    uint32_t s = e->scratch;
    pf2_zero(e, s, 0x6c);
    W32(e, s, 0x6c);
    PF2_C1(e, PF2_ISURF, 0x58, R32(e, d + 0xc), s);
    if (ro) W32(e, ro, R32(e, s + 0x58));
    if (go) W32(e, go, R32(e, s + 0x5c));
    if (bo) W32(e, bo, R32(e, s + 0x60));
    if (ao) W32(e, ao, R32(e, s + 0x64));
    return 0;
}

/* 0x004b5a94 resize_window(eax D, edx x, ebx y, ecx w, stack h; ret 4): windowed (+0x70 == 0):
   style = GetWindowLongA(hwnd, GWL_STYLE); SetRect(&r, x, y, x + w, y + h);
   AdjustWindowRect(&r, style, GetMenu(hwnd) != 0). Full screen: SetRect(&r, 0, 0, w, h).
   MoveWindow(hwnd, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE); returns 0. */
uint32_t pf2_ddraw_resize_window(Pf2Env* e, uint32_t d, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    uint32_t r = e->scratch;
    if (R32(e, d + 0x70) == 0) {
        uint32_t style = pf2_call2(e, I_GetWindowLongA, R32(e, d + 0x54), (uint32_t)-16);
        pf2_call5(e, I_SetRect, r, x, y, w + x, h + y);
        uint32_t menu = pf2_call1(e, I_GetMenu, R32(e, d + 0x54)) != 0;
        pf2_call3(e, I_AdjustWindowRect, r, style, menu);
    } else {
        pf2_call5(e, I_SetRect, r, 0, 0, w, h);
    }
    uint32_t l = R32(e, r), t = R32(e, r + 4);
    pf2_call6(e, I_MoveWindow, R32(e, d + 0x54), l, t, R32(e, r + 8) - l, R32(e, r + 0xc) - t, 1);
    return 0;
}

/* 0x004b5b40 set_client_origin(eax D, edx x, ebx y): windowed (0x00c848b0 = the display's +0x70,
   read absolute): GetClientRect(hwnd, &r); SetRect(D + 0x78, x, y, x + r.right, y + r.bottom).
   Returns 0. */
uint32_t pf2_ddraw_set_client_origin(Pf2Env* e, uint32_t d, uint32_t x, uint32_t y) {
    if (R32(e, 0x00c848b0u) == 0) {
        uint32_t r = e->scratch;
        pf2_call2(e, I_GetClientRect, R32(e, d + 0x54), r);
        pf2_call5(e, I_SetRect, d + 0x78, x, y, R32(e, r + 8) + x, R32(e, r + 0xc) + y);
    }
    return 0;
}

/* ---- Direct3D device helpers ---- */

uint32_t pf2_d3d_is_active(Pf2Env* e) { return R32(e, HW3D); }                 /* 0x004b5f60 */
uint32_t pf2_d3d_get_device(Pf2Env* e, uint32_t d) { return R32(e, d + 0x28); } /* 0x004b5f8c */

/* 0x004b5f94 begin_scene(D): SetLightState(D3DLIGHTSTATE_MATERIAL 1, +0x4c) (result dropped),
   BeginScene (returned) */
uint32_t pf2_d3d_begin_scene(Pf2Env* e, uint32_t d) {
    PF2_C2(e, PF2_IDEV2, 0x64, R32(e, d + 0x28), 1, R32(e, d + 0x4c));
    return PF2_C0(e, PF2_IDEV2, 0x28, R32(e, d + 0x28));
}

/* 0x004b5fb8 end_scene(D): EndScene */
uint32_t pf2_d3d_end_scene(Pf2Env* e, uint32_t d) { return PF2_C0(e, PF2_IDEV2, 0x2c, R32(e, d + 0x28)); }

/* 0x004b5fc8 apply_render_states(D): no viewport +0x2c -> 1. BeginScene (failure: its result);
   SetRenderState from the cache: SHADEMODE 9 [0xc84f74], TEXTUREPERSPECTIVE 4 [0xc84f8c],
   ZENABLE 7 [0xc84f94], ZWRITEENABLE 0xe [0xc84f98], ZFUNC 0x17 [0xc84f9c], FILLMODE 8
   [0xc84f78], TEXTUREMAPBLEND 0x1a [0xc84f88], SPECULARENABLE 0x1d [0xc84f90], ANTIALIAS 2
   [0xc84f84], CULLMODE 0x16 = 1 (none), TEXTUREMAG 0x11 and TEXTUREMIN 0x12 [0xc84fa4], DITHERENABLE
   0x15 [0xc84fa0], ALPHABLENDENABLE 0x1b [0xc84fa8], COLORKEYENABLE 0x29 = texture 0's color key
   count [0x00a2dc4c] > 0 ? 0 : 1; EndScene (returned). The cache values are the ones 0x004b5bf0
   sets; the state names are Direct3D 5's numbers. */
uint32_t pf2_d3d_apply_render_states(Pf2Env* e, uint32_t d) {
    static const uint32_t tab[][2] = {{9, 0xc84f74}, {4, 0xc84f8c}, {7, 0xc84f94}, {0xe, 0xc84f98},
                                      {0x17, 0xc84f9c}, {8, 0xc84f78}, {0x1a, 0xc84f88}, {0x1d, 0xc84f90},
                                      {2, 0xc84f84}};
    if (R32(e, d + 0x2c) == 0) return 1;
    uint32_t r = PF2_C0(e, PF2_IDEV2, 0x28, R32(e, d + 0x28));
    if (r) return r;
    for (unsigned i = 0; i < sizeof tab / sizeof tab[0]; i++)
        PF2_C2(e, PF2_IDEV2, 0x5c, R32(e, d + 0x28), tab[i][0], R32(e, tab[i][1]));
    PF2_C2(e, PF2_IDEV2, 0x5c, R32(e, d + 0x28), 0x16, 1);
    PF2_C2(e, PF2_IDEV2, 0x5c, R32(e, d + 0x28), 0x11, R32(e, 0xc84fa4));
    PF2_C2(e, PF2_IDEV2, 0x5c, R32(e, d + 0x28), 0x12, R32(e, 0xc84fa4));
    PF2_C2(e, PF2_IDEV2, 0x5c, R32(e, d + 0x28), 0x15, R32(e, 0xc84fa0));
    PF2_C2(e, PF2_IDEV2, 0x5c, R32(e, d + 0x28), 0x1b, R32(e, 0xc84fa8));
    PF2_C2(e, PF2_IDEV2, 0x5c, R32(e, d + 0x28), 0x29, (int32_t)R32(e, 0x00a2dc4cu) > 0 ? 0 : 1);
    return PF2_C0(e, PF2_IDEV2, 0x2c, R32(e, d + 0x28));
}
