/*
 * en2_env.h: the environment the en2_* ports of TM2.EXE 0x00488000..0x004b0000 run in.
 *
 * Memory: the original's globals and objects are addressed in the game's 32 bit address space;
 * mem(addr) gives the host bytes at that address (contiguous to the end of the object). Calls to
 * code that is not ported here (the DirectDraw / Win32 module above 0x004b0000, Win32 imports,
 * engine functions of other ports) go through call(fn, args, n): fn is the original's address
 * (for an import: the address of its IAT slot, e.g. 0x00c90550 = SendMessageA), args are the
 * Watcom arguments in order EAX, EDX, EBX, ECX, then the stack words (for an import: the stdcall
 * stack words, first argument first). The return value is the callee's EAX. This is the seam the
 * platform layer and the other ports are wired into.
 *
 * scratch: the game address of a 0x400 byte area (en4_io keeps buffers up to 0x268 bytes there) (reachable through mem) where a port keeps the
 * locals the original keeps on its stack and passes to a callee by address (an out parameter of
 * ReadFile, CreateThread, RegCreateKeyExA, ...). The tests treat such addresses as equal.
 */
#ifndef TM2_EN2_ENV_H
#define TM2_EN2_ENV_H

#include <stdint.h>
#include <string.h>

typedef struct En2Env {
    uint8_t* (*mem)(void* user, uint32_t addr);
    uint32_t (*call)(void* user, uint32_t fn, const uint32_t* args, int n);
    void* user;
    uint32_t scratch;
} En2Env;

static inline uint32_t en2_r32(En2Env* e, uint32_t a) { uint32_t v; memcpy(&v, e->mem(e->user, a), 4); return v; }
static inline uint16_t en2_r16(En2Env* e, uint32_t a) { uint16_t v; memcpy(&v, e->mem(e->user, a), 2); return v; }
static inline uint8_t en2_r8(En2Env* e, uint32_t a) { return *e->mem(e->user, a); }
static inline void en2_w32(En2Env* e, uint32_t a, uint32_t v) { memcpy(e->mem(e->user, a), &v, 4); }
static inline void en2_w16(En2Env* e, uint32_t a, uint16_t v) { memcpy(e->mem(e->user, a), &v, 2); }
static inline void en2_w8(En2Env* e, uint32_t a, uint8_t v) { *e->mem(e->user, a) = v; }

static inline uint32_t en2_call0(En2Env* e, uint32_t fn) { return e->call(e->user, fn, 0, 0); }
static inline uint32_t en2_call1(En2Env* e, uint32_t fn, uint32_t a) { return e->call(e->user, fn, &a, 1); }
static inline uint32_t en2_call2(En2Env* e, uint32_t fn, uint32_t a, uint32_t b)
{ uint32_t v[2] = {a, b}; return e->call(e->user, fn, v, 2); }
static inline uint32_t en2_call3(En2Env* e, uint32_t fn, uint32_t a, uint32_t b, uint32_t c)
{ uint32_t v[3] = {a, b, c}; return e->call(e->user, fn, v, 3); }
static inline uint32_t en2_call4(En2Env* e, uint32_t fn, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{ uint32_t v[4] = {a, b, c, d}; return e->call(e->user, fn, v, 4); }
static inline uint32_t en2_call5(En2Env* e, uint32_t fn, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t f)
{ uint32_t v[5] = {a, b, c, d, f}; return e->call(e->user, fn, v, 5); }
static inline uint32_t en2_call6(En2Env* e, uint32_t fn, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t f, uint32_t g)
{ uint32_t v[6] = {a, b, c, d, f, g}; return e->call(e->user, fn, v, 6); }

/* Win32 imports (IAT slots) */
#define EN2_RegCloseKey      0x00c90134u
#define EN2_RegQueryValueExA 0x00c90144u
#define EN2_RegSetValueExA   0x00c90148u
#define EN2_CloseHandle      0x00c902a4u
#define EN2_lstrcpyA         0x00c9031cu
#define EN2_DrawMenuBar      0x00c90498u
#define EN2_GetFocus         0x00c904ccu
#define EN2_GetWindowLongA   0x00c904ecu
#define EN2_InvalidateRect   0x00c904f8u
#define EN2_SendMessageA     0x00c90550u
#define EN2_ShowWindow       0x00c90584u

#define EN2_DISPLAY 0x00c84840u   /* the display object (DirectDraw wrapper) */

#endif
