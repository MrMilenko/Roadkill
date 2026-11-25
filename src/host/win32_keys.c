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

typedef struct { int frame, hold; SDL_Scancode sc; int state; int down_frame; } Press;   /* state 0 wait, 1 down, 2 done */
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
        SDL_Scancode sc = key_of(f1);
        if (sc == SDL_SCANCODE_UNKNOWN) { fprintf(stderr, "tm2 host: TM2_KEYS: unknown key %s\n", f1); continue; }
        presses[npress++] = (Press){atoi(item), f2 ? atoi(f2) : 4, sc, 0, 0};
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
        if (p->state == 0 && frame >= p->frame) { push(p->sc, 1); p->state = 1; p->down_frame = frame; }
        else if (p->state == 1 && frame >= (int)p->down_frame + p->hold) { push(p->sc, 0); p->state = 2; }
    }
}

void win32_keys_frame(int frame)
{
    parse();
    last_frame = frame;
    last_ms = SDL_GetTicks();
    schedule(frame);
    shot(frame);
}

/* called from PeekMessageA: presses and releases come due by the time clock too */
void win32_keys_tick(void)
{
    parse();
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
