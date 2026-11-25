/*
 * watcom_eh.c: the Watcom C++ runtime's object destruction (TM2.EXE 0x004c43a8 and its callers)
 * and a few small runtime entries, on the host (port code). Watcom convention: a[0..3] = EAX,
 * EDX, EBX, ECX as far as the callee takes registers, then its stack words (compact).
 *
 * The original unwinds a "state" frame on the chain 0x00c8b9c0 through a per kind handler table
 * (0x00c8b9e4, filled by the runtime's start up) and an action table. The game only uses it in
 * two shapes, done here directly (port code, not the generic interpreter; NOT VERIFIED under the
 * emulator):
 *   - delete[] of an array whose class has a destructor (0x004c156c(array, count, descriptor)):
 *     the elements destroyed last first with the descriptor's destructor;
 *   - the exit handlers (0x004a2dcc pushes a frame per registered static object and calls
 *     0x004c44e2): table kind 3 {3, destructor, object, descriptor}, state 1 = constructed.
 * Class descriptor (as the ported array construct 0x004bf848 reads it): +0x00 kind byte, +0x01
 * constructor, +0x05 copy constructor, +0x09 destructor, +0x0d element size. A destructor is
 * called as the original does, eax = the object, edx = 0 (do not free).
 */
#include "win32_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void destroy(uint32_t dtor, uint32_t obj)
{
    if (!dtor) return;
    uint32_t v[2] = {obj, 0};
    rt_call(dtor, v, 2);
}

/* 0x004c156c (array eax, count edx, descriptor ebx): destroys the elements, the last first;
 * returns the array */
static uint32_t e_array_destroy(const uint32_t* a, int n)
{
    uint32_t arr = W(0), cnt = W(1), desc = W(2);
    if (!desc) rt_stop("0x004c156c without a descriptor (ebx)", arr);
    uint32_t dtor = rt_r32(desc + 9), size = rt_r32(desc + 0x0d);
    for (uint32_t i = cnt; i > 0; i--) destroy(dtor, arr + (i - 1) * size);
    return arr;
}

/* 0x004c44e2: unwinds the frame on top of the chain 0x00c8b9c0 to state 0 (the exit handler run
 * 0x004a2dcc pushed it: +4 the node's table, +8 its state). Table kind 3: one static object,
 * {3, destructor, object, descriptor}, destroyed when the state is not 0. */
static uint32_t e_exit_unwind(const uint32_t* a, int n)
{
    (void)a; (void)n;
    uint32_t f = rt_r32(0x00c8b9c0u);
    if (!f) return 0;
    uint32_t t = rt_r32(f + 4), state = rt_r32(f + 8);
    if (state == 0) return 0;
    switch (rt_r32(t)) {
    case 3:
        rt_w32(f + 8, 0);
        destroy(rt_r32(t + 4), rt_r32(t + 8));
        break;
    default:
        fprintf(stderr, "tm2: exit handler table 0x%08x kind %u not implemented: skipped\n", t, rt_r32(t));
        rt_w32(f + 8, 0);
        break;
    }
    return 0;
}

/* 0x004c4530 (o eax): o+0 = 0 (the library slot object's constructor); eax kept */
static uint32_t e_slot_ctor(const uint32_t* a, int n) { rt_w32(W(0), 0); return W(0); }
/* 0x004c4538 (o eax): its destructor: FreeLibrary(o+0) when set, o+0 = 0; returns o */
static uint32_t e_slot_dtor(const uint32_t* a, int n)
{
    uint32_t o = W(0);
    if (rt_r32(o)) {
        uint32_t v[1] = {rt_r32(o)};
        rt_call(rt_import_slot("KERNEL32.dll!FreeLibrary"), v, 1);
        rt_w32(o, 0);
    }
    return o;
}

/* 0x004c4510 (message eax, code edx): the fatal error exit: under a debugger (u8 0x004e94e0) an
 * int3, else the message written to the stderr handle and exit(code) (0x004bc957, which runs the
 * exit handlers; the host exit() when that is not bound) */
static uint32_t e_fatal(const uint32_t* a, int n)
{
    fprintf(stderr, "tm2: fatal: %s\n", W(0) ? w32_str(W(0)) : "");
    fflush(stderr);
    if (rt_name(0x004bc957u)) { uint32_t v[1] = {W(1)}; rt_call(0x004bc957u, v, 1); }
    exit((int)W(1));
}

/* 0x004c38fc (x eax): u32 0x004e8f88 = x; eax kept */
static uint32_t e_set_4e8f88(const uint32_t* a, int n) { rt_w32(0x004e8f88u, W(0)); return W(0); }

/* 0x004cb209 strdup(s eax): malloc 0x004bf680(strlen + 1) and the copy; 0 when out of memory */
static uint32_t e_strdup(const uint32_t* a, int n)
{
    uint32_t s = W(0);
    uint32_t len = (uint32_t)strlen(w32_str(s)) + 1;
    uint32_t v[1] = {len};
    uint32_t p = rt_call(0x004bf680u, v, 1);
    if (p) memmove(rt_ptr(p), rt_ptr(s), len);
    return p;
}

void rt_bind_watcom_eh(void)
{
    rt_bind(0x004c156cu, e_array_destroy, "crt_array_destroy");
    rt_bind(0x004c44e2u, e_exit_unwind, "crt_exit_unwind");
    rt_bind(0x004c4530u, e_slot_ctor, "lib_slot_ctor");
    rt_bind(0x004c4538u, e_slot_dtor, "lib_slot_dtor");
    rt_bind(0x004c4510u, e_fatal, "crt_fatal_exit");
    rt_bind(0x004c38fcu, e_set_4e8f88, "lib_set_4e8f88");
    rt_bind(0x004cb209u, e_strdup, "crt_strdup");
}

RT_BIND_AREA(watcom_eh)
