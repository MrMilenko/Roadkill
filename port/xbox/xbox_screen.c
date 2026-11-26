/*
 * xbox_screen.c: the game's picture scaled to the console's screen (port code; compat/rk_xbox.h
 * routes Roadkill's SDL_RenderSetLogicalSize / SDL_RenderCopy here).
 *
 * SDL2x makes its back buffer the console's video mode (640 x 480 up to 1280 x 720 or 1080i), but
 * with SDL's own logical size the first test builds drew the game 1:1 in the top left corner. So
 * here SDL's logical size stays off; each copy is mapped from the game's coordinates (the logical
 * size Roadkill asked for) to the screen, with the viewport set to the whole screen.
 */
#undef SDL_RenderSetLogicalSize
#undef SDL_RenderGetLogicalSize
#undef SDL_RenderCopy
#include <SDL.h>

static int lw, lh;

static void screen_size(SDL_Renderer* r, int* w, int* h)
{
    SDL_DisplayMode dm;
    (void)r;
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0 && dm.w > 0 && dm.h > 0) { *w = dm.w; *h = dm.h; return; }
    *w = 640;
    *h = 480;
}

int rk_SDL_RenderSetLogicalSize(SDL_Renderer* r, int w, int h)
{
    lw = w;
    lh = h;
    return SDL_RenderSetLogicalSize(r, 0, 0);
}

void rk_SDL_RenderGetLogicalSize(SDL_Renderer* r, int* w, int* h)
{
    (void)r;
    if (w) *w = lw;
    if (h) *h = lh;
}

int rk_SDL_RenderCopy(SDL_Renderer* r, SDL_Texture* t, const SDL_Rect* src, const SDL_Rect* dst)
{
    int sw, sh, gw = lw, gh = lh;
    SDL_Rect full, out;
    float s;
    screen_size(r, &sw, &sh);
    if (gw <= 0 || gh <= 0) { gw = sw; gh = sh; }
    s = (float)sw / gw < (float)sh / gh ? (float)sw / gw : (float)sh / gh;
    if (!dst) { full.x = 0; full.y = 0; full.w = gw; full.h = gh; dst = &full; }
    out.x = (int)((sw - gw * s) / 2 + dst->x * s);
    out.y = (int)((sh - gh * s) / 2 + dst->y * s);
    out.w = (int)(dst->w * s + 0.5f);
    out.h = (int)(dst->h * s + 0.5f);
    {
        SDL_Rect vp = {0, 0, sw, sh};
        SDL_RenderSetViewport(r, &vp);
    }
    return SDL_RenderCopy(r, t, src, &out);
}
