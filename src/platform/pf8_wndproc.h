/* pf8_wndproc.h: src/platform/pf8_wndproc.c (unlisted 0x004a6a60, the window procedure trampoline) */
#ifndef PF8_WNDPROC_H
#define PF8_WNDPROC_H
#include "pf2_env.h"
uint32_t pf8_window_proc(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c, uint32_t hwnd, uint32_t msg,
                         uint32_t wp, uint32_t lp);
#endif
