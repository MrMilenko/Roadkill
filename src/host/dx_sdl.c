/*
 * dx_sdl.c: the window, renderer and texture the DirectDraw host presents into (dx_sdl.h has
 * the contract with the Win32 host). Port code, not from the original: stands in for the
 * primary surface's trip to the screen (IDirectDrawSurface::Blt to the primary, Flip) and for
 * the display mode (SetDisplayMode, SetCooperativeLevel).
 */
#include "dx_sdl.h"
#include "host/dialogs/dialogs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qol/display.h"
#include "rt.h"
#include "host/settings/settings.h"

void win32_keys_frame(int frame);   /* win32_keys.c: scripted keys, TM2_SHOT_EVERY */

/* the Win32 host's main window (win32_host.h, win32_user.c); this weak stand in answers NULL when
 * that host is not linked in (tools/rt/test_dx.c) */
__attribute__((weak)) struct SDL_Window* win32_sdl_window(void) { return NULL; }

static struct {
    SDL_Window* win;
    SDL_Renderer* ren;
    SDL_Texture* tex;
    int own_window;
    int tex_w, tex_h, tex_565;
    int mode_w, mode_h, full;       /* the requested mode (0: none set, windowed 640 x 480) */
    int shown_w, shown_h;           /* the size the window was last fitted to */
    int scale, fmt565, want_full;
    uint32_t hwnd;
    int frames, shot_frame;
    const char* shot;
} S = {.scale = 0};

static void init_env(void)
{
    if (S.scale) return;
    int sc = settings_int("display.scale", 2);   /* settings.h: roadkill.ini, TM2_SCALE */
    S.scale = sc > 0 ? sc : 2;
    S.fmt565 = settings_int("debug.pixel", 565) != 555;
    S.want_full = settings_bool("display.fullscreen", 0);
    S.shot = settings_str("debug.shot", NULL);
    S.shot_frame = settings_int("debug.shot_frame", 100);
}

int dx_sdl_scale(void) { init_env(); return S.scale; }

/* the window's size for a w x h picture: display.window_width / _height when set, else the scale */
static int win_w(int w) { int v = settings_int("display.window_width", 0); return v > 0 ? v : w * S.scale; }
static int win_h(int h) { int v = settings_int("display.window_height", 0); return v > 0 ? v : h * S.scale; }
int dx_sdl_is_565(void) { init_env(); return S.fmt565; }

void dx_sdl_client_size(int* w, int* h)
{
    *w = S.mode_w ? S.mode_w : (S.shown_w ? S.shown_w : 640);
    *h = S.mode_h ? S.mode_h : (S.shown_h ? S.shown_h : 480);
}

void dx_sdl_set_window(SDL_Window* w, uint32_t hwnd)
{
    init_env();
    if (S.ren && S.win != w) {
        if (S.tex) SDL_DestroyTexture(S.tex);
        S.tex = NULL;
        S.ren = NULL;   /* belongs to the old window */
        if (S.own_window) SDL_DestroyWindow(S.win);
    }
    S.win = w;
    S.own_window = 0;
    S.shown_w = S.shown_h = 0;
    if (hwnd) S.hwnd = hwnd;
}

void dx_sdl_set_hwnd(uint32_t hwnd) { S.hwnd = hwnd; }

SDL_Window* dx_sdl_window(void)
{
    init_env();
    if (S.win) return S.win;
    if ((S.win = win32_sdl_window()) != NULL) return S.win;
    if (!SDL_WasInit(SDL_INIT_VIDEO) && SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "tm2 host: SDL video: %s\n", SDL_GetError());
        return NULL;
    }
    for (Uint32 id = 1; id <= 32 && !S.win; id++) S.win = SDL_GetWindowFromID(id);
    if (S.win) return S.win;
    int w, h;
    dx_sdl_client_size(&w, &h);
    S.win = SDL_CreateWindow("Twisted Metal 2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w(w),
                             win_h(h), SDL_WINDOW_RESIZABLE);
    if (!S.win) { fprintf(stderr, "tm2 host: SDL window: %s\n", SDL_GetError()); return NULL; }
    S.own_window = 1;
    S.shown_w = w;
    S.shown_h = h;
    return S.win;
}

SDL_Renderer* dx_sdl_renderer(void)
{
    if (S.ren) return S.ren;
    SDL_Window* w = dx_sdl_window();
    if (!w) return NULL;
    S.ren = SDL_GetRenderer(w);
    if (!S.ren) {
        Uint32 fl = settings_bool("display.vsync", 1) ? SDL_RENDERER_PRESENTVSYNC : 0;
        S.ren = SDL_CreateRenderer(w, -1, SDL_RENDERER_ACCELERATED | fl);
        if (!S.ren) S.ren = SDL_CreateRenderer(w, -1, 0);
        if (!S.ren) S.ren = SDL_CreateRenderer(w, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!S.ren) fprintf(stderr, "tm2 host: SDL renderer: %s\n", SDL_GetError());
    return S.ren;
}

/* fits the window to w x h (scaled) unless full screen; the renderer's logical size is w x h */
static void fit(int w, int h)
{
    if (!S.win || (w == S.shown_w && h == S.shown_h)) return;
    S.shown_w = w;
    S.shown_h = h;
    if (S.full) SDL_SetWindowFullscreen(S.win, SDL_WINDOW_FULLSCREEN_DESKTOP);
    else {
        SDL_SetWindowFullscreen(S.win, 0);
        /* display.widescreen (host/qol/display.h): the window keeps the wide shape, a 4:3 picture
         * (the menus) is shown pillarboxed in it */
        int ww = w, wa = qol_display_wide_width(4 * h / 3, h);
        if (qol_display_aspect() > 0 && wa > ww) ww = wa;
        SDL_SetWindowSize(S.win, win_w(ww), win_h(h));
    }
    if (S.ren) SDL_RenderSetLogicalSize(S.ren, w, h);
}

void dx_sdl_set_mode(int w, int h, int fullscreen)
{
    init_env();
    S.mode_w = w;
    S.mode_h = h;
    S.full = w && fullscreen && S.want_full;
    /* a mode is applied at once; RestoreDisplayMode (w 0) only at the next present, so the
     * RestoreDisplayMode + SetDisplayMode pair of 0x004b4c80 does not leave full screen between */
    S.shown_w = S.shown_h = 0;
    if (S.win && w) fit(w, h);
}

/* shows tw x th 16 bit pixels (pitch bytes) as the game's w x h display: the same size but for
 * display.internal_scale (host/qol/display.h), whose picture is larger than the game's */
static void present_pixels(const void* pixels, int tw, int th, int pitch, int w, int h)
{
    init_env();
    if (w <= 0 || h <= 0) return;
    SDL_Renderer* r = dx_sdl_renderer();
    if (!r) return;
    if (w != S.shown_w || h != S.shown_h) fit(w, h);
    else if (!S.full && (win_w(w) != w || win_h(h) != h)) {
        /* someone (MoveWindow of the Win32 host) set the window to the unscaled size: scale it */
        int ww, wh;
        SDL_GetWindowSize(S.win, &ww, &wh);
        if (ww == w && wh == h) SDL_SetWindowSize(S.win, win_w(w), win_h(h));
    }
    if (!S.tex || S.tex_w != tw || S.tex_h != th || S.tex_565 != S.fmt565) {
        if (S.tex) SDL_DestroyTexture(S.tex);
        S.tex = SDL_CreateTexture(r, S.fmt565 ? SDL_PIXELFORMAT_RGB565 : SDL_PIXELFORMAT_RGB555,
                                  SDL_TEXTUREACCESS_STREAMING, tw, th);
        if (!S.tex) { fprintf(stderr, "tm2 host: SDL texture: %s\n", SDL_GetError()); return; }
        S.tex_w = tw;
        S.tex_h = th;
        S.tex_565 = S.fmt565;
        SDL_RenderSetLogicalSize(r, w, h);
    }
    if (SDL_GetWindowFlags(S.win) & SDL_WINDOW_HIDDEN) SDL_ShowWindow(S.win);
    /* display.widescreen: the window changes shape after the picture's size was set: the
     * renderer's logical size again, so a 4:3 picture is centred in the wide window */
    if (qol_display_aspect() > 0) SDL_RenderSetLogicalSize(r, w, h);
    SDL_UpdateTexture(S.tex, NULL, pixels, pitch);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);
    SDL_RenderCopy(r, S.tex, NULL, NULL);
    w32dlg_overlay(r, 1);
    S.frames++;
    win32_keys_frame(S.frames);   /* scripted keys and TM2_SHOT_EVERY (win32_keys.c) */
    if (S.shot && (S.frames == S.shot_frame || S.shot_frame < 0)) {   /* -1: every frame, the file keeps the last */
        if (dx_sdl_screenshot(S.shot) == 0 && S.shot_frame >= 0) fprintf(stderr, "tm2 host: frame %d -> %s\n", S.frames, S.shot);
    }
    SDL_RenderPresent(r);
}

void dx_sdl_present(uint32_t px, int w, int h, int pitch) { present_pixels(rt_ptr(px), w, h, pitch, w, h); }

void dx_sdl_present_host(const void* pixels, int tw, int th, int pitch, int w, int h)
{
    present_pixels(pixels, tw, th, pitch, w, h);
}

/* the last frame again, with the dialogs' overlay (a modal dialog while the game presents nothing) */
void dx_sdl_redraw(void)
{
    SDL_Renderer* r = S.ren;
    if (!r || !S.tex) return;
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);
    SDL_RenderCopy(r, S.tex, NULL, NULL);
    w32dlg_overlay(r, 0);
    SDL_RenderPresent(r);
}

int dx_sdl_screenshot(const char* path)
{
    SDL_Renderer* r = S.ren;
    if (!r || !S.tex) return -1;
    /* re-draw the last frame and read it back at the renderer's output size */
    SDL_RenderClear(r);
    SDL_RenderCopy(r, S.tex, NULL, NULL);
    w32dlg_overlay(r, 0);
    int ow, oh;
    if (SDL_GetRendererOutputSize(r, &ow, &oh) != 0) return -1;
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, ow, oh, 24, SDL_PIXELFORMAT_BGR24);
    if (!s) return -1;
    /* display.widescreen: a 4:3 picture in the wide window sits in a viewport, which
     * SDL_RenderReadPixels reads relative to: read the whole window instead */
    int lw = 0, lh = 0;
    if (qol_display_aspect() > 0) {
        SDL_RenderGetLogicalSize(r, &lw, &lh);
        SDL_RenderSetLogicalSize(r, 0, 0);
        SDL_RenderSetViewport(r, NULL);
    }
    int rc = SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_BGR24, s->pixels, s->pitch);
    if (lw > 0) SDL_RenderSetLogicalSize(r, lw, lh);
    if (rc == 0) rc = SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
    return rc;
}

static void rt_bind_host_dx_sdl(void) {}

RT_BIND_AREA(host_dx_sdl)
