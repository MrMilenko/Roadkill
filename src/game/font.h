/*
 * font.h: the front end's bitmap fonts, ported from TM2.EXE (PS1 decomp: font_measure.c,
 * font_print.c).
 *   0x00451a9c font_string_width: width of the last line of a string in pixels
 *   0x00451f54 font_print: draws a string at the cursor (0x005024d0 / 0x005024d4) as sprites
 *   0x00452404 font_sprite_print: the same through a caller's array of sprite records
 *   0x00452204 / 0x0045222c: font_sprite_print at x, y / centered on x
 *
 * Per font index (the original's tables): data pointer 0x005027f8[font], spacing 0x005025d8[font].
 * Font data: glyph records of 6 bytes from glyph 0 (character '0'), width at +2; +0x105 line
 * height; +0x11c the width of a space.
 * Characters: '.' '!' ',' '\'' '*' use the glyph slots of ';' '\\' '^' '`' '{'; anything below '0'
 * or above '|' (after that) uses glyph 0x2f. 8 = one pixel back, 10 = new line, 13 is skipped;
 * 11 and 12 are drawn as glyph 0x2f (not skipped).
 *
 * Verified against the original under emulation (tools/emu/check_font.sh).
 */
#ifndef TM2_FONT_H
#define TM2_FONT_H

#include <stdint.h>

typedef struct {
    const uint8_t* data;    /* 0x005027f8[font] */
    int32_t spacing;        /* 0x005025d8[font] */
} TM2Font;

/* the glyph a character draws with (0..0x4c) */
int32_t font_glyph(uint8_t c);

int32_t font_string_width(const TM2Font* f, const uint8_t* s);

/* What font_print uses besides the font: the cursor (0x005024d0 x, 0x005024d4 y), the font's
 * texture base (u8 0x005024d8 + 8 * font, v 0x005024dc + 8 * font) and its 24 byte sprite record
 * (0x00502518 + 0x18 * font), filled per glyph:
 *   +0x04..+0x06 u8 color (all 0xff), +0x07 u8 flags (bit 1 cleared), +0x08 i16 x, +0x0a i16 y,
 *   +0x0c u16 w, +0x0e u16 h, +0x10 u8 u, +0x11 u8 v, +0x12 u8 u + w, +0x13 u8 v + h (bytes wrap)
 * and passed to draw (the original's sprite draw 0x004967f4). */
typedef struct {
    int32_t* x;
    int32_t* y;
    uint8_t base_u, base_v;
    uint8_t* sprite;        /* 24 bytes */
    void (*draw)(void* user, uint8_t* sprite);
    void* user;
} TM2FontPrint;

/* Draws s: glyphs with a zero width or height are skipped (no advance); space advances by the
 * space width + spacing; 8 = x - 1; 10 = x back to 0 (not to the start) and y down a line;
 * 13 = y down a line (font_string_width skips it); 11 and 12 draw glyph 0x2f. Returns the number
 * of bytes read (control bytes included); the cursor x is stored back at the end. */
int32_t font_print(const TM2Font* f, TM2FontPrint* p, const uint8_t* s);

/* 0x00452404: one 24 byte record in out per byte of s (control bytes too); each glyph's record
 * starts as a copy of p->sprite, is filled as in font_print and drawn, zero sized glyphs
 * included. The cursor is updated in place (*p->x, *p->y). Returns the bytes read. The original's
 * second argument is never read. */
int32_t font_sprite_print(const TM2Font* f, TM2FontPrint* p, uint8_t* out, const uint8_t* s);
/* 0x00452204: cursor = (x, y), then font_sprite_print */
void font_sprite_print_xy(const TM2Font* f, TM2FontPrint* p, int32_t x, int32_t y, uint8_t* out, const uint8_t* s);
/* 0x0045222c: cursor = (x - (width >> 1), y), width as font_string_width, then font_sprite_print */
void font_sprite_print_centered(const TM2Font* f, TM2FontPrint* p, int32_t x, int32_t y, uint8_t* out, const uint8_t* s);

#endif /* TM2_FONT_H */
