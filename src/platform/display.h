/*
 * display.h: what the game's code needs from the screen, from TM2.EXE's DirectDraw module
 * (0x004b2000..0x004b7000, the display object at 0x00c84840; docs/DIRECTX.md).
 *
 * The PC game renders in software into a locked 16 bit surface: the lock wrapper 0x004b4418
 * stores the surface pointer at 0x00a04674 and its pitch at 0x00a04670 (and the display object
 * +0x60); the renderer is told both (0x004ad82c, 0x004ad760). Here a backend (SDL, or a test)
 * provides the same: a 16 bit pixel buffer, its pitch, its format, lock / unlock and present.
 */
#ifndef TM2_DISPLAY_H
#define TM2_DISPLAY_H

#include <stdint.h>

typedef struct TM2Display TM2Display;

struct TM2Display {
    /* set by lock, cleared by unlock (0x00a04674 / 0x00a04670) */
    uint8_t* pixels;
    int32_t pitch;              /* bytes per row */
    int32_t width, height;      /* display object +0x58 / +0x5c (0x004b5a14 / 0x004b5a10) */
    int format_565;             /* 0 = 555, 1 = 565 (the original's 0x00a2dba5, from DirectDraw masks) */
    /* the backend */
    int (*lock)(TM2Display* d);     /* 0 on success: fills pixels / pitch (0x004b4418 on the offscreen surface) */
    void (*unlock)(TM2Display* d);  /* 0x004b44e4: pixels = NULL */
    void (*present)(TM2Display* d); /* the flip / blit to the window (0x004b5128 / 0x004b52b8) */
    void* user;
};

static inline int display_lock(TM2Display* d) { return d->lock(d); }
static inline void display_unlock(TM2Display* d) { d->unlock(d); d->pixels = 0; }

#endif /* TM2_DISPLAY_H */
