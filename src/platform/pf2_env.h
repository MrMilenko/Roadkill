/*
 * pf2_env.h: the environment the pf2_* wrapper ports of TM2.EXE 0x004b0000..0x004c0000 run in
 * (the small DirectDraw / Direct3D / DirectSound / Win32 / MCI wrappers of docs/PLATFORM.md).
 *
 * Memory: the original's globals and objects are addressed in the game's 32 bit address space;
 * mem(addr) gives the host bytes at that address (contiguous to the end of the object).
 * Calls go through call(fn, args, n) in the original's order with the original's arguments:
 *   - a Watcom function that is not part of the port: fn = its address, args = EAX, EDX, EBX,
 *     ECX, then the stack words;
 *   - a Win32 import: fn = its IAT slot (PF2_* below), args = the stdcall stack words in order;
 *   - a COM method: fn = PF2_COM(interface, vtable offset), args[0] = the object (this), then
 *     the stdcall arguments.
 * The return value is the callee's EAX. This is the seam the platform backend plugs into (an SDL
 * backend maps each fn to its own implementation; docs/PLATFORM.md "Portable backends").
 *
 * scratch: the game address of a 0x200 byte area (reachable through mem) for the locals the
 * original keeps on its stack and passes by address (a DDSURFACEDESC, a RECT, an out value).
 */
#ifndef PF2_ENV_H
#define PF2_ENV_H

#include <stdint.h>
#include <string.h>

typedef struct Pf2Env {
    uint8_t* (*mem)(void* user, uint32_t addr);
    uint32_t (*call)(void* user, uint32_t fn, const uint32_t* args, int n);
    void* user;
    uint32_t scratch;
} Pf2Env;

/* COM interfaces: fn = PF2_COM(i, slot) */
#define PF2_COM(i, slot) (0xC0000000u | ((uint32_t)(i) << 12) | (uint32_t)(slot))
enum { PF2_IDD2 = 1, PF2_ISURF = 2, PF2_ID3D2 = 3, PF2_IDEV2 = 4, PF2_IVIEW2 = 5, PF2_IMAT2 = 6, PF2_ICLIP = 7,
       PF2_IDS = 8, PF2_IDSBUF = 9, PF2_IUNK = 10 };

static inline uint32_t pf2_r32(Pf2Env* e, uint32_t a) { uint32_t v; memcpy(&v, e->mem(e->user, a), 4); return v; }
static inline uint16_t pf2_r16(Pf2Env* e, uint32_t a) { uint16_t v; memcpy(&v, e->mem(e->user, a), 2); return v; }
static inline uint8_t pf2_r8(Pf2Env* e, uint32_t a) { return *e->mem(e->user, a); }
static inline void pf2_w32(Pf2Env* e, uint32_t a, uint32_t v) { memcpy(e->mem(e->user, a), &v, 4); }
static inline void pf2_w16(Pf2Env* e, uint32_t a, uint16_t v) { memcpy(e->mem(e->user, a), &v, 2); }
static inline void pf2_w8(Pf2Env* e, uint32_t a, uint8_t v) { *e->mem(e->user, a) = v; }
static inline float pf2_rf(Pf2Env* e, uint32_t a) { float v; memcpy(&v, e->mem(e->user, a), 4); return v; }
static inline void pf2_wf(Pf2Env* e, uint32_t a, float v) { memcpy(e->mem(e->user, a), &v, 4); }
static inline void pf2_zero(Pf2Env* e, uint32_t a, uint32_t n) { memset(e->mem(e->user, a), 0, n); }

static inline uint32_t pf2_call0(Pf2Env* e, uint32_t fn) { return e->call(e->user, fn, 0, 0); }
static inline uint32_t pf2_call1(Pf2Env* e, uint32_t fn, uint32_t a) { return e->call(e->user, fn, &a, 1); }
static inline uint32_t pf2_call2(Pf2Env* e, uint32_t fn, uint32_t a, uint32_t b)
{ uint32_t v[2] = {a, b}; return e->call(e->user, fn, v, 2); }
static inline uint32_t pf2_call3(Pf2Env* e, uint32_t fn, uint32_t a, uint32_t b, uint32_t c)
{ uint32_t v[3] = {a, b, c}; return e->call(e->user, fn, v, 3); }
static inline uint32_t pf2_call4(Pf2Env* e, uint32_t fn, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{ uint32_t v[4] = {a, b, c, d}; return e->call(e->user, fn, v, 4); }
static inline uint32_t pf2_call5(Pf2Env* e, uint32_t fn, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t f)
{ uint32_t v[5] = {a, b, c, d, f}; return e->call(e->user, fn, v, 5); }
static inline uint32_t pf2_call6(Pf2Env* e, uint32_t fn, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t f, uint32_t g)
{ uint32_t v[6] = {a, b, c, d, f, g}; return e->call(e->user, fn, v, 6); }

/* COM call helpers: this first */
#define PF2_C0(e, i, s, o) pf2_call1((e), PF2_COM(i, s), (o))
#define PF2_C1(e, i, s, o, a) pf2_call2((e), PF2_COM(i, s), (o), (a))
#define PF2_C2(e, i, s, o, a, b) pf2_call3((e), PF2_COM(i, s), (o), (a), (b))
#define PF2_C3(e, i, s, o, a, b, c) pf2_call4((e), PF2_COM(i, s), (o), (a), (b), (c))
#define PF2_C4(e, i, s, o, a, b, c, d) pf2_call5((e), PF2_COM(i, s), (o), (a), (b), (c), (d))
#define PF2_C5(e, i, s, o, a, b, c, d, f) pf2_call6((e), PF2_COM(i, s), (o), (a), (b), (c), (d), (f))

#define PF2_DISPLAY 0x00c84840u   /* the display object (DirectDraw wrapper) */

#endif /* PF2_ENV_H */
