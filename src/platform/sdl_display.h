/*
 * sdl_display.h: TM2Display on SDL2. The game draws into a 16 bit buffer (555 or 565, as the
 * original's DirectDraw surface); present uploads it to a streaming texture scaled to the window.
 * Port code, not from the original (docs/DIRECTX.md lists what it stands in for).
 */
#ifndef TM2_SDL_DISPLAY_H
#define TM2_SDL_DISPLAY_H

#include "display.h"

/* Opens a window (hidden when shot is set) with a width x height 16 bit screen. 0 on success. */
int sdl_display_open(TM2Display* d, int width, int height, int format_565, const char* title, int hidden);
void sdl_display_close(TM2Display* d);
/* Writes the current screen as a 24 bit BMP. 0 on success. */
int sdl_display_save_bmp(TM2Display* d, const char* path);

#endif /* TM2_SDL_DISPLAY_H */
