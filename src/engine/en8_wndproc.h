/*
 * en8_wndproc.h: the window object's message dispatcher, TM2.EXE 0x004a7154, in the En2Env of
 * en2_env.h (memory by game address, every call by the original's address; the MENUITEMINFO,
 * POINT and MSG locals live at En2Env.scratch at the original's frame offsets).
 *
 * Window object: +0 HWND, +4 previous window procedure (subclassed), +0x14 a registered message,
 * +0x34 vtable (handler methods; slot offsets below). Other windows' objects are found with
 * GetPropA(hwnd, 0x004e3194). A handler result of 2 means "not handled": the message goes to
 * CallWindowProcA(+4, ...) when there is a previous procedure, else 0 is returned.
 *
 * Verified against the original under emulation (tools/emu/check_en8_wndproc.sh).
 */
#ifndef TM2_EN8_WNDPROC_H
#define TM2_EN8_WNDPROC_H

#include <stdint.h>
#include "en2_env.h"

/* 0x004a7154 (Watcom: eax = obj, edx = hwnd, ebx = msg, ecx = wParam, stack lParam; ret 4).
 * esi_in: the caller's esi. ORIGINAL BUG: four paths return (and test against 2) the
 * uninitialized esi register: WM_COMMAND from a control without an object, WM_CTLCOLOR* whose
 * handler answers 0, WM_ENTERIDLE (0x121, wParam 0) without an object or with a message already
 * waiting; the port takes that register as an argument. */
uint32_t en8_wndproc(En2Env* e, uint32_t obj, uint32_t hwnd, uint32_t msg, uint32_t wp, uint32_t esi_in,
                     uint32_t lp);

#endif
