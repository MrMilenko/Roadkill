/*
 * gp4_hud.h: the in-game HUD of one view, TM2.EXE 0x00434710 (Ghidra render_3d_scene, old name;
 * PS1 decomp hudDrawSingleView, src/semantic/hud_main.c). eax = view / player index i.
 *
 * Nothing when 0x0045c348() (u8) or 0x0046be2c() is set. split = (i8) 0x004958ac().
 * Layout: 0x004938a4(&w, &h); when (w, h) != (0x004f6a2c, 0x004f6a30) the 12 layout entries
 * (0x18 bytes) at 0x004e66fc get +0x10 = +0x00 * (w / 2) + w / 2 + +0x08 and +0x14 = +0x04 *
 * (h / 2) + h / 2 + +0x0c (the cached size is never updated, so this runs every frame once it
 * differs). The positions read below are entries of that table (0x004e6724 / 28, 3c / 40, 54 /
 * 58, 6c / 70, 9c / a0, b4 / b8, cc / d0, e4 / e8, fc / 6800, 14 / 18).
 * car = 0x0045afbc(i). Primitives of view i live at 0x004f6a3c + 0x800 i (named below by their
 * address for view 0); every draw goes through AddPrim 0x004967f4, colors by level 0x00436120,
 * digits 0x00496dcc(n, prim, 0x004f70d8), text 0x00452204 / 0x0045222c / 0x00451a5c, with the
 * empty 0x00452d28 and 0x00452d48 around them.
 *
 * Target panel (u8 0x004e685c + i): 0x004e685e + i -> 0x004361d4(0x004f6a34, i); the "enemies
 * toasted" message cycle 0x004fb15c..6c (100 frame period, 0x0045bab8 picks); its text. Target
 * t = 0x00429f7c(i, &human): its id i16 +0x90 / +0x30, stats +0x138 / +0x4ec; frame prim
 * 0x004f6ab4 (y up 0x35 when 0x004e685e + i is clear); name (split or not, two places); its health
 * bar 0x004f6a9c: pct = health i32 +0xa0 * 100 / i16 +0x3a, level 3 / 2 / 1 / 0 above 40 / 12 / 0,
 * width pct * 90 / 100 clamped to 1..90, drawn with the y shifted by 0x35 and back.
 * Own health bar 0x004f6a60 / 0x004f6a78: car i32 +0x1d8 * 100 / i16 +0x172, width 70.
 * Lives digit (single player only, 0x004632b0() == 0): '0' for game types 3 / 4, else
 * 0x00474510(i) + '/' clamped to '0'..'4' else '2', into "Lives: x" at 0x004e6864.
 * Turbo bar 0x004f6ad8 / 0x004f6af0: i16 +0x878 * 100 / 3000, width 46, three layouts by
 * 0x004e685c / 0x004e6860.
 * Weapons: 0x004e6860 + i set: the list (count i16 0x004f6b14) with ammo i16 +0x856 + 2 k, the
 * selected i8 +0x855 highlighted by a stack primitive, one or two digits by ammo >= 10; clear:
 * only the selected one (remapped by the table 0x004e681c unless the event type u8 0x00624ec7 is
 * 0x0b or 4; out of 0..7 -> 0).
 * Then i16 +0x87a / 10 as two digits, the score 0x0045aee8(i) (abs, three digits into the template
 * 0x004e69a0), 0x00437134 when 0x004f6a38 (set with 0x004f6a3a once in multiplayer), and the fps
 * counter when 0x0044eee4(0x15): timeGetTime (import 0x00c90640), once a second (signed compares)
 * the digits of 0x004931dc() into 0x004e66f4.
 *
 * DECOMPILER TRAPS: Ghidra's (dword at a) >> 16 reads are the i16 at a + 2 throughout (target id
 * +0x90 / +0x30, max health +0x3a / +0x172, turbo +0x878, digits +0x87a, ammo +0x856, the list
 * count 0x004f6b14); the fps "once a second" test is signed in the machine code.
 *
 * Port: every address is a 32 bit game address resolved by env->mem; calls go through env->call
 * (game address, register then stack arguments; `local` for the one argument pointing at the
 * caller's stack: the size outputs, the human flag, the score string, the highlight primitive).
 *
 * Verified against the original under emulation (tools/emu/check_gp4_hud.sh).
 */
#ifndef TM2_GP4_HUD_H
#define TM2_GP4_HUD_H

#include <stdint.h>

typedef struct {
    uint8_t* (*mem)(void* user, uint32_t addr);
    uint32_t (*call)(void* user, uint32_t addr, int n, const uint32_t* args, void* local, void* local2);
    void* user;
} Gp4HudEnv;

void gp4_hud_draw(const Gp4HudEnv* env, int32_t view);

#endif /* TM2_GP4_HUD_H */
