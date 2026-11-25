/*
 * rt.h: the runtime the ported functions run in when they run as the game (tm2), not under the
 * emulator diff.
 *
 * Every port reaches game memory by game address (En2Env / Pf2Env mem(), ShEnv's WMem, or host
 * pointers to the original's globals) and reaches every other function by the original's address
 * (call(fn, args, n): args are EAX, EDX, EBX, ECX, then the stack words; a Win32 import is called
 * by its IAT slot with its stdcall words; a COM method by PF2_COM(interface, vtable offset)).
 * The runtime gives both one meaning:
 *
 *   memory  one host buffer that is the game's 32 bit address space below RT_SIZE. TM2.EXE's
 *           sections are copied into it at their addresses from the player's own file at start
 *           up (no game data in the source). Above the image: the heap the game's allocator
 *           grows into, and a stack of frames for the locals a port keeps at game addresses
 *           (En2Env.scratch, the screens' frame bases).
 *   calls   a table from original address to an adapter that unpacks the arguments and calls the
 *           port (src/rt/bind_*.c), or a host service (the Watcom C runtime above 0x004c0000,
 *           Win32 / DirectX / MCI on SDL). An address with no entry stops the run and prints
 *           the chain of calls that led to it: the next thing to bind.
 *
 * This is port code (not a decompiled function); where it differs from the original, it says so.
 */
#ifndef TM2_RT_H
#define TM2_RT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define RT_SIZE        0x04000000u   /* 64 MB of game address space */
#define RT_HEAP_BASE   0x01000000u   /* the game heap grows up from here ... */
#define RT_HEAP_END    0x03000000u
#define RT_STACK_END   0x03000000u   /* ... the frame stack grows down from RT_FAKE_BASE to here */
#define RT_FAKE_BASE   0x03f00000u   /* addresses handed out for fake COM vtables / host objects */

extern uint8_t* rt_base;

static inline uint8_t* rt_ptr(uint32_t a) { return rt_base + (a & (RT_SIZE - 1)); }
static inline uint32_t rt_addr(const void* p) { return (uint32_t)((const uint8_t*)p - rt_base); }
static inline uint32_t rt_r32(uint32_t a) { uint32_t v; memcpy(&v, rt_ptr(a), 4); return v; }
static inline uint16_t rt_r16(uint32_t a) { uint16_t v; memcpy(&v, rt_ptr(a), 2); return v; }
static inline uint8_t rt_r8(uint32_t a) { return *rt_ptr(a); }
static inline void rt_w32(uint32_t a, uint32_t v) { memcpy(rt_ptr(a), &v, 4); }
static inline void rt_w16(uint32_t a, uint16_t v) { memcpy(rt_ptr(a), &v, 2); }
static inline void rt_w8(uint32_t a, uint8_t v) { *rt_ptr(a) = v; }

/* 0 on success; reads the player's TM2.EXE and copies its sections into the address space */
int rt_init(const char* exe_path);
void rt_shutdown(void);

/* frames: the game address of n zeroed bytes on the frame stack; pop the same n, in reverse order */
uint32_t rt_frame_push(uint32_t n);
/* a game thread (CreateThread's host side) calls this first: its own frame slice and call chain */
void rt_thread_begin(void);
void rt_frame_pop(uint32_t n);

/* heap: n bytes (16 aligned, zeroed) above the image; 0 when full. The game's own allocator
 * (crt_malloc, ported) grows into this through the runtime's sbrk 0x004cb87c. */
uint32_t rt_heap_grow(uint32_t n);

/* binding tables: a src/rt/bind_<area>.c or src/host/<x>.c file ends with RT_BIND_AREA(area) after
 * defining `void rt_bind_<area>(void)`; rt_bind_all() runs every registered table at start up */
void rt_register_area(void (*fn)(void), const char* name);
#define RT_BIND_AREA(a) \
    __attribute__((constructor)) static void rt_register_##a(void) { rt_register_area(rt_bind_##a, #a); }
void rt_bind_all(void);

/* calls */
typedef uint32_t (*RtFn)(const uint32_t* a, int n);
void rt_bind(uint32_t addr, RtFn fn, const char* name);
void rt_bind_weak(uint32_t addr, RtFn fn, const char* name);
/* the number of arguments a bound function reads (registers + stack words, compact); a call that
 * passes fewer prints one warning (TM2_ARITY=stop stops the run): the callee would read a register
 * the ported caller only left in place in the original */
void rt_set_arity(uint32_t addr, int k);   /* yields to an rt_bind of the same address */
uint32_t rt_call(uint32_t fn, const uint32_t* args, int n);
const char* rt_name(uint32_t addr);   /* the bound name, or NULL */
int rt_bind_count(void);
#include <stdio.h>
void rt_list_binds(FILE* f);   /* every binding, sorted (TM2_LIST_BINDS=1 at start up) */

/* imports, from the player's TM2.EXE import table: "DLL!name" or just "name" */
uint32_t rt_import_slot(const char* name);          /* the IAT slot, 0 when not imported */
const char* rt_import_name(uint32_t slot);          /* "KERNEL32.DLL!lstrcpyA", or NULL */
void rt_bind_import(const char* name, RtFn fn);     /* binds the slot; no-op when not imported */

/* the En2Env / Pf2Env style callbacks (user is unused) */
uint8_t* rt_mem_cb(void* user, uint32_t addr);
uint32_t rt_call_cb(void* user, uint32_t fn, const uint32_t* args, int n);
uint32_t rt_call_n_cb(void* user, uint32_t fn, int n, const uint32_t* args);   /* ShEnv order */

/* a fresh address for a fake object or vtable entry the host implements (COM, handles) */
uint32_t rt_fake_alloc(uint32_t n);

/* tracing: TM2_TRACE=1 prints every call; the chain is printed on a stop */
void rt_stop(const char* why, uint32_t addr);

#endif /* TM2_RT_H */
