/*
 * cb_font.h: two font helpers of TM2.EXE next to the ported font code (src/game/font.c).
 *
 *   0x00451bf0 cb_font_char_width      width of one character: 10 and 13 give 0, space the space
 *                                      width (data +0x11C); anything else its glyph's width
 *                                      (font_glyph, u8 at data + 6 * glyph + 2); plus the spacing.
 *   0x00451f28 cb_font_print_centered  cursor = (x - (font_string_width >> 1), y) (arithmetic
 *                                      shift), then font_print; returns what font_print returns.
 *                                      PS1 fontPrintCenteredXY.
 *
 * Verified against the original under emulation (tools/emu/check_combat_font.sh).
 */
#ifndef TM2_CB_FONT_H
#define TM2_CB_FONT_H

#include <stdint.h>

#include "game/font.h"

int32_t cb_font_char_width(const TM2Font* f, uint8_t c);
int32_t cb_font_print_centered(const TM2Font* f, TM2FontPrint* p, int32_t x, int32_t y, const uint8_t* s);

#endif /* TM2_CB_FONT_H */
