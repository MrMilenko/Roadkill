/*
 * shell.c: tm2shell, the front end growing on the ported code. For now it shows a full screen TIM
 * the way the game's front end does (bg_tim_load / bg_tim_draw, verified), on the SDL display.
 *
 *   tm2shell <TIM> [--565] [--shot out.bmp]
 *
 * Port choices, not the original: a 640 x 480 screen in a window scaled 2x, Esc quits.
 */
#include "platform/sdl_display.h"
#include "game/shell_bg.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t* load_file(const char* path, size_t* size) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* b = n > 0 ? malloc((size_t)n) : NULL;
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f);
    *size = (size_t)n;
    return b;
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: tm2shell <TIM> [--565] [--shot out.bmp]\n"); return 2; }
    int f565 = 0;
    const char* shot = NULL;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--565")) f565 = 1;
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot = argv[++i];
    }
    size_t size;
    uint8_t* tim = load_file(argv[1], &size);
    if (!tim) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    if (bg_tim_load(tim, size, f565) != 0) { fprintf(stderr, "%s: not a TIM this reads\n", argv[1]); return 1; }

    TM2Display d;
    if (sdl_display_open(&d, 640, 480, f565, "tm2shell", shot != NULL) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    bg_tim_draw(tim, size, &d);
    d.present(&d);
    if (shot) {
        int r = sdl_display_save_bmp(&d, shot);
        sdl_display_close(&d);
        return r ? 1 : 0;
    }
    for (int running = 1; running;) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) running = 0;
        }
        d.present(&d);
        SDL_Delay(16);
    }
    sdl_display_close(&d);
    return 0;
}
