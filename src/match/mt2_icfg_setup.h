/*
 * mt2_icfg_setup.h: the "press a key" screen of the input configuration (one control row of one
 * device), set up by TM2.EXE 0x0045fc3c (old name inputcfg_setup), ported from the PC machine code.
 *
 * It loads the background SHELLDB3\INPUTDLG.PCX into the image 0x0050e444 at (160, 77), marks
 * (160, 77, 479, 427) for redraw, creates the seven widgets (0x0050e428 accept / 0x0050e42c
 * cancel text buttons, 0x0050e430 row name / 0x0050e434 current code images, 0x0050e438
 * instructions / 0x0050e43c status lists, 0x0050e440 code text), colours and links them, reads
 * the row's current code (0x004a8fe8) into 0x0050e5c8 / 0x0050e5ca / 0x0050e5cb / 0x0050e5c9,
 * fills the instructions for the device (1 keyboard, 4 joystick, 2 mouse; other kinds none), and
 * focuses the code text. Widgets are objects with a method table at +0x34 (+0xb0 / +0xb4 / +0xb8
 * colours, +0xc0, +0xe0 click handler).
 *
 * Verified against the original under emulation (tools/emu/check_mt2_icfg_setup.sh).
 */
#ifndef MT2_ICFG_SETUP_H
#define MT2_ICFG_SETUP_H

#include <stdint.h>

typedef struct {
    uint8_t* (*resolve)(void* user, uint32_t addr);
    /* a call of original code (Watcom registers, stack arguments, callee pops); returns eax.
     * The Win32 imports go here too: fn 0x00c90574 SetRect (stack: rect, l, t, r, b; the rect,
     * a local the original never reads, is passed as 0) and 0x00c904f8 InvalidateRect. */
    uint32_t (*call)(void* user, uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx,
                     const uint32_t* stack, int nstack);
    /* a call whose register `reg` (0 eax, 1 edx) is the original's 300 byte stack buffer */
    uint32_t (*call_buf)(void* user, uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx,
                         char* buf, int reg);
    void* user;
} Mt2IcfgSetup;

/* 0x0045fc3c: kind = device (1, 2, 4), row = control row 0..15. */
void mt2_icfg_setup(const Mt2IcfgSetup* m, int32_t kind, int32_t row);

/* 0x0045f3b4 (old name inputcfg_create_widgets): the input configuration screen itself.
 * Background SHELLDB3\INPUTCFG.PCX into the image 0x0050e4f8; the device buttons 0x0050e108 /
 * 0x0050e10c, the default buttons 0x0050e414 / 0x0050e418, the done / cancel buttons 0x0050e41c /
 * 0x0050e420; the mouse flags 0x0050e5ac / 0x0050e5b0 / 0x0050e5b4 (0x08000000 when
 * 0x004932a0(1 / 4 / 2) is 2), 0x0050e5b8 = 0; then for each of the 16 control rows (key id
 * k = byte 0x004e7a48 + 8 row, y = 77 + 22 row) a left (keyboard) and a right (joystick) key
 * button with its record: left 0x0050e110 + 16 row {+4 i32 1, +8 i32 id 0xb4 + row, +0xc u8 k,
 * +0xd u8 code 0x004a1f8c(k), +0x10 widget}, right 0x0050e210 + 16 row {+4 i32 2, +8 id 0xc8 +
 * row, +0xc k, +0xd code 0x004a2758(k), +0x10 widget}; the device panels 0x00460438 /
 * 0x004604f4, the click handlers, the navigation links (rows wrap from 15 to 0, then rows 0 and
 * 15 relinked to the buttons), the key lists drawn (0x0046086c / 0x00460ea0 with 0, 0) and the
 * buttons shown. */
void mt2_icfg_create_widgets(const Mt2IcfgSetup* m);

#endif /* MT2_ICFG_SETUP_H */
