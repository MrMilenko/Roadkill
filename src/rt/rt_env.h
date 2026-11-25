/*
 * rt_env.h: building the ports' environments on the runtime (rt.h). Each adapter gives the port
 * a fresh frame for its game address locals (scratch / frame base) and pops it on return.
 */
#ifndef TM2_RT_ENV_H
#define TM2_RT_ENV_H

#include "rt.h"
#include "engine/en2_env.h"
#include "platform/pf2_env.h"

#define RT_SCRATCH 0x800u   /* the largest scratch any En2Env / Pf2Env port asks for */

static inline En2Env rt_en2env(void)
{
    En2Env e = {rt_mem_cb, rt_call_cb, NULL, rt_frame_push(RT_SCRATCH)};
    return e;
}
static inline void rt_en2env_done(void) { rt_frame_pop(RT_SCRATCH); }

static inline Pf2Env rt_pf2env(void)
{
    Pf2Env e = {rt_mem_cb, rt_call_cb, NULL, rt_frame_push(RT_SCRATCH)};
    return e;
}

/* staging: a host buffer at a game address for one call (copy in, call, copy out). Push n bytes
 * holding `host` (when not NULL); rt_stage_out copies them back; pop with rt_frame_pop(n). */
static inline uint32_t rt_stage_in(const void* host, uint32_t n)
{
    uint32_t a = rt_frame_push(n);
    if (host) memcpy(rt_ptr(a), host, n);
    return a;
}
static inline void rt_stage_out(void* host, uint32_t a, uint32_t n) { if (host) memcpy(host, rt_ptr(a), n); }

/* calls with 1 .. 4 register arguments (EAX, EDX, EBX, ECX) */
static inline uint32_t rt_call1(uint32_t fn, uint32_t a0) { return rt_call(fn, &a0, 1); }
static inline uint32_t rt_call2(uint32_t fn, uint32_t a0, uint32_t a1) { uint32_t v[2] = {a0, a1}; return rt_call(fn, v, 2); }
static inline uint32_t rt_call3(uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2) { uint32_t v[3] = {a0, a1, a2}; return rt_call(fn, v, 3); }
static inline uint32_t rt_call4(uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) { uint32_t v[4] = {a0, a1, a2, a3}; return rt_call(fn, v, 4); }

/* the i-th argument, 0 when the caller passed fewer */
static inline uint32_t rt_arg(const uint32_t* a, int n, int i) { return i < n ? a[i] : 0; }

#endif
