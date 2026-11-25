/*
 * mt2_icfg_dlg.h: the key rebinding dialog of the input configuration screen, ported from the PC
 * machine code of TM2.EXE 0x00462168..0x00462e5a (constructor, destructor, the three per device
 * key readers, the dialog builder, the OK handler).
 *
 * The dialog object (emulator address `dlg`; window class derived from 0x004a9dd0):
 *   +0x000 u32 parent window handle
 *   +0x034 u32 method table (0x004e5008 for this class; method +0xc8 = close (obj, 0))
 *   +0x058 u32 device kind: 1 keyboard (names 0x004e755c), 2 (names 0x004e795c), 4 (names 0x004e797c)
 *   +0x05c i32 control row 0..15 (key id byte 0x004e7a48 + 8 row, row name ptr 0x004e7a44 + 8 row)
 *   +0x060 u32 chosen code
 *   +0x064 u8 code at open, +0x065 u8 last key, +0x066 u8 device 4 code, +0x067 u8 device 2 code,
 *   +0x068 u8 shown code, +0x069 u8 last shown code
 *   +0x06c, +0x12c buttons (OK / cancel, ids 1 / 2), +0x1ec status text, +0x240 prompt,
 *   +0x294 row name, +0x2e8 current code, +0x33c new code (white on black)
 * Globals: 0x004e7554 / 0x004e7558 sound sequence counters (accept / reject), 0x00626010 sound
 * object (+0x1c method table: +0 play(obj, id, 0, 0; 0x63, seq, 0), +4 commit(obj)), 0x0047314c
 * sound flush, 0x0050e5cc u8[256] keyboard code usable, 0x0050e6cc u8[8] device 2 codes usable,
 * 0x0050e6d4 u8[50] device 4 codes usable, 0x0050e11d / 0x0050e21d / 0x0050e31d + 16 row: the
 * code bound per row (keyboard / device 4 / device 2), 0x0050e120 / 0x0050e220 / 0x0050e320 + 16 row
 * the row's label widget.
 *
 * Every access to game memory goes through resolve (emulator address -> host bytes), every call
 * of other code through call (the original's address and its register / stack arguments).
 *
 * Verified against the original under emulation (tools/emu/check_mt2_icfg_dlg.sh).
 */
#ifndef MT2_ICFG_DLG_H
#define MT2_ICFG_DLG_H

#include <stdint.h>

typedef struct {
    uint8_t* (*resolve)(void* user, uint32_t addr);
    /* a call of original code at fn: Watcom registers (only the ones the callee reads are
     * meaningful) and the stack arguments (callee pops); returns eax */
    uint32_t (*call)(void* user, uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx,
                     const uint32_t* stack, int nstack);
    /* 0x004a6f08 (set label) with a text the original builds in its stack frame */
    void (*text_local)(void* user, uint32_t widget, const char* text);
    void* user;
} Mt2IcfgDlg;

/* 0x00462168: constructor; returns the object (eax of the last base constructor - 0x33c). */
uint32_t mt2_icfg_dlg_ctor(const Mt2IcfgDlg* m, uint32_t dlg);
/* 0x004621f8: destructor (flags bit 2: only the class free path 0x004a9ff8 / 0x004aa018;
 * bit 1: free 0x004aa39c after the member destructors). */
uint32_t mt2_icfg_dlg_dtor(const Mt2IcfgDlg* m, uint32_t dlg, uint32_t flags);
/* 0x004622fc: keyboard reader (kind 1). */
void mt2_icfg_dlg_read_keyboard(const Mt2IcfgDlg* m, uint32_t dlg);
/* 0x00462500: device 4 reader. */
void mt2_icfg_dlg_read_dev4(const Mt2IcfgDlg* m, uint32_t dlg);
/* 0x004627c4: device 2 reader. */
void mt2_icfg_dlg_read_dev2(const Mt2IcfgDlg* m, uint32_t dlg);
/* 0x00462a88: builds the dialog's controls and texts; returns 1. */
uint32_t mt2_icfg_dlg_build(const Mt2IcfgDlg* m, uint32_t dlg);
/* 0x00462d90: OK: the chosen code into the row's table and label, then close; returns 0. */
uint32_t mt2_icfg_dlg_ok(const Mt2IcfgDlg* m, uint32_t dlg);

#endif /* MT2_ICFG_DLG_H */
