/*
 * gp4_font.h: font setup, TM2.EXE 0x0045289c (Ghidra subsys_game_init, old name; PS1 decomp
 * fontInit, src/semantic/font_init.c). No arguments; returns the last packed color (eax of the
 * final 0x00496590 call). The function falls through into 0x00452d28 (a lone ret, Ghidra's
 * game_set_palette_mode).
 *
 * Globals set (block 0x005024d0..0x0050281f): i32 0x005024d0 / d4 = 0; u32 0x00502800 / 04 =
 * 0x004e7206, 0x00502808 = 0x004e6ffc, 0x005027f8 / fc and 0x0050280c = 0x004e6c60, 0x00502814 =
 * 0x004e7038, 0x00502810 = 0x004e6e2e (addresses of font tables, stored as is); i32 0x005025e8 = 3,
 * 0x005025d8 / dc / f0 = -1, 0x005025e4 / e0 = 0, 0x005025f4 = 1, 0x005025ec = 7.
 *
 * Eight fonts i = 0..7: descriptor d = the pointer i of the table at 0x004e73f4 (+0x00 file name,
 * +0x04 i32 loaded, +0x08 handle, +0x0c, +0x10 filled by the loader, +0x14 u8 color), page
 * p = 0x1ff - i32 i of the table at 0x004e73d4 (both tables read from the player's TM2.EXE).
 *   not loaded: handle = 0x004960bc(name, d + 0x0c, d + 0x10), loaded = 1.
 *   record 0x00502518 + 0x18 i: +0x05 u8 = 0x20, +0x07 u8 = 0x7c, +0x10..+0x13 u8 = 0, +0x14 i32 = p.
 *   page entry 0x00a30c48 + 0x30 p: +0x00 handle, +0x04 / +0x08 = d +0x0c / +0x10, +0x0c =
 *   0x005025f8, +0x10 / +0x20 / +0x28 = 1, +0x2c = 0.
 *   u8 0x00502818 + i = d +0x14, then replaced for i = 0..6 (jump table 0x00452880) by
 *   0xed, 0xf6, 0xf0, 0xf5, 0xf5, 0xf9, 0xee.
 * Then the 256 i16 at 0x005025f8 = 0 and the 26 i16 at 0x005027c4.. = 0x00496590(r, g, b) colors
 * (pink 0xfc 0x90 0xb8 by default, yellow, black, green, blue, a light green at 0x005027ec,
 * black at 0x005027f6).
 *
 * Port: descriptors are 32 bit handles (game addresses) resolved by env->mem; the loader and the
 * color pack go through env->call.
 *
 * Verified against the original under emulation (tools/emu/check_gp4_font.sh).
 */
#ifndef TM2_GP4_FONT_H
#define TM2_GP4_FONT_H

#include <stdint.h>

typedef struct {
    uint8_t* g;                /* 0x005024d0 .. 0x0050281f */
    uint8_t* pages;            /* 0x00a30c48, 0x200 x 0x30 */
    const int32_t* page_of;    /* 0x004e73d4, 8 x i32, from TM2.EXE */
    const uint32_t* desc;      /* 0x004e73f4, 8 x u32, from TM2.EXE */
    uint8_t* (*mem)(void* user, uint32_t handle);
    uint32_t (*call)(void* user, uint32_t addr, uint32_t a, uint32_t b, uint32_t c);
    void* user;
} Gp4FontEnv;

uint32_t gp4_font_init(const Gp4FontEnv* env);

#endif /* TM2_GP4_FONT_H */
