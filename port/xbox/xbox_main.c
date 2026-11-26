/*
 * xbox_main.c: Roadkill's entry point on the original Xbox (port code). The XBE's folder is D:.
 * stderr goes to D:\roadkill.log; the game runs as on the desktop (src/app/tm2.c's main, renamed
 * roadkill_main by the Makefile); when it ends, or stops, the console goes back to the dashboard.
 */
#include <xtl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <SDL.h>

int roadkill_main(int argc, char** argv);

/* the kernel's memory counters (xboxkrnl MmQueryStatistics; GlobalMemoryStatus read wrong here) */
typedef struct {
    ULONG Length, TotalPhysicalPages, AvailablePages, VirtualMemoryBytesCommitted, VirtualMemoryBytesReserved,
        CachePagesCommitted, PoolPagesCommitted, StackPagesCommitted, ImagePagesCommitted;
} RkMmStats;
LONG __stdcall MmQueryStatistics(RkMmStats* s);

static void log_memory(const char* what)
{
    RkMmStats m;
    memset(&m, 0, sizeof m);
    m.Length = sizeof m;
    if (MmQueryStatistics(&m) < 0) { rk_log("%s: MmQueryStatistics failed", what); return; }
    rk_log("%s: %u KB free of %u KB (committed %u KB, pool %u KB, stacks %u KB, image %u KB)", what,
           (unsigned)m.AvailablePages * 4, (unsigned)m.TotalPhysicalPages * 4,
           (unsigned)(m.VirtualMemoryBytesCommitted >> 10), (unsigned)m.PoolPagesCommitted * 4,
           (unsigned)m.StackPagesCommitted * 4, (unsigned)m.ImagePagesCommitted * 4);
}

void rk_log(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "roadkill: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(ap);
}

void rk_fatal(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "roadkill: stopped: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(ap);
    Sleep(2000);
    XLaunchNewImage(NULL, NULL);   /* back to the dashboard */
}

/* compat/rk_xbox.h: the window keeps the console's video mode size */
struct SDL_Window;
void rk_SDL_SetWindowSize(struct SDL_Window* w, int x, int y) { (void)w; (void)x; (void)y; }
void rk_SDL_SetWindowPosition(struct SDL_Window* w, int x, int y) { (void)w; (void)x; (void)y; }
int rk_SDL_SetWindowFullscreen(struct SDL_Window* w, unsigned flags) { (void)w; (void)flags; return 0; }

/* The pad as keys, for what the game does only by keyboard. While driving (the game's car update
 * ran in the last quarter second: rt_world_updates) the pad is the game's joystick and only
 * START presses a key, Esc, the original's pause. In the menus and the pause screen: the d-pad is
 * the arrow keys, A / START Enter, B / BACK Esc. */
#include "rt/rt.h"
void win32_keys_virtual(int sc, int down);
void xbox_pad_keys(void)
{
    static SDL_GameController* pad;
    static const struct { SDL_GameControllerButton b; SDL_Scancode sc; } menu[] = {
        {SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_SCANCODE_UP}, {SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_SCANCODE_DOWN},
        {SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_SCANCODE_LEFT}, {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_SCANCODE_RIGHT},
        {SDL_CONTROLLER_BUTTON_A, SDL_SCANCODE_RETURN}, {SDL_CONTROLLER_BUTTON_START, SDL_SCANCODE_RETURN},
        {SDL_CONTROLLER_BUTTON_B, SDL_SCANCODE_ESCAPE}, {SDL_CONTROLLER_BUTTON_BACK, SDL_SCANCODE_ESCAPE},
    };
    static const struct { SDL_GameControllerButton b; SDL_Scancode sc; } driving[] = {
        {SDL_CONTROLLER_BUTTON_START, SDL_SCANCODE_ESCAPE},
    };
    static uint8_t held[SDL_NUM_SCANCODES];
    static uint32_t last_world, last_world_ms;
    uint8_t now[SDL_NUM_SCANCODES];
    size_t i;
    int drive;
    uint32_t t = SDL_GetTicks();
    if (rt_world_updates != last_world) { last_world = rt_world_updates; last_world_ms = t; }
    drive = last_world_ms && t - last_world_ms < 250;
    if (!SDL_WasInit(SDL_INIT_GAMECONTROLLER)) return;
    if (!pad || !SDL_GameControllerGetAttached(pad)) {
        int j;
        pad = NULL;
        for (j = 0; j < SDL_NumJoysticks() && !pad; j++)
            if (SDL_IsGameController(j)) pad = SDL_GameControllerOpen(j);
        if (!pad) return;
    }
    SDL_GameControllerUpdate();
    memset(now, 0, sizeof now);
    if (drive) {
        for (i = 0; i < sizeof driving / sizeof driving[0]; i++)
            if (SDL_GameControllerGetButton(pad, driving[i].b)) now[driving[i].sc] = 1;
    } else {
        for (i = 0; i < sizeof menu / sizeof menu[0]; i++)
            if (SDL_GameControllerGetButton(pad, menu[i].b)) now[menu[i].sc] = 1;
    }
    /* a key held when the mode changes is let go (a held A doesn't stay Enter into the match) */
    for (i = 0; i < SDL_NUM_SCANCODES; i++)
        if (now[i] != held[i]) { win32_keys_virtual((int)i, now[i]); held[i] = now[i]; }
}

/* every 5 s: the free memory, so a log that ends in a crash shows the trend before it */
static DWORD WINAPI memory_watch(LPVOID p)
{
    (void)p;
    for (;;) {
        Sleep(5000);
        log_memory("memory");
    }
}

void __cdecl main(void)
{
    static char* argv[] = {"D:\\default.xbe", NULL};
    freopen("D:\\roadkill.log", "w", stderr);
    setvbuf(stderr, NULL, _IONBF, 0);   /* every line reaches the file, even when the console crashes */
    rk_log("starting");
    log_memory("memory at start");
    {
        DWORD id;
        HANDLE h = CreateThread(NULL, 0x4000, memory_watch, NULL, 0, &id);
        if (h) CloseHandle(h);
    }
    /* SDL's video up first, so the screen mode is the game's 480 lines (xbox_screen.c) */
    {
        void xbox_screen_mode(void);
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) xbox_screen_mode();
        else rk_log("SDL video: %s", SDL_GetError());
    }
    /* the console's screen is the game's 640 x 480: no window scale (roadkill.ini can't change it) */
    rk_setenv("TM2_SCALE", "1", 1);
    roadkill_main(1, argv);
    rk_log("the game ended");
    fflush(stderr);
    XLaunchNewImage(NULL, NULL);
}
