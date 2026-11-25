/*
 * sh_mem.h: game memory as the shell / registry ports see it. The registries keep the
 * original's byte layout and hold 32 bit game addresses (object pointers, scene nodes); ShEnv
 * maps game address ranges to host buffers (WMem, src/weapons/wmem.h) so a pointer stored by
 * one function can be followed by another exactly as the original does. Reads of an unmapped
 * address give 0, writes to one are dropped (the original would fault there).
 */
#ifndef TM2_SH_MEM_H
#define TM2_SH_MEM_H

#include <stdint.h>
#include "../weapons/wmem.h"

typedef struct {
    WMem mem;
    /* other systems */
    void (*anim_step)(void* user, uint32_t node, uint32_t step);       /* animSetStep 0x004a0c94 */
    void (*target_error)(void* user, int32_t type, int32_t instance);   /* 0x004a1f1e("ERROR: TARGET TYPE %d INSTANCE ...") */
    int32_t (*car_taken)(void* user, int32_t car);                      /* 0x00463c9c: -1 when no player has the car */
    uint32_t (*frame_parity)(void* user);                                /* 0x004931f4 */
    void (*net_send)(void* user, int32_t kind, uint32_t buf, int32_t len);  /* 0x00463e38 */
    void (*net_overflow)(void* user);                                   /* 0x004632b0 (message buffer full) */
    uint32_t (*entity_alloc)(void* user, int32_t i);                    /* 0x00492d90: entity game address or 0 */
    void (*entity_init)(void* user, uint32_t ent, uint32_t node);       /* 0x00492de4 */
    void (*entity_insert)(void* user, uint32_t ent);                    /* 0x00492c4c */
    void (*actor_place)(void* user, uint32_t actor);                    /* targetActorPlace 0x004713f0 */
    /* 0x004b9168 (eax, edx, ebx, ecx, then 4 stack words in push order reversed: a[4] is the
     * first stack argument) and 0x004b9240 (eax, edx, ebx): the sound manager's play / stop */
    void (*snd_play)(void* user, const int32_t a[8]);
    void (*snd_stop)(void* user, int32_t a, int32_t b, int32_t c);
    /* any other original function not ported yet: call(fn, n, args) stands for calling TM2.EXE's
     * fn with n arguments (eax, edx, ebx, ecx, then the stack words); returns its eax. A local
     * buffer of the caller is passed as SH_LOCAL(k) (k numbers the buffers of the function). */
    uint32_t (*call)(void* user, uint32_t fn, int n, const uint32_t* args);
    void* user;
    /* the game address of a frame for the caller's local buffers when the ports run as the game
     * (the runtime, src/rt, pushes one: rt_frame_push); 0 under the emulator harnesses, where a
     * local buffer is passed as the placeholder SH_LOCAL(k) and its host bytes are matched by the
     * harness. Port code (not in the original): the original's locals are on its stack. */
    uint32_t local;
} ShEnv;

#define SH_LOCAL(k) (0xfff00000u + (uint32_t)(k))
#define SH_LOCAL_STRIDE 0x400u    /* bytes per numbered buffer in ShEnv.local's frame */

/* the game address of local buffer k (numbered buffers: frame + k * SH_LOCAL_STRIDE) */
static inline uint32_t sh_local(const ShEnv* e, uint32_t k) {
    return e->local ? e->local + k * SH_LOCAL_STRIDE : SH_LOCAL(k);
}
/* the game address of byte off of a frame laid out at the original's stack offsets */
static inline uint32_t sh_local_at(const ShEnv* e, uint32_t off) {
    return e->local ? e->local + off : SH_LOCAL(off);
}
/* the host bytes of the n byte local at game address a: the frame's own bytes when there is a
 * frame (so a callee sees what the port writes), else the port's own buffer */
static inline uint8_t* sh_local_host(const ShEnv* e, uint32_t a, uint8_t* own, uint32_t n) {
    uint8_t* p = e->local ? wmem_ptr(&e->mem, a, n) : NULL;
    return p ? p : own;
}

static inline uint32_t sh_r32(const ShEnv* e, uint32_t a) { const uint8_t* p = wmem_ptr(&e->mem, a, 4); return p ? w_u32(p, 0) : 0; }
static inline uint16_t sh_r16(const ShEnv* e, uint32_t a) { const uint8_t* p = wmem_ptr(&e->mem, a, 2); return p ? w_u16(p, 0) : 0; }
static inline uint8_t sh_r8(const ShEnv* e, uint32_t a) { const uint8_t* p = wmem_ptr(&e->mem, a, 1); return p ? p[0] : 0; }
static inline void sh_w32(const ShEnv* e, uint32_t a, uint32_t v) { uint8_t* p = wmem_ptr(&e->mem, a, 4); if (p) w_s32(p, 0, v); }
static inline void sh_w16(const ShEnv* e, uint32_t a, uint32_t v) { uint8_t* p = wmem_ptr(&e->mem, a, 2); if (p) w_s16(p, 0, v); }
static inline void sh_w8(const ShEnv* e, uint32_t a, uint32_t v) { uint8_t* p = wmem_ptr(&e->mem, a, 1); if (p) p[0] = (uint8_t)v; }

#endif /* TM2_SH_MEM_H */
