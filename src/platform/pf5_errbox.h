/*
 * pf5_errbox.h: win32_error_box, TM2.EXE 0x004bc660 (matcher name menuPasswordDigitsCreate:
 * wrong). cdecl, variadic: stack[0] = error index (its low byte, sign extended), then the format's
 * arguments; plain ret (the caller pops).
 *
 * Table 0x004e8840 (from TM2.EXE), 12 bytes per entry {MessageBox type, caption, format}:
 * wvsprintfA(buf[800], format, &args) (IAT 0x00c90598), ShowCursor(TRUE) (0x00c90580), hwnd =
 * 0x004a8f34(), GetStockObject(BLACK_BRUSH 4) (0x00c901ec), old = SetClassLongA(hwnd, -10, brush)
 * (0x00c90558), r = MessageBoxA(hwnd, buf, caption, type) (0x00c90524), ShowCursor(FALSE),
 * SetClassLongA(hwnd, -10, old); returns r. No range check on the index (negative indexes read
 * before the table).
 *
 * Port: the two argument words after the index are placed in env scratch + 0x100 (the original's
 * variadic list on its stack); the message buffer is env scratch.
 *
 * Verified against the original under emulation (tools/emu/check_pf5_errbox.sh).
 */
#ifndef PF5_ERRBOX_H
#define PF5_ERRBOX_H

#include "pf2_env.h"

uint32_t pf5_error_box(Pf2Env* e, uint32_t index, uint32_t a1, uint32_t a2);

#endif /* PF5_ERRBOX_H */
