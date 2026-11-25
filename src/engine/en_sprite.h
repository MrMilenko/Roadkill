/*
 * en_sprite.h: 2D sprite helpers of TM2.EXE (PS1 grutils / font names), ported from the
 * machine code:
 *
 *   0x00496d04 grutilsParseSpriteRects (EAX node, EDX out, EBX max): for a type 0 mesh node
 *     (else 0), n = min(max (unsigned), polygon count +0x10); per polygon i < n an 8 byte rect
 *     {u8 u, u8 v, u8 w, u8 h, u16 page} from its uvs (after the color words): u = u0, v = v0,
 *     w = u2 - u3, h = v2 - v1 (bytes), page = the word after 3 uvs (uv count & 0x7f == 3) or
 *     after 4; returns n
 *   0x00496da0 grutilsParse2DSprite: the same, then w += 1, h += 1 per rect
 *   0x00496dcc fontSpritePrintCenteredXY (EAX value, EDX two 0x18 byte SPRT records, EBX the
 *     digit rects): value < 10: record 0 = digit value (u +0x10, v +0x11, w +0x0c, h +0x0e,
 *     u + w +0x12, v + h +0x13, page +0x14), AddPrim(record 0); else value % 100 when >= 100,
 *     record 0 = tens, AddPrim, record 1 (+0x18) = ones, AddPrim
 *   0x00496ee8 (no Ghidra entry) the same number print without the +0x12 / +0x13 / page fields;
 *     record 1's x (+0x20) = record 0's x + w + 1 (u16)
 *   0x00496fac (no Ghidra entry) returns [0x00a36c54]
 * Negative values index before the digit table (as the original).
 */
#ifndef TM2_EN_SPRITE_H
#define TM2_EN_SPRITE_H

#include <stdint.h>

typedef struct {
    uint8_t* (*mem)(void* user, uint32_t addr);
    void* user;
    void (*add_prim)(void* user, uint32_t prim);   /* AddPrim 0x004967f4 */
} EnSpriteEnv;

int32_t en_sprite_parse_rects(EnSpriteEnv* e, uint32_t node, uint32_t out, uint32_t max);
int32_t en_sprite_parse_2d(EnSpriteEnv* e, uint32_t node, uint32_t out, uint32_t max);
void en_sprite_print_number(EnSpriteEnv* e, int32_t value, uint32_t prim, uint32_t rects);
void en_sprite_print_number2(EnSpriteEnv* e, int32_t value, uint32_t prim, uint32_t rects);
uint32_t en_sprite_get_a36c54(EnSpriteEnv* e);

/* 0x004960bc Load4BitClut (EAX path, EDX &w, EBX &h): opens the file (-1: w = h = 0, returns 0),
 * reads u16 w - 1 and u16 h - 1, w and h out, takes the next of 5 0x8000 byte buffers at
 * 0x00a05ba4 (counter byte 0x00a2dba4; all used: closes, w = h = 0, returns 0), reads w * h
 * bytes into it, closes, returns the buffer. File I/O is platform: callbacks. */
typedef struct {
    uint32_t (*open)(void* user, uint32_t path);               /* 0x00498b38, -1 on failure */
    uint32_t (*read)(void* user, uint32_t h, uint8_t* dst, uint32_t n);  /* 0x00498bb8 (ReadFile) */
    void (*close)(void* user, uint32_t h);                     /* 0x00498cb0 */
} EnFileIo;
uint32_t en_load_4bit_clut(EnSpriteEnv* e, const EnFileIo* io, uint32_t path, uint32_t w_out, uint32_t h_out);

/* 0x00495b8c (AL r, DL g, BL b): full screen -> 0x004ad624(r, g, b) (renderer); split screen ->
 * outline of viewport 0 (0x0049392c: x0, y0, x1, y1) as four 0x40 line primitives (color r, g,
 * b; x0 y0 - x1 y0, x1 y0 - x1 y1, x0 y1 - x1 y1, x0 y0 - x0 y1, i16 at +8 / +0xa / +0xc / +0xe)
 * to AddPrim. ORIGINAL QUIRK: nothing is drawn when the viewport's x0 is 0. The primitive is a
 * stack record in the original; prim[0..3] (its tag) is not written. */
typedef struct {
    int (*split)(void* user);                                  /* screen_is_split 0x004958ac */
    void (*clear_color)(void* user, int r, int g, int b);      /* 0x004ad624 */
    void (*viewport)(void* user, int32_t out[4]);              /* 0x0049392c(0, out) */
    void (*add_prim)(void* user, const uint8_t* prim);         /* AddPrim 0x004967f4 */
} EnOutlineEnv;
void en_viewport_outline(EnOutlineEnv* e, void* user, uint8_t r, uint8_t g, uint8_t b);

#endif /* TM2_EN_SPRITE_H */
