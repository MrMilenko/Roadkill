/*
 * watcom_crt.c: the Watcom C / C++ runtime of TM2.EXE above 0x004c0000 that the ported
 * functions call, on the host (port code). Watcom register convention: a[0..3] = EAX, EDX, EBX,
 * ECX as far as the callee takes registers, then its stack words (compact, docs/RUNTIME.md).
 *
 * setjmp / longjmp (0x004c47e8 / 0x004c4827): WinMain (en8_main.c) calls setjmp(0x00bdfaf0)
 * and the message loop (WM_QUIT) and the game's exit (0x004a8f24) longjmp to it with 1 / 2,
 * from frames deep below. The setjmp binding's frame is gone once it returns to the port, so a
 * plain host setjmp there would jump into a dead frame. Design: setjmp takes a host _setjmp
 * AND a copy of the host stack from its own frame up to the thread's stack top (the frames of
 * rt_call, en8_game_main, b_game_main, main: a few KB). longjmp descends below that range
 * (recursion with padding), copies the saved bytes back (so those frames are exactly as they
 * were at setjmp: return addresses, saved registers, the port's locals), pops the runtime frame
 * stack back to its setjmp height and _longjmps: the setjmp binding returns a second time, with
 * the value, into a live and intact en8_game_main. The game's own jmp_buf at the game address
 * gets a marker only (nothing reads it). Limits: one armed buffer at a time (the game has one),
 * main thread only; rt.c's call depth counter is not unwound (the trace indent and a later stop's
 * chain are off by the skipped calls; the core could expose rt_depth get/set).
 *
 * The heap (malloc 0x004bf680 / free 0x004bf76f are ported, pf5_crt.c): the mini heap list at
 * 0x004e8e34 (next at +0x08, largest free at +0x14) is the original's; what the runtime does
 * inside one mini heap is host code here: 0x004cbb33 grow adds a mini heap (the first one 24 MB)
 * from rt_heap_grow, 0x004cb87c allocates in a heap (first fit on a host side free list, blocks
 * with the Watcom 4 byte header size | 1 in use before the data), 0x004cb924 frees (coalescing),
 * 0x004cbb8e (out of memory hook) answers 0.
 */
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#include "win32_host.h"

#include <pthread.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- setjmp / longjmp ---- */

static jmp_buf host_jb;
static int armed;
static uint32_t jb_game, frame_top, jmp_value;
static char* save_lo;
static size_t save_n;
static char* save_buf;

static char* stack_top(void)
{
#ifdef __APPLE__
    return (char*)pthread_get_stackaddr_np(pthread_self());
#else
    pthread_attr_t at;
    void* base;
    size_t size;
    pthread_getattr_np(pthread_self(), &at);
    pthread_attr_getstack(&at, &base, &size);
    pthread_attr_destroy(&at);
    return (char*)base + size;
#endif
}

static __attribute__((noinline)) uint32_t c_setjmp(const uint32_t* a, int n)
{
    if (w32_thread_id() != 1) rt_stop("setjmp off the main thread", W(0));
    char probe[16];
    jb_game = W(0);
    frame_top = rt_frame_push(0);
    save_lo = (char*)((uintptr_t)(probe - 512) & ~(uintptr_t)15);
    save_n = (size_t)(stack_top() - save_lo);
    free(save_buf);
    save_buf = malloc(save_n);
    if (_setjmp(host_jb) == 0) {
        /* the copy is taken after _setjmp, so it holds this frame as it is now */
        memcpy(save_buf, save_lo, save_n);
        armed = 1;
        memset(rt_ptr(jb_game), 0, 0x30);
        rt_w32(jb_game, 0x4a4d5048u);   /* "HPMJ": a host jump buffer */
        return 0;
    }
    /* the second return: the stack between save_lo and the top was put back by do_jump */
    return jmp_value;
}

static __attribute__((noinline, noreturn)) void do_jump(void)
{
    memcpy(save_lo, save_buf, save_n);
    _longjmp(host_jb, 1);
}
static __attribute__((noinline, noreturn)) void descend(void)
{
    volatile char pad[4096];
    pad[0] = 0;
    if ((char*)pad + sizeof pad + 1024 > save_lo) descend();
    do_jump();
}

/* longjmp(buf eax, value edx): never returns */
static uint32_t c_longjmp(const uint32_t* a, int n)
{
    if (!armed || W(0) != jb_game) rt_stop("longjmp to a buffer setjmp did not arm", W(0));
    if (w32_thread_id() != 1) rt_stop("longjmp off the main thread", W(0));
    jmp_value = W(1) ? W(1) : 1;
    uint32_t cur = rt_frame_push(0);
    if (cur < frame_top) rt_frame_pop(frame_top - cur);
    descend();
}

/* ---- the heap inside one mini heap ---- */

#define MH_FIRST (24u << 20)
#define MH_MIN   (1u << 20)
#define MH_HDR   0x20u
#define HEAPS    0x004e8e34u

typedef struct { uint32_t addr, size; } Free;
typedef struct { uint32_t base, end; Free* f; int nf, cap; } Heap;
static Heap heaps[64];
static int nheaps;

static Heap* heap_of(uint32_t h)
{
    for (int i = 0; i < nheaps; i++) if (heaps[i].base == h) return &heaps[i];
    return NULL;
}
static void free_insert(Heap* h, uint32_t addr, uint32_t size)
{
    int lo = 0, hi = h->nf;
    while (lo < hi) { int m = (lo + hi) / 2; if (h->f[m].addr < addr) lo = m + 1; else hi = m; }
    /* coalesce with the previous / next free range */
    if (lo > 0 && h->f[lo - 1].addr + h->f[lo - 1].size == addr) {
        h->f[lo - 1].size += size;
        if (lo < h->nf && h->f[lo - 1].addr + h->f[lo - 1].size == h->f[lo].addr) {
            h->f[lo - 1].size += h->f[lo].size;
            memmove(h->f + lo, h->f + lo + 1, sizeof(Free) * (size_t)(h->nf - lo - 1));
            h->nf--;
        }
        return;
    }
    if (lo < h->nf && addr + size == h->f[lo].addr) { h->f[lo].addr = addr; h->f[lo].size += size; return; }
    if (h->nf == h->cap) { h->cap = h->cap ? h->cap * 2 : 64; h->f = realloc(h->f, sizeof(Free) * (size_t)h->cap); }
    memmove(h->f + lo + 1, h->f + lo, sizeof(Free) * (size_t)(h->nf - lo));
    h->f[lo] = (Free){addr, size};
    h->nf++;
}

/* 0x004cbb33 grow(size eax): a new mini heap at the end of the list; nonzero on success */
static uint32_t c_heap_grow(const uint32_t* a, int n)
{
    if (nheaps == 64) return 0;
    uint32_t want = W(0) + MH_HDR + 64;
    uint32_t size = nheaps == 0 ? MH_FIRST : MH_MIN;
    if (want > size) size = (want + 0xffffu) & ~0xffffu;
    uint32_t base = rt_heap_grow(size);
    if (!base) return 0;
    Heap* h = &heaps[nheaps++];
    memset(h, 0, sizeof *h);
    h->base = base;
    h->end = base + size;
    rt_w32(base + 0x04, size);
    rt_w32(base + 0x08, 0);            /* next */
    rt_w32(base + 0x14, size - MH_HDR);   /* largest free (an upper bound) */
    /* the first block's data 8 aligned: blocks start at 8k + 4 */
    uint32_t first = base + MH_HDR + 4;
    free_insert(h, first, (h->end - first) & ~7u);
    uint32_t p = HEAPS, cur;
    while ((cur = rt_r32(p)) != 0) p = cur + 0x08;
    rt_w32(p, base);
    return 1;
}

/* 0x004cb87c (size eax, ds edx, heap ebx): the block's data address, 0 if it does not fit */
static uint32_t c_heap_alloc(const uint32_t* a, int n)
{
    Heap* h = heap_of(W(2));
    if (!h) return 0;
    uint32_t need = (W(0) + 4u + 7u) & ~7u;
    if (need < 16) need = 16;
    for (int i = 0; i < h->nf; i++) {
        if (h->f[i].size < need) continue;
        uint32_t b = h->f[i].addr;
        if (h->f[i].size - need < 16) {
            need = h->f[i].size;
            memmove(h->f + i, h->f + i + 1, sizeof(Free) * (size_t)(h->nf - i - 1));
            h->nf--;
        } else {
            h->f[i].addr += need;
            h->f[i].size -= need;
        }
        rt_w32(b, need | 1u);
        return b + 4;
    }
    return 0;
}

/* 0x004cb924 (p eax, ds edx, heap ebx) */
static uint32_t c_heap_free(const uint32_t* a, int n)
{
    uint32_t p = W(0);
    Heap* h = heap_of(W(2));
    if (!h || p < h->base + MH_HDR + 4 || p >= h->end) {
        for (int i = 0; i < nheaps; i++) if (p > heaps[i].base && p < heaps[i].end) h = &heaps[i];
        if (!h || p >= h->end) return 0;
    }
    uint32_t b = p - 4, sz = rt_r32(b);
    if (!(sz & 1u)) return 0;   /* not in use: a double free, ignored */
    sz &= ~1u;
    rt_w32(b, sz);
    free_insert(h, b, sz);
    return 0;
}
static uint32_t c_heap_nomem(const uint32_t* a, int n) { (void)a; (void)n; return 0; }

/* ---- C++ runtime pieces ---- */

/* 0x004c4508 operator new(n eax): malloc until it succeeds; the new handler 0x00c8b9dc (with n,
 * nonzero: try again) and the other hook 0x00c8b9d8 as the original */
static uint32_t new_last, new_edx, new_ebx;   /* the last call's answer and its edx / ebx */
static uint32_t c_new_raw(const uint32_t* a, int n)
{
    uint32_t sz = W(0) ? W(0) : 1;
    for (;;) {
        uint32_t m[1] = {sz};
        uint32_t p;
        for (;;) {
            p = rt_call(0x004bf680u, m, 1);
            if (p) return p;
            uint32_t hook = rt_r32(0x00c8b9dcu);
            if (!hook || rt_call(hook, m, 1) == 0) break;
        }
        uint32_t h2 = rt_r32(0x00c8b9d8u);
        if (!h2) return 0;
        rt_call(h2, m, 0);
    }
}

static uint32_t c_new(const uint32_t* a, int n)
{
    uint32_t r = c_new_raw(a, n);
    new_last = r;
    new_edx = W(1);
    new_ebx = W(2);
    return r;
}

/* 0x004ca4d3 (memory eax, count edx, descriptor ebx): new[] completion: 0 for no memory; else
 * [memory] = count and the elements at memory + 4 constructed by 0x004bf848 (the ported array
 * construct; the original takes 0x004cd338 for a kind 4 descriptor and registers an exception
 * frame, not done here); returns that call's answer. */
static uint32_t l_array_init(const uint32_t* a, int n)
{
    uint32_t mem = W(0), cnt = W(1), desc = W(2);
    if (!mem) return 0;
    if (n < 3) {
        /* a port that passes eax only (pf3_bank): edx / ebx are still those of the operator new
         * 0x004c4508 call that made the memory (Watcom preserves them) */
        if (mem != new_last) rt_stop("0x004ca4d3 without its count (edx) / descriptor (ebx)", mem);
        cnt = new_edx;
        desc = new_ebx;
    }
    rt_w32(mem, cnt);
    if (rt_r8(desc) == 4) rt_stop("0x004ca4d3 kind 4 descriptor (0x004cd338) not implemented", desc);
    uint32_t v[3] = {mem + 4, cnt, desc};
    return rt_call(0x004bf848u, v, 3);
}

/* ---- the exception frame chain 0x00c8b9c0 (frames live in game memory; nothing throws in the
 * port, the frames only keep the chain as the original has it) ---- */
static void eh_link(uint32_t f, uint32_t edx, uint32_t ebx)
{
    rt_w32(f + 4, edx);
    rt_w32(f + 8, ebx);
    rt_w32(f, rt_r32(0x00c8b9c0u));
    rt_w32(0x00c8b9c0u, f);
}
/* 0x004cd310 (frame eax, edx, ebx): frame +4 = edx, +8 = ebx, linked first; returns 0x00c8b9c0 */
static uint32_t c_eh_push(const uint32_t* a, int n) { eh_link(W(0), W(1), W(2)); return 0x00c8b9c0u; }
/* 0x004cd329: unlinks the first frame; returns the new first */
static uint32_t c_eh_pop(const uint32_t* a, int n)
{
    (void)a; (void)n;
    uint32_t f = rt_r32(rt_r32(0x00c8b9c0u));
    rt_w32(0x00c8b9c0u, f);
    return f;
}
/* 0x004cbc71 (memory eax, frame edx, descriptor ebx): an array construction frame: +0x10 = 2,
 * +0x14 = descriptor, +0x18 = [descriptor + 9], +0x1c = 0, +0x0c = memory, linked with
 * +4 = frame + 0x10, +8 = 0; returns the chain head 0x00c8b9c0 */
static uint32_t c_eh_array_frame(const uint32_t* a, int n)
{
    uint32_t f = W(1), d = W(2);
    rt_w32(f + 0x10, 2);
    rt_w32(f + 0x14, d);
    rt_w32(f + 0x18, rt_r32(d + 9));
    rt_w32(f + 0x1c, 0);
    rt_w32(f + 0x0c, W(0));
    eh_link(f, f + 0x10, 0);
    return 0x00c8b9c0u;
}

/* ---- the application object helpers (0x004c45c0 ...): ports of the original ---- */

static uint32_t imp(const char* name) { return rt_import_slot(name); }
static uint32_t call_imp1(const char* name, uint32_t x) { uint32_t v[1] = {x}; return rt_call(imp(name), v, 1); }

/* 0x004c465c (o eax): o+0 = GetCommandLineA(), o+4 = argc (<= 10), o+8.. = argv: the command
 * line split in place at blanks outside quotes */
static void app_split(uint32_t o)
{
    uint32_t p = rt_call(imp("KERNEL32.dll!GetCommandLineA"), NULL, 0);
    rt_w32(o + 4, 0);
    rt_w32(o, p);
    int q = 0;
    while (rt_r8(p) && (int32_t)rt_r32(o + 4) < 10) {
        rt_w32(o + 8 + 4 * rt_r32(o + 4), p);
        while (rt_r8(p) && (rt_r8(p) != ' ' || q == 1)) {
            if (rt_r8(p) == '"') q = 1 - q;
            p++;
        }
        if (rt_r8(p) == ' ') {
            rt_w8(p, 0);
            p++;
            while (rt_r8(p) == ' ') p++;
        }
        rt_w32(o + 4, rt_r32(o + 4) + 1);
    }
}
/* 0x004c45c0 (o eax): the command line split (above), the program's directory (argv[0] up to
 * its last '\', without a leading quote) to o+0x30 (not terminated, as the original: the
 * object is zeroed), its first byte also to o+0x1c0; screen width / height and the screen's bits
 * per pixel to o+0x1c4 / +0x1c8 / +0x1cc */
static uint32_t c_app_init(const uint32_t* a, int n)
{
    uint32_t o = W(0);
    app_split(o);
    uint32_t s = rt_r32(o + 8);
    int32_t len = (int32_t)call_imp1("KERNEL32.dll!lstrlenA", s);
    if (rt_r8(s) == '"') { len--; s++; }
    for (uint32_t p = s + (uint32_t)len; len > 0 && rt_r8(p) != '\\'; p--) len--;
    memmove(rt_ptr(o + 0x30), rt_ptr(s), (size_t)len);
    rt_w8(o + 0x1c0, rt_r8(o + 0x30));
    rt_w32(o + 0x1c4, call_imp1("USER32.dll!GetSystemMetrics", 0));
    rt_w32(o + 0x1c8, call_imp1("USER32.dll!GetSystemMetrics", 1));
    uint32_t dc = call_imp1("USER32.dll!GetDC", 0);
    uint32_t v[2] = {dc, 12};
    rt_w32(o + 0x1cc, rt_call(imp("GDI32.dll!GetDeviceCaps"), v, 2));
    uint32_t r[2] = {0, dc};
    rt_call(imp("USER32.dll!ReleaseDC"), r, 2);
    return o;
}
/* 0x004c4558 (slot eax, name edx): FreeLibrary(*slot) if set, *slot = LoadLibraryA(name) */
static uint32_t c_lib_load(const uint32_t* a, int n)
{
    uint32_t slot = W(0);
    if (rt_r32(slot)) { call_imp1("KERNEL32.dll!FreeLibrary", rt_r32(slot)); rt_w32(slot, 0); }
    uint32_t h = call_imp1("KERNEL32.dll!LoadLibraryA", W(1));
    rt_w32(slot, h);
    return h;
}
/* 0x004c4584 (slot eax): FreeLibrary(*slot) if set, *slot = 0; its answer (0 when unset) */
static uint32_t c_lib_free(const uint32_t* a, int n)
{
    uint32_t slot = W(0);
    if (!rt_r32(slot)) return 0;
    uint32_t r = call_imp1("KERNEL32.dll!FreeLibrary", rt_r32(slot));
    rt_w32(slot, 0);
    return r;
}
static uint32_t c_app_argc(const uint32_t* a, int n) { return rt_r32(W(0) + 4); }   /* 0x004c4718 */
static uint32_t c_app_argv(const uint32_t* a, int n) { return W(0) + 8; }           /* 0x004c471c */

/* ---- the formatter 0x004c92ec (buffer eax, format edx, &va ebx, writer ecx) ---- */

typedef struct { uint32_t va; } Va;
static uint32_t va_next(void* u) { Va* v = u; uint32_t w = rt_r32(v->va); v->va += 4; return w; }
static uint32_t c_vformat(const uint32_t* a, int n)
{
    char buf[4096];
    if (!W(0) || !W(1)) rt_stop("0x004c92ec vformat without a buffer or format (eax / edx 0)", W(1));
    Va v = {rt_r32(W(2))};
    int len = w32_format(buf, sizeof buf, w32_str(W(1)), va_next, &v);
    if (len > (int)sizeof buf - 1) len = (int)sizeof buf - 1;
    memcpy(rt_ptr(W(0)), buf, (size_t)len);   /* the writer only stores the characters: done here */
    rt_w32(W(2), v.va);
    return (uint32_t)len;
}

/* the runtime's hook slots 0x004e89c4 .. 0x004e8a04 (thread data, the heap / file / stream locks,
 * the exit hooks): ports call the slot address (the original's call [slot]); the binding calls
 * whatever the slot holds now (mostly the lone ret 0x004bc7df), with the registers as given.
 * The locks are no-ops in the original's single threaded set up; the host lock covers threads. */
#define HOOK(addr) static uint32_t hook_##addr(const uint32_t* a, int n) { return rt_call(rt_r32(addr##u), a, n); }
HOOK(0x004e89c4) HOOK(0x004e89c8) HOOK(0x004e89cc) HOOK(0x004e89d0) HOOK(0x004e89d4) HOOK(0x004e89d8)
HOOK(0x004e89dc) HOOK(0x004e89e0) HOOK(0x004e89e4) HOOK(0x004e89e8) HOOK(0x004e89ec) HOOK(0x004e89f0)
HOOK(0x004e89f4) HOOK(0x004e89f8) HOOK(0x004e89fc) HOOK(0x004e8a00) HOOK(0x004e8a04)

void rt_bind_watcom_crt(void)
{
    static const struct { uint32_t a; RtFn f; } hooks[] = {
        {0x004e89c4u, hook_0x004e89c4}, {0x004e89c8u, hook_0x004e89c8}, {0x004e89ccu, hook_0x004e89cc},
        {0x004e89d0u, hook_0x004e89d0}, {0x004e89d4u, hook_0x004e89d4}, {0x004e89d8u, hook_0x004e89d8},
        {0x004e89dcu, hook_0x004e89dc}, {0x004e89e0u, hook_0x004e89e0}, {0x004e89e4u, hook_0x004e89e4},
        {0x004e89e8u, hook_0x004e89e8}, {0x004e89ecu, hook_0x004e89ec}, {0x004e89f0u, hook_0x004e89f0},
        {0x004e89f4u, hook_0x004e89f4}, {0x004e89f8u, hook_0x004e89f8}, {0x004e89fcu, hook_0x004e89fc},
        {0x004e8a00u, hook_0x004e8a00}, {0x004e8a04u, hook_0x004e8a04},
    };
    for (size_t i = 0; i < sizeof hooks / sizeof hooks[0]; i++) rt_bind(hooks[i].a, hooks[i].f, "crt_hook_slot");
    rt_bind(0x004c47e8u, c_setjmp, "crt_setjmp");
    rt_bind(0x004c4827u, c_longjmp, "crt_longjmp");
    rt_bind(0x004cbb33u, c_heap_grow, "crt_heap_grow");
    rt_bind(0x004cb87cu, c_heap_alloc, "crt_heap_alloc_in");
    rt_bind(0x004cb924u, c_heap_free, "crt_heap_free_in");
    rt_bind(0x004cbb8eu, c_heap_nomem, "crt_heap_nomem");
    rt_bind(0x004c4508u, c_new, "crt_operator_new");
    rt_bind(0x004ca4d3u, l_array_init, "crt_new_array_init");
    rt_bind(0x004cd310u, c_eh_push, "crt_eh_push");
    rt_bind(0x004cd329u, c_eh_pop, "crt_eh_pop");
    rt_bind(0x004cbc71u, c_eh_array_frame, "crt_eh_array_frame");
    rt_bind(0x004c45c0u, c_app_init, "crt_app_init");
    rt_bind(0x004c4558u, c_lib_load, "crt_lib_load");
    rt_bind(0x004c4584u, c_lib_free, "crt_lib_free");
    rt_bind(0x004c4718u, c_app_argc, "crt_app_argc");
    rt_bind(0x004c471cu, c_app_argv, "crt_app_argv");
    rt_bind(0x004c92ecu, c_vformat, "crt_vformat");
}

RT_BIND_AREA(watcom_crt)
