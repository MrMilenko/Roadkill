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

/* SDL2x's window and back buffer are this mode (set by xbox_screen_mode) */
extern SDL_DisplayMode g_XboxDesktopMode;
unsigned long __stdcall XGetVideoFlags(void);
unsigned long __stdcall XGetVideoStandard(void);
#define XC_VIDEO_FLAGS_WIDESCREEN 0x00000001
#define XC_VIDEO_FLAGS_HDTV_480p 0x00000008
#define XC_VIDEO_STANDARD_PAL_I 3

static void screen_size(SDL_Renderer* r, int* w, int* h)
{
    (void)r;
    *w = g_XboxDesktopMode.w > 0 ? g_XboxDesktopMode.w : 640;
    *h = g_XboxDesktopMode.h > 0 ? g_XboxDesktopMode.h : 480;
}

/* The game is 640 x 480: 480p (720 x 480) when the dashboard allows it, else 640 x 480, in place of
 * the dashboard's HD mode, whose larger back buffer only costs memory (1280 x 720 x 32 bit plus
 * its depth buffer: 3.7 MB more). Called after SDL's video is up, before the game's window. PAL
 * consoles keep SDL2x's choice. */
void xbox_screen_mode(void)
{
    if (XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I) return;
    if (XGetVideoFlags() & XC_VIDEO_FLAGS_HDTV_480p) { g_XboxDesktopMode.w = 720; g_XboxDesktopMode.h = 480; }
    else { g_XboxDesktopMode.w = 640; g_XboxDesktopMode.h = 480; }
}

/* the TV's shape: 16:9 for HD modes and a widescreen dashboard setting, else 4:3 */
static float display_aspect(int sw, int sh)
{
    if (sh >= 720) return 16.0f / 9.0f;
    if (sw == 720 && (XGetVideoFlags() & XC_VIDEO_FLAGS_WIDESCREEN)) return 16.0f / 9.0f;
    return 4.0f / 3.0f;
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
    screen_size(r, &sw, &sh);
    if (gw <= 0 || gh <= 0) { gw = sw; gh = sh; }
    {
        /* the picture is 4:3 on the TV: pw x sh pixels of the back buffer, whatever their shape */
        float pw = sw * (4.0f / 3.0f) / display_aspect(sw, sh), sx = pw / gw, sy = (float)sh / gh;
        if (pw > sw) { pw = (float)sw; sx = pw / gw; }
        if (!dst) { full.x = 0; full.y = 0; full.w = gw; full.h = gh; dst = &full; }
        out.x = (int)((sw - pw) / 2 + dst->x * sx);
        out.y = (int)(dst->y * sy);
        out.w = (int)(dst->w * sx + 0.5f);
        out.h = (int)(dst->h * sy + 0.5f);
    }
    {
        SDL_Rect vp = {0, 0, sw, sh};
        SDL_RenderSetViewport(r, &vp);
    }
    return SDL_RenderCopy(r, t, src, &out);
}
