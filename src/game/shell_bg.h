/*
 * shell_bg.h: full screen TIM backgrounds of the front end, ported from TM2.EXE
 *   0x0046f784 bg_tim_load: reads the file into the buffer at 0x0058a998, parses it, converts its
 *              pixels to the display format in place
 *   0x0046f7c4 bg_tim_draw: locks the screen and copies the image to its top left corner
 * Verified against the original under emulation (tools/emu/check_shell_bg.sh).
 */
#ifndef TM2_SHELL_BG_H
#define TM2_SHELL_BG_H

#include <stddef.h>
#include <stdint.h>

#include "../platform/display.h"

/* buf holds the TIM file (the original's 0x0058a998, up to 0x96014 bytes); converts it in place.
 * Returns 0, or -1 if it is not a TIM this code reads. */
int bg_tim_load(uint8_t* buf, size_t size, int to_565);

/* Draws the converted TIM at the screen's top left. (The original returns what the unlock wrapper
 * leaves in eax; nothing reads it.) */
void bg_tim_draw(const uint8_t* buf, size_t size, TM2Display* d);

#endif /* TM2_SHELL_BG_H */
