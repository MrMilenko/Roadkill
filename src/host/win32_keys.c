/*
 * win32_keys.c: scripted input and periodic screenshots for unattended runs (port code, host
 * only; not from the original).
 *
 *   TM2_KEYS="frame:KEY[:hold],..."  at presented frame `frame` KEY goes down for `hold` frames
 *       (default 4), e.g. "300:RETURN,330:DOWN,360:RETURN". KEY is an SDL scancode name
 *       ("Return", "Down", "A", "F1", "Left Shift" ...; case does not matter) or one of RETURN,
 *       ENTER, ESC, UP, DOWN, LEFT, RIGHT, SPACE, TAB, SHIFT, CTRL, ALT. Each press is pushed as
 *       an SDL_KEYDOWN / SDL_KEYUP event (the message loop turns it into WM_KEYDOWN / UP) and is
 *       seen in the keyboard state every reader uses, host_keyboard_state() (DirectInput's
 *       keyboard, GetAsyncKeyState).
 *       KEY "click/X/Y" is a left mouse click at game pixel (X, Y) instead (button down at
 *       `frame`, up after `hold` frames), e.g. "1100:click/150/120".
 *   TM2_SHOT_EVERY=N  saves the presented frame every N frames as <dir>/NNNNN.bmp, dir =
 *       TM2_SHOT_DIR (default "shots", created).
 *   TM2_KEYS_LOG=1  logs each scripted press and release.
 *
 * Frames are counted by presents (dx_sdl_present calls win32_keys_frame() with its counter);
 * while nothing is presented (a menu waiting for input redraws nothing) the count goes on at 60
 * a second from the last present, checked in PeekMessageA (win32_keys_tick()).
 */
#include "win32_host.h"
#include "host/settings/settings.h"

#include <SDL.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include "host/os/os.h"

typedef struct { int frame, hold; SDL_Scancode sc; int state; int down_frame; int click, mx, my; } Press;   /* state 0 wait, 1 down, 2 done */
static Press presses[256];
static int npress, parsed;
static uint8_t scripted[SDL_NUM_SCANCODES];
static uint8_t merged[SDL_NUM_SCANCODES];
static int shot_every = -1;
static char shot_dir[512];

static SDL_Scancode key_of(const char* s)
{
    static const struct { const char* a; const char* sdl; } al[] = {
        {"RETURN", "Return"}, {"ENTER", "Return"}, {"ESC", "Escape"}, {"ESCAPE", "Escape"}, {"UP", "Up"},
        {"DOWN", "Down"}, {"LEFT", "Left"}, {"RIGHT", "Right"}, {"SPACE", "Space"}, {"TAB", "Tab"},
        {"SHIFT", "Left Shift"}, {"CTRL", "Left Ctrl"}, {"ALT", "Left Alt"},
    };
    for (size_t i = 0; i < sizeof al / sizeof al[0]; i++)
        if (!strcasecmp(s, al[i].a)) return SDL_GetScancodeFromName(al[i].sdl);
    return SDL_GetScancodeFromName(s);
}

static void parse(void)
{
    if (parsed) return;
    parsed = 1;
    const char* k = settings_str("debug.keys", NULL);
    while (k && *k && npress < 256) {
        char item[64];
        size_t l = strcspn(k, ",");
        snprintf(item, sizeof item, "%.*s", (int)(l < 63 ? l : 63), k);
        k += l + (k[l] == ',');
        char* f1 = strchr(item, ':');
        if (!f1) continue;
        *f1++ = 0;
        char* f2 = strchr(f1, ':');
        if (f2) *f2++ = 0;
        if (!strncasecmp(f1, "click/", 6)) {
            int mx = atoi(f1 + 6), my = strchr(f1 + 6, '/') ? atoi(strchr(f1 + 6, '/') + 1) : 0;
            presses[npress++] = (Press){atoi(item), f2 ? atoi(f2) : 4, SDL_SCANCODE_UNKNOWN, 0, 0, 1, mx, my};
            continue;
        }
        SDL_Scancode sc = key_of(f1);
        if (sc == SDL_SCANCODE_UNKNOWN) { fprintf(stderr, "tm2 host: TM2_KEYS: unknown key %s\n", f1); continue; }
        presses[npress++] = (Press){atoi(item), f2 ? atoi(f2) : 4, sc, 0, 0, 0, 0, 0};
    }
    if (settings_int("debug.shot_every", 0) > 0) {
        shot_every = settings_int("debug.shot_every", 0);
        snprintf(shot_dir, sizeof shot_dir, "%s", settings_str("debug.shot_dir", "shots"));
        os_mkdir(shot_dir);
    }
}

static void push(SDL_Scancode sc, int down)
{
    SDL_Event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    ev.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    ev.key.keysym.scancode = sc;
    ev.key.keysym.sym = SDL_GetKeyFromScancode(sc);
    SDL_Window* w = win32_sdl_window();
    ev.key.windowID = w ? SDL_GetWindowID(w) : 0;
    ev.key.timestamp = SDL_GetTicks();
    SDL_PushEvent(&ev);
    scripted[sc] = (uint8_t)down;
    if (settings_bool("debug.keys_log", 0)) fprintf(stderr, "tm2 host: key %s %s\n", SDL_GetScancodeName(sc), down ? "down" : "up");
}

/* a scripted mouse click: the pointer moves there, then the left button */
static void push_mouse(int x, int y, int down)
{
    SDL_Window* w = win32_sdl_window();
    /* the renderer's event watch maps window pixels to its logical size for pushed events too:
     * give it window pixels */
#if SDL_VERSION_ATLEAST(2, 0, 18)
    SDL_Renderer* r = w ? SDL_GetRenderer(w) : NULL;
    if (r) {
        int wx, wy;
        SDL_RenderLogicalToWindow(r, (float)x + 0.5f, (float)y + 0.5f, &wx, &wy);
        x = wx;
        y = wy;
    }
#endif
    SDL_Event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = SDL_MOUSEMOTION;
    ev.motion.windowID = w ? SDL_GetWindowID(w) : 0;
    ev.motion.x = x;
    ev.motion.y = y;
    ev.motion.timestamp = SDL_GetTicks();
    SDL_PushEvent(&ev);
    memset(&ev, 0, sizeof ev);
    ev.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    ev.button.windowID = w ? SDL_GetWindowID(w) : 0;
    ev.button.button = SDL_BUTTON_LEFT;
    ev.button.state = down ? SDL_PRESSED : SDL_RELEASED;
    ev.button.clicks = 1;
    ev.button.x = x;
    ev.button.y = y;
    ev.button.timestamp = SDL_GetTicks();
    SDL_PushEvent(&ev);
    if (settings_bool("debug.keys_log", 0)) fprintf(stderr, "tm2 host: click %d,%d %s\n", x, y, down ? "down" : "up");
}

/* a key held by something other than the keyboard (the Xbox's pad in the menus: port/xbox) */
void win32_keys_virtual(int sc, int down)
{
    if (sc <= 0 || sc >= SDL_NUM_SCANCODES || scripted[sc] == (uint8_t)!!down) return;
    push((SDL_Scancode)sc, !!down);
}

/* the schedule's clock: presented frames, and while nothing is presented (a menu waiting for
 * input redraws nothing) 60 a second on from the last present */
static int last_frame;
static uint32_t last_ms;
static int vframe(void)
{
    uint32_t now = SDL_GetTicks();
    if (!last_ms) last_ms = now;
    return last_frame + (int)((now - last_ms) * 60u / 1000u);
}

static int last_shot;
static void shot(int frame)
{
    if (shot_every <= 0 || frame / shot_every == last_shot / shot_every) return;
    last_shot = frame;
    char path[600];
    snprintf(path, sizeof path, "%s/%05d.bmp", shot_dir, frame - frame % shot_every);
    if (dx_sdl_screenshot(path) != 0) fprintf(stderr, "tm2 host: screenshot %s failed\n", path);
}

static void schedule(int frame)
{
    for (int i = 0; i < npress; i++) {
        Press* p = &presses[i];
        if (p->state == 0 && frame >= p->frame) {
            if (p->click) push_mouse(p->mx, p->my, 1); else push(p->sc, 1);
            p->state = 1;
            p->down_frame = frame;
        } else if (p->state == 1 && frame >= (int)p->down_frame + p->hold) {
            if (p->click) push_mouse(p->mx, p->my, 0); else push(p->sc, 0);
            p->state = 2;
        }
    }
}

void win32_keys_frame(int frame)
{
    /* the count never steps back: after a wait the clock ran ahead of the presents, and a step
     * back would hold a scripted key down until the clock got there again (seconds) */
    static int ahead;
    parse();
    if (last_ms && frame + ahead < vframe()) ahead = vframe() - frame;
    frame += ahead;
    last_frame = frame;
    last_ms = SDL_GetTicks();
    schedule(frame);
    shot(frame);
}

/* called from PeekMessageA: presses and releases come due by the time clock too */
#ifdef ROADKILL_XBOX
void xbox_pad_keys(void);
#endif
void win32_keys_tick(void)
{
    parse();
#ifdef ROADKILL_XBOX
    xbox_pad_keys();
#endif
    int f = vframe();
    if (npress) schedule(f);
    shot(f);   /* a screen that waits redraws nothing: its last frame, numbered by the clock */
}

const uint8_t* host_keyboard_state(int* n)
{
    int nk = 0;
    const Uint8* k = SDL_GetKeyboardState(&nk);
    if (nk > SDL_NUM_SCANCODES) nk = SDL_NUM_SCANCODES;
    memset(merged, 0, sizeof merged);
    if (k) memcpy(merged, k, (size_t)nk);
    for (int i = 0; i < SDL_NUM_SCANCODES; i++) merged[i] |= scripted[i];
    if (n) *n = SDL_NUM_SCANCODES;
    return merged;
}

static void rt_bind_win32_keys(void) { parse(); }

RT_BIND_AREA(win32_keys)
