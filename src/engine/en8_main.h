/*
 * en8_main.h: the game's WinMain, TM2.EXE 0x004a8b04 (game_main), in the En2Env of en2_env.h.
 * Frame (En2Env.scratch, the original's offsets): +0 the runtime's start up block (0x004c45c0 /
 * 0x004c471c / 0x004c4718; +0x1c4 / +0x1c8 the screen width / height), +0x1e0 client RECT,
 * +0x1f0 window RECT, +0x200 registry key, +0x204 / +0x208 window x / y.
 * Verified against the original under emulation (tools/emu/check_en8_main.sh).
 */
#ifndef TM2_EN8_MAIN_H
#define TM2_EN8_MAIN_H

#include <stdint.h>
#include "en2_env.h"

/* 0x004a8b04 (stdcall WinMain(hInstance, hPrev, cmdline, nShow), ret 0x10): instance stored
 * (0x00bdfb24, 0x004a6b78), registry path / key created, runtime start up, CoInitialize (failure:
 * returns 1); then setjmp (0x004c47e8, jump buffer 0x00bdfaf0): on the first return the paths
 * (0x00498ddc "TM2.EXE" marker 0x004e321f, failure returns 0), the 640 x 480 client window
 * (style 0x02ca0000) at the saved position (0x004e3227 / 0x004e3233, kept on the screen), the
 * client area filled black, the game (0x00477124); on both returns: display shutdown 0x004775a8,
 * the window position saved, the window destroyed, ClipCursor(NULL), CoUninitialize, the runtime
 * end. Returns reg_flush's result (0 when GetWindowRect failed).
 * edi_in: the caller's edi. ORIGINAL: after a longjmp back to the setjmp, edi (the window
 * handle on the normal path) is the value saved by setjmp, i.e. the caller's, and is passed to
 * GetWindowRect; the port takes it as an argument. */
uint32_t en8_game_main(En2Env* e, uint32_t edi_in, uint32_t inst, uint32_t prev, uint32_t cmdline, uint32_t show);

#endif
