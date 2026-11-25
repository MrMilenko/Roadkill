/*
 * gp4_score.h: the score / player list overlay, TM2.EXE 0x00437134 (Ghidra FUN_00437134, no
 * PS1 pair). No arguments.
 *
 * W = screen width 0x004958d8(), q = W * 100 / 0x140, x0 = W - q - 8.
 * Single player (0x004632b0() == 0): u8 0x004f6a38 > 1 -> 0; for each AI car i < 0x0045b078()
 *   (asked again every round), y = 0xf + 0xb i: car 0x0045b080(i) non null with i32 +0x58c > 0:
 *   draw (font 2, W - 100, y) the name of 0x0045bb20(i16 +0x30) from the table 0x004e6870.
 * Network, u8 0x004f6a38 == 1 (score table): sc = game type 0x00474500() == 7; headers "Player"
 *   at (x0, 0xf), "K"/"D" (sc) or "W"/"L" at W - 0x2e and W - 0x18 (font 2, the strings at
 *   0x004e0008..0x004e0015). Per player k < 0x004632c8() (asked every round), y = 0x1a + 0xb k,
 *   idx = 0x00464cdc(k): two numbers, sc: 0x0045b9c4(idx) / 0x0045b9cc(idx) (each called twice:
 *   tens from the first call, units from the second), else i32 +0x04 / +0x00 of the 12 byte
 *   record idx at 0x00624db8; each printed as two digits of v % 100 (tens '0' -> ' '), right
 *   aligned at W - 0x10 / W - 0x24 with one pixel less for each digit '1'; then the player name
 *   0x004632e0(k) at x0, cut to fit q / 2 (see below).
 * Network, u8 0x004f6a38 == 2 (player / vehicle list): f = W * 0.003125f (x87, the floats at
 *   0x004e002a / 2e / 32), c = trunc(f * 90), xl = trunc((W - c) - f * 15); "Player" at (xl, 0xf),
 *   "Vehicle" at (xl + c / 2, 0xf) (font 2); per player k: the name 0x004632e0(k) at (xl, y) and
 *   the vehicle 0x00474d68(0x00624ea8, (i8) 0x004986dc(i32 0x0050f9dc, k)) at (xl + c / 2, y),
 *   each cut to fit c / 2.
 * Cut: when the font 7 width of the string > room: excess = width("...") + (width - room);
 *   while excess > 0 drop the last character (excess -= 0x00451bf0(7, char)); then "..." is
 *   appended.
 * Text: draw 0x00451a5c(font, x, y, s), width 0x00451a9c(font, s); copies go through a stack
 * buffer (200 bytes) as in the original.
 *
 * Port: handles (game addresses) resolve through env->mem (strings in the image, the AI car
 * blocks, strings returned by the callees); the game functions through env->call, the text ones
 * through draw / width / char_width. x87 code matched in double (53 bit precision, the game's
 * control word 0x127f); 0x004979a2 is frndint with chop = trunc.
 *
 * Verified against the original under emulation (tools/emu/check_gp4_score.sh).
 */
#ifndef TM2_GP4_SCORE_H
#define TM2_GP4_SCORE_H

#include <stdint.h>

typedef struct {
    uint8_t* mode;                /* u8 0x004f6a38 */
    const uint8_t* scores;        /* 0x00624db8, 12 byte records */
    const int32_t* list_arg;      /* i32 0x0050f9dc */
    const uint32_t* names;        /* 0x004e6870, string handles, from TM2.EXE */
    const float* k;               /* 0x004e002a, 3 floats (0.003125, 90, 15), from TM2.EXE */
    uint8_t* (*mem)(void* user, uint32_t handle);
    uint32_t (*call)(void* user, uint32_t addr, uint32_t a, uint32_t b);
    void (*draw)(void* user, int32_t font, int32_t x, int32_t y, const char* s);
    int32_t (*width)(void* user, int32_t font, const char* s);
    int32_t (*char_width)(void* user, int32_t font, uint8_t c);
    void* user;
} Gp4ScoreEnv;

void gp4_score_draw(const Gp4ScoreEnv* env);

#endif /* TM2_GP4_SCORE_H */
