/*
 * rk_xbox.h: included first in every Roadkill file of the Xbox build (port/xbox/Makefile). The
 * XDK's C library is Visual C++ 2003's: the C99 / POSIX names Roadkill uses are mapped here onto
 * it or onto port/xbox/xbox_posix.c. Nothing of the XDK's own headers (xtl.h) comes in, so the
 * game's names stay clear of Win32's macros.
 */
#ifndef RK_XBOX_H
#define RK_XBOX_H

#include <stddef.h>
#include <stdarg.h>

#define strcasecmp  _stricmp
#define strncasecmp _strnicmp
int _stricmp(const char*, const char*);
int _strnicmp(const char*, const char*, size_t);

/* C99's snprintf / vsnprintf (the CRT has only _snprintf, which neither terminates nor counts) */
int rk_snprintf(char* s, size_t n, const char* fmt, ...);
int rk_vsnprintf(char* s, size_t n, const char* fmt, va_list ap);
#define snprintf  rk_snprintf
#define vsnprintf rk_vsnprintf

/* the environment: the console has none; xbox_posix.c keeps one (settings, TM2_* variables) */
char* rk_getenv(const char* name);
int rk_setenv(const char* name, const char* value, int overwrite);
#define getenv rk_getenv
#define setenv rk_setenv

/* files: XAPI wants '\' and full paths ("D:\..."); these turn '/' into '\' first */
#include <stdio.h>
FILE* rk_fopen(const char* path, const char* mode);
#define fopen rk_fopen

/* the screen: SDL2x's window is always the console's video mode (640 x 480 up to 1280 x 720 /
 * 1080i), and its back buffer is made that size. Roadkill sizes its window to the game's 640 x 480
 * on the desktop; here that would leave SDL scaling for a 640 x 480 window while the back buffer
 * stays 1280 x 720 (a quarter of the screen used). So the window keeps its size, and the
 * renderer's logical size (dx_sdl.c) scales the game to the screen, 4:3 in the middle. */
#define SDL_SetWindowSize rk_SDL_SetWindowSize
#define SDL_SetWindowPosition rk_SDL_SetWindowPosition
#define SDL_SetWindowFullscreen rk_SDL_SetWindowFullscreen
/* and SDL's logical size isn't relied on (with SDL2x the game came out 1:1 in the corner of a
 * 1280 x 720 screen): xbox_screen.c keeps the logical size and scales each copy to the console's
 * screen itself, 4:3 centred */
#define SDL_RenderSetLogicalSize rk_SDL_RenderSetLogicalSize
#define SDL_RenderGetLogicalSize rk_SDL_RenderGetLogicalSize
#define SDL_RenderCopy rk_SDL_RenderCopy

/* the log (D:\roadkill.log, also stderr's file) and a stop with a message on the screen */
void rk_log(const char* fmt, ...);
void rk_fatal(const char* fmt, ...);
void rk_tls_release(void);

#endif
