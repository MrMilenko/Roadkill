/*
 * pf7_dtor.c: the Watcom C++ "deleting destructor" vtable slots among the unlisted functions of
 * TM2.EXE (tools/rt/unlisted.tsv), ported over Pf2Env (pf2_env.h). Emulator diff:
 * tools/emu/check_pf7_dtor.sh.
 *
 * dtor(eax this, edx flags): flags & 4: an array: 0x004a9ff8(this, the element destructor table)
 * then free 0x004aa018 of its result, returns this; else the base destructor (0x004a3ff4, or
 * 0x004a6b10 for 0x004acb30) with edx 1, flags & 2: free 0x004aa39c of its result; returns it.
 * Registers are passed as the original has them (ebx = this or the result, ecx = flags; edx kept).
 */
#include "pf2_env.h"
#include "pf7_dtor.h"

#define C4(fn, a, d, b, c) pf2_call4(e, (fn), (a), (d), (b), (c))

static uint32_t dtor(Pf2Env* e, uint32_t self, uint32_t fl, uint32_t table, uint32_t base, uint32_t obj) {
    if (fl & 4) {
        uint32_t r = C4(0x004a9ff8u, self, table, self, fl);
        C4(0x004aa018u, r, table, self, fl);
        return self;
    }
    uint32_t r = C4(base, obj, 1, self, fl);
    if (fl & 2) C4(0x004aa39cu, r, 1, r, fl);
    return r;
}

#define DTOR(addr, table, base) \
    uint32_t pf7_##addr(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) \
    { (void)b; (void)c; return dtor(e, a, d, (table), (base), a); }
DTOR(00462ed0, 0x004e4fa4u, 0x004a3ff4u)
DTOR(00462f10, 0x004e4fb8u, 0x004a3ff4u)
DTOR(00489900, 0x004e57ccu, 0x004a3ff4u)
DTOR(00489940, 0x004e57e0u, 0x004a3ff4u)
DTOR(004a6960, 0x004e5934u, 0x004a3ff4u)
DTOR(004a69a0, 0x004e5948u, 0x004a3ff4u)
DTOR(004acb30, 0x004e61c4u, 0x004a6b10u)

/* 0x00462e60: the same with four member destructors 0x004aa064(member, 0) first, from +0xa8 down
   by 0x14 (each on the previous one's returned eax), and the base on that - 0x6c */
uint32_t pf7_00462e60(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    (void)b; (void)c;
    if (d & 4) {
        uint32_t r = C4(0x004a9ff8u, a, 0x004e4f90u, a, d);
        C4(0x004aa018u, r, 0x004e4f90u, a, d);
        return a;
    }
    uint32_t x = C4(0x004aa064u, a + 0xa8, 0, a, d);
    for (int k = 0; k < 3; k++) x = C4(0x004aa064u, x - 0x14, 0, a, d);
    uint32_t r = C4(0x004a3ff4u, x - 0x6c, 1, a, d);
    if (d & 2) C4(0x004aa39cu, r, 1, r, d);
    return r;
}
