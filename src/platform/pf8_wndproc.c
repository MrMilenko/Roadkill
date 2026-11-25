/*
 * pf8_wndproc.c: TM2.EXE 0x004a6a60, an unlisted function (not in Ghidra's function list; window_obj_init
 * 0x004a6ac8 installs it): the window / dialog procedure trampoline of the game's window objects.
 * Ported over Pf2Env (pf2_env.h); emulator diff: tools/emu/check_pf8_wndproc.sh.
 *
 * stdcall (hwnd, msg, wparam, lparam), ret 0x10. The object is the window property "ST Object"
 * (GetPropA, name 0x004e3194); WM_INITDIALOG (0x110) with an lparam and no property yet stores lparam as
 * the property (SetPropA, the same name at 0x004e319e) and uses it. With an object: its vtable (+0x34)
 * slot 8 (eax this, edx hwnd, ebx msg, ecx wparam; stack lparam) gives the result; without:
 * DefWindowProcA(hwnd, msg, wparam, lparam).
 */
#include "pf2_env.h"
#include "pf8_wndproc.h"

uint32_t pf8_window_proc(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c, uint32_t hwnd, uint32_t msg,
                         uint32_t wp, uint32_t lp) {
    (void)a; (void)d; (void)b; (void)c;
    uint32_t obj = pf2_call2(e, 0x00c904e0u, hwnd, 0x004e3194u);
    if (obj == 0 && msg == 0x110 && lp != 0) {
        pf2_call3(e, 0x00c90570u, hwnd, 0x004e319eu, lp);
        obj = lp;
    }
    if (obj) {
        uint32_t v[5] = {obj, hwnd, msg, wp, lp};
        return e->call(e->user, pf2_r32(e, pf2_r32(e, obj + 0x34) + 8), v, 5);
    }
    return pf2_call4(e, 0x00c90484u, hwnd, msg, wp, lp);
}
