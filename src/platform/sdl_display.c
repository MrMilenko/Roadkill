/*
 * sdl_display.c: TM2Display on SDL2 (see sdl_display.h).
 */
#include "sdl_display.h"

#include <SDL.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    SDL_Window* win;
    SDL_Renderer* ren;
    SDL_Texture* tex;
    uint8_t* buf;       /* the screen: width * 2 bytes a row */
} SdlBackend;

static int sdl_lock(TM2Display* d) {
    SdlBackend* b = d->user;
    d->pixels = b->buf;
    d->pitch = d->width * 2;
    return 0;
}

static void sdl_unlock(TM2Display* d) { (void)d; }

static void sdl_present(TM2Display* d) {
    SdlBackend* b = d->user;
    SDL_UpdateTexture(b->tex, NULL, b->buf, d->width * 2);
    SDL_RenderClear(b->ren);
    SDL_RenderCopy(b->ren, b->tex, NULL, NULL);
    SDL_RenderPresent(b->ren);
}

int sdl_display_open(TM2Display* d, int width, int height, int format_565, const char* title, int hidden) {
    SdlBackend* b = calloc(1, sizeof *b);
    if (!b) return -1;
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { free(b); return -1; }
    b->win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width * 2, height * 2,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | (hidden ? SDL_WINDOW_HIDDEN : 0));
    if (!b->win) { free(b); return -1; }
    b->ren = SDL_CreateRenderer(b->win, -1, SDL_RENDERER_PRESENTVSYNC);
    if (!b->ren) b->ren = SDL_CreateRenderer(b->win, -1, SDL_RENDERER_SOFTWARE);
    if (!b->ren) { SDL_DestroyWindow(b->win); free(b); return -1; }
    SDL_RenderSetLogicalSize(b->ren, width, height);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    b->tex = SDL_CreateTexture(b->ren, format_565 ? SDL_PIXELFORMAT_RGB565 : SDL_PIXELFORMAT_RGB555,
                               SDL_TEXTUREACCESS_STREAMING, width, height);
    b->buf = calloc((size_t)width * height, 2);
    if (!b->tex || !b->buf) return -1;
    memset(d, 0, sizeof *d);
    d->width = width; d->height = height; d->format_565 = format_565;
    d->lock = sdl_lock; d->unlock = sdl_unlock; d->present = sdl_present;
    d->user = b;
    return 0;
}

void sdl_display_close(TM2Display* d) {
    SdlBackend* b = d->user;
    if (!b) return;
    SDL_DestroyTexture(b->tex);
    SDL_DestroyRenderer(b->ren);
    SDL_DestroyWindow(b->win);
    free(b->buf);
    free(b);
    d->user = NULL;
    SDL_Quit();
}

int sdl_display_save_bmp(TM2Display* d, const char* path) {
    SdlBackend* b = d->user;
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom(b->buf, d->width, d->height, 16, d->width * 2,
                                                        d->format_565 ? SDL_PIXELFORMAT_RGB565 : SDL_PIXELFORMAT_RGB555);
    if (!s) return -1;
    SDL_Surface* rgb = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_RGB24, 0);
    SDL_FreeSurface(s);
    if (!rgb) return -1;
    int r = SDL_SaveBMP(rgb, path);
    SDL_FreeSurface(rgb);
    return r;
}
