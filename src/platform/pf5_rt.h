/*
 * pf5_rt.h: small Watcom runtime pieces of TM2.EXE (docs/PLATFORM.md "Watcom runtime, one row
 * each"):
 *   0x004bc7da thread data hook default: returns u32 0x00c85bc0 (falls into the ret at 0x004bc7df)
 *   0x004bc7df a lone ret (default of the other runtime hooks); eax unchanged
 *   0x004bc7e0 / 0x004bc7e5 jmp 0x004cac51 / jmp 0x004cad66 (registers passed on)
 *   0x004bc911 C start up body (eax, edx): module = GetModuleHandleA(NULL) (thunk 0x004d18c0),
 *              0x004bc7ea(0, edx, module), 0x004cab7f(eax), call [0x004e8a04], the stack limit
 *              fs:[8] stored through the thread data hook call [0x004e89c4], then
 *              0x004cb24e(0xff) (initializers), whose result is returned. The port reads fs:[8]
 *              as memory at PF5_TIB + 8 (PF5_TIB = 0: the emulator's FS base).
 *   0x004bc957 exit(code eax): 0x004cabbd(), 0x004cb299(0, 0xff), call [0x004e8a00],
 *              ExitProcess(code) (thunk 0x004d18de); never returns.
 *   0x004bf848 array construct (eax memory, edx count, ebx descriptor: +0x01 constructor, +0x0d
 *              element size): 0 for no memory; 0x004cbc71(memory, &frame) registers an exception
 *              frame (frame +0x08 constructed count, +0x0c result) and returns the chain head;
 *              the constructor runs on each element while count > frame +0x08 (unsigned, the
 *              counter incremented after each); the head is unlinked (*head = **head); returns
 *              frame +0x0c. The frame is env scratch.
 *
 * Verified against the original under emulation (tools/emu/check_pf5_rt.sh).
 */
#ifndef PF5_RT_H
#define PF5_RT_H

#include "pf2_env.h"

#define PF5_TIB 0u

uint32_t pf5_thread_data(Pf2Env* e);
uint32_t pf5_hook_nop(Pf2Env* e, uint32_t eax);
uint32_t pf5_fd_add(Pf2Env* e, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx);
uint32_t pf5_fd_clear(Pf2Env* e, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx);
uint32_t pf5_crt_startup(Pf2Env* e, uint32_t eax, uint32_t edx);
uint32_t pf5_crt_exit(Pf2Env* e, uint32_t code);
uint32_t pf5_array_construct(Pf2Env* e, uint32_t mem, uint32_t count, uint32_t desc);

#endif /* PF5_RT_H */
