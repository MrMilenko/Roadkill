/*
 * pf5_rt.c: small Watcom runtime pieces of TM2.EXE (see pf5_rt.h).
 */
#include "pf5_rt.h"

uint32_t pf5_thread_data(Pf2Env* e) { return pf2_r32(e, 0xc85bc0); }

uint32_t pf5_hook_nop(Pf2Env* e, uint32_t eax) { (void)e; return eax; }

uint32_t pf5_fd_add(Pf2Env* e, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx)
{ return pf2_call4(e, 0x4cac51, eax, edx, ebx, ecx); }

uint32_t pf5_fd_clear(Pf2Env* e, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx)
{ return pf2_call4(e, 0x4cad66, eax, edx, ebx, ecx); }

uint32_t pf5_crt_startup(Pf2Env* e, uint32_t eax, uint32_t edx) {
    uint32_t module = pf2_call1(e, 0x4d18c0, 0);
    pf2_call3(e, 0x4bc7ea, 0, edx, module);
    pf2_call1(e, 0x4cab7f, eax);
    pf2_call0(e, 0x4e8a04);
    uint32_t limit = pf2_r32(e, PF5_TIB + 8);
    pf2_w32(e, pf2_call0(e, 0x4e89c4), limit);
    return pf2_call1(e, 0x4cb24e, 0xff);
}

uint32_t pf5_crt_exit(Pf2Env* e, uint32_t code) {
    pf2_call0(e, 0x4cabbd);
    pf2_call2(e, 0x4cb299, 0, 0xff);
    pf2_call0(e, 0x4e8a00);
    pf2_call1(e, 0x4d18de, code);
    return 0;   /* not reached in the original */
}

uint32_t pf5_array_construct(Pf2Env* e, uint32_t mem, uint32_t count, uint32_t desc) {
    if (mem == 0) return 0;
    uint32_t fr = e->scratch;
    uint32_t head = pf2_call2(e, 0x4cbc71, mem, fr);
    uint32_t ctor = pf2_r32(e, desc + 1), size = pf2_r32(e, desc + 0xd);
    for (uint32_t p = mem; count > pf2_r32(e, fr + 8); p += size) {
        pf2_call1(e, ctor, p);
        pf2_w32(e, fr + 8, pf2_r32(e, fr + 8) + 1);
    }
    pf2_w32(e, head, pf2_r32(e, pf2_r32(e, head)));
    return pf2_r32(e, fr + 0xc);
}
