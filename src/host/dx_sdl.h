/*
 * dx_sdl.h: the display side of the host (DirectDraw on SDL2) and its contract with the Win32
 * host (window creation, message loop, GDI). Port code, not from the original.
 *
 * The window
 *   The Win32 host owns the window (CreateWindowExA -> SDL_CreateWindow) and the event loop
 *   (PeekMessageA / GetMessageA -> SDL_PollEvent). It tells the display which SDL window is the
 *   game's with dx_sdl_set_window() right after creating it. If it never does, the display looks
 *   for any SDL window (SDL_GetWindowFromID 1..32) and, failing that, creates its own window the
 *   first time something is presented. The display creates the renderer (SDL_GetRenderer first,
 *   so a renderer the Win32 host made is reused) and a streaming texture of the mode's size.
 *
 * Sizes and coordinates
 *   The game's display is W x H (640 x 480 normally). The window is W * scale x H * scale
 *   (dx_sdl_scale(): TM2_SCALE, default 2) and the renderer has the logical size W x H, so SDL
 *   mouse events come in game coordinates. The Win32 host should answer GetClientRect with
 *   dx_sdl_client_size() (not the scaled window size), ClientToScreen / ScreenToClient as the
 *   identity (the client area's origin is (0, 0) in "screen" coordinates: the primary surface is
 *   the client area, so the game's windowed Blt to the client rect lands at 0, 0), and should not
 *   resize the window itself on MoveWindow / SetWindowPos (the display sizes it on mode changes).
 *   GetSystemMetrics(SM_CXSCREEN / SM_CYSCREEN) may answer the current mode size.
 *
 * GDI on surfaces
 *   IDirectDrawSurface::GetDC hands out a fake HDC (a fake area address). A GDI host that wants
 *   to draw text into a surface (TextOutA / ExtTextOutA on that HDC) asks dx_sdl_hdc_surface()
 *   for the surface's game memory: 16 bit pixels, pitch in bytes, 565 or 555 (dx_sdl_is_565()).
 *   GetDeviceCaps(hdc, BITSPIXEL) should answer 16.
 *
 * Full screen
 *   SetCooperativeLevel(DDSCL_FULLSCREEN) + SetDisplayMode go full screen (desktop, scaled) only
 *   with TM2_FULLSCREEN=1; by default the game stays in a window.
 *
 * Screenshots
 *   dx_sdl_screenshot(path) writes the last presented frame (a 24 bit BMP read back from the
 *   renderer). TM2_SHOT=<file.bmp> writes the frame presented number TM2_SHOT_FRAME (default 100)
 *   and keeps running.
 */
#ifndef TM2_HOST_DX_SDL_H
#define TM2_HOST_DX_SDL_H

#include <stdint.h>

#include <SDL.h>

/* called by the Win32 host after it created the game's window (hwnd: the HWND it hands out) */
void dx_sdl_set_window(SDL_Window* w, uint32_t hwnd);

/* the window / renderer the display presents into; created on demand (NULL if SDL video fails) */
SDL_Window* dx_sdl_window(void);
SDL_Renderer* dx_sdl_renderer(void);

int dx_sdl_scale(void);
void dx_sdl_client_size(int* w, int* h);   /* the game's display size (640 x 480 before a mode) */
int dx_sdl_is_565(void);                   /* 1: RGB 565 surfaces, 0: 555 (TM2_PIXEL=555) */

/* the surface behind a fake HDC from IDirectDrawSurface::GetDC: 1 when hdc is one */
int dx_sdl_hdc_surface(uint32_t hdc, uint32_t* pixels, int* width, int* height, int* pitch);

int dx_sdl_screenshot(const char* path);   /* 0 on success */

/* ---- used by the DirectDraw host (dx_ddraw.c) ---- */
void dx_sdl_set_mode(int w, int h, int fullscreen);   /* SetDisplayMode / RestoreDisplayMode (w 0) */
void dx_sdl_set_hwnd(uint32_t hwnd);                  /* SetCooperativeLevel's window */
/* shows w x h 16 bit pixels at game address px (pitch bytes) in the window */
void dx_sdl_present(uint32_t px, int w, int h, int pitch);
/* the same for a picture in host memory, tw x th pixels shown as the game's w x h display
 * (display.internal_scale, host/qol/display.h: tw x th is a multiple of w x h) */
void dx_sdl_present_host(const void* pixels, int tw, int th, int pitch, int w, int h);

/* ---- the movie overlay (host/movies/mci_avi.c) ---- */
/* draws a w x h XRGB picture (pitch bytes) over the game's last frame at x, y scaled to dw x dh
 * (the game's display coordinates), presents, and keeps drawing it over the game's presents until
 * dx_sdl_overlay_off() */
void dx_sdl_overlay(const void* xrgb, int w, int h, int pitch, int x, int y, int dw, int dh);
void dx_sdl_overlay_off(void);

#endif
