/*
 * pf5_crt.h: Watcom C runtime routines of TM2.EXE, as the original does them:
 *   0x004bc710 strstr(haystack eax, needle edx): an empty needle returns the haystack; a one
 *              character needle is a strchr two bytes a round (never matching the NUL); else
 *              end = the haystack's NUL, then repeatedly: fewer than strlen(needle) bytes left
 *              -> 0; memchr for the first character in what is left (0 if none); a full compare
 *              (repe cmpsb) at the hit -> the hit, else go on after it.
 *              DECOMPILER TRAP: the memchr idiom jumps into the middle of `test ax, 0xcf89`
 *              (89 cf = mov edi, ecx: 0 when not found).
 *   0x004bf680 malloc(size eax): 0 for size 0 or > 0xffffffd4 (no lock). Else under the heap
 *              lock hooks (call [0x004e89e0] / [0x004e89e8]): n = max((size + 3) & ~3, 0xc); the
 *              walk starts at the rover 0x004e8e38 when n is more than the largest free size
 *              cache 0x004e8e3c and the rover is set, else at the first mini heap 0x004e8e34 with
 *              the cache reset to 0; per heap (next at +0x08, largest free at +0x14): rover = heap,
 *              0x004cb87c(size, ds, heap) allocates (non zero: done), else the cache takes the
 *              heap's largest free when larger. Nothing found: grow 0x004cbb33(size) once (then
 *              walk again), else the out of memory hook 0x004cbb8e(size): non zero -> walk again
 *              (and grow may run again), 0 -> return 0. Clears u8 0x00c8bfac before unlocking.
 *              (Note: the request size, not n, goes to 0x004cb87c; n only steers the start.)
 *   0x004bf76f free(p eax): nothing for 0; under the lock: the heap is the last used one
 *              0x00c8b34c when last <= p < last->next, else the first heap from 0x004e8e34 with
 *              heap <= p < heap->next, or the last heap; 0x004cb924(p, ds, heap); last used =
 *              heap; when heap < rover and heap +0x14 > the cache, the cache takes it; u8
 *              0x00c8bfac = 0; unlock.
 *   0x004bc7ea crt_init_rtns(is_dll eax, edx, ebx module): the Watcom start up: u32 0x00c85bbc =
 *              is_dll; thread data 0x004caf36(edx) -> 0x00c85bc0 (none: return 0 in a DLL, else
 *              ExitProcess(1)); file table 0x004cad92(); GetEnvironmentStrings -> u32 0x004e8e79
 *              (unaligned); GetModuleFileNameA(NULL, buf, 0x104) -> strdup 0x004cb209 -> 0x004e8e4c;
 *              GetCommandLineA -> strdup -> the program name skipped (quoted: to the closing quote
 *              and past it; else up to the first character whose ctype (table 0x004e633c, index
 *              (u8)(c + 1)) has bit 2, or the NUL), then blanks (bit 2) skipped -> 0x004e8e48; a
 *              DLL also gets GetModuleFileNameA(module) -> strdup -> 0x004e8e50; GetVersion ->
 *              u16 0x004e8e81 = low word, u8 0x004e8e7f = bits 24..31, u8 0x004e8e80 = bits
 *              16..19. Returns 1. Imports are called through their thunks (fn = thunk address,
 *              stdcall stack words).
 *   The data selector (mov edx, ds) passed along is the env's `ds` argument (0 under the
 *   emulator; meaningless in a flat port).
 *
 * Verified against the original under emulation (tools/emu/check_pf5_crt.sh).
 */
#ifndef PF5_CRT_H
#define PF5_CRT_H

#include "pf2_env.h"

uint32_t pf5_strstr(Pf2Env* e, uint32_t hay, uint32_t needle);
uint32_t pf5_malloc(Pf2Env* e, uint32_t size, uint32_t ds);
void pf5_free(Pf2Env* e, uint32_t p, uint32_t ds);
uint32_t pf5_crt_init_rtns(Pf2Env* e, uint32_t is_dll, uint32_t edx, uint32_t module);

#endif /* PF5_CRT_H */
