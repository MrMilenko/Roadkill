/*
 * pf8_mid.c: code inside Ghidra functions that the game also enters on its own ("mid-function entries",
 * tools/rt/midentries.tsv: pointed at by vtables), ported over Pf2Env (pf2_env.h). Emulator diff:
 * tools/emu/check_pf8_mid.sh. Registers: eax a, edx d, ebx b, ecx c.
 */
#include "pf2_env.h"
#include "pf8_mid.h"

/* 0x00473140 (the `ret` at the start of render_frame's block, a vtable slot 0x004e5704 / 0x004e5708):
   returns eax unchanged */
uint32_t pf8_00473140(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) { (void)e; (void)d; (void)b; (void)c; return a; }

/* 0x004ac9bc (the tail of 0x004ac9b0, a window object vtable slot 0x004e61f4): returns 2 */
uint32_t pf8_004ac9bc(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) { (void)e; (void)a; (void)d; (void)b; (void)c; return 2; }
