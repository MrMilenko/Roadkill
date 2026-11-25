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

/* the pad in the menus: the d-pad is the arrow keys, A Enter, B / Back Esc, Start Enter. The
 * game reads the pad as its joystick in a match (input.controller, on by default here). */
void win32_keys_virtual(int sc, int down);
void xbox_pad_keys(void)
{
    static SDL_GameController* pad;
    static const struct { SDL_GameControllerButton b; SDL_Scancode sc; } map[] = {
        {SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_SCANCODE_UP}, {SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_SCANCODE_DOWN},
        {SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_SCANCODE_LEFT}, {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_SCANCODE_RIGHT},
        {SDL_CONTROLLER_BUTTON_A, SDL_SCANCODE_RETURN}, {SDL_CONTROLLER_BUTTON_START, SDL_SCANCODE_RETURN},
        {SDL_CONTROLLER_BUTTON_B, SDL_SCANCODE_ESCAPE}, {SDL_CONTROLLER_BUTTON_BACK, SDL_SCANCODE_ESCAPE},
    };
    static uint8_t held[SDL_NUM_SCANCODES];
    uint8_t now[SDL_NUM_SCANCODES];
    size_t i;
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
    for (i = 0; i < sizeof map / sizeof map[0]; i++)
        if (SDL_GameControllerGetButton(pad, map[i].b)) now[map[i].sc] = 1;
    for (i = 0; i < sizeof map / sizeof map[0]; i++) {
        SDL_Scancode sc = map[i].sc;
        if (now[sc] != held[sc]) { win32_keys_virtual(sc, now[sc]); held[sc] = now[sc]; }
    }
}

void __cdecl main(void)
{
    static char* argv[] = {"D:\\default.xbe", NULL};
    freopen("D:\\roadkill.log", "w", stderr);
    rk_log("starting");
    {
        MEMORYSTATUS ms;
        GlobalMemoryStatus(&ms);
        rk_log("memory: %u KB total, %u KB free", (unsigned)(ms.dwTotalPhys >> 10), (unsigned)(ms.dwAvailPhys >> 10));
    }
    /* the console's screen is the game's 640 x 480: no window scale (roadkill.ini can't change it) */
    rk_setenv("TM2_SCALE", "1", 1);
    roadkill_main(1, argv);
    rk_log("the game ended");
    fflush(stderr);
    XLaunchNewImage(NULL, NULL);
}
