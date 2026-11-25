/*
 * cb2_arcs.h: weaponArcsDraw, TM2.EXE 0x00447748 (PS1 decomp: weaponArcsDraw), the lightning arcs
 * (Mr Grimm / special weapon arcs) between a source position and a target.
 *
 * Arc records, 5 x 0x14 bytes at 0x004ff7f8:
 *   +0x02 i16 owner (passed to damage)   +0x04 i16 timer (> 0: active)   +0x06 i16 kind (0: blue)
 *   +0x08 i16 target car (< 0: none)     +0x0C u32 source position (handle, i32[3])
 *   +0x10 u32 end position (handle, i32[3])
 * 0x004fef68 i32: set to 0 on entry, 1 when any record is active.
 *
 * Per active record (timer >= 1):
 *   - no target: the end becomes a point on a circle around the source:
 *     x = src.x + cos(timer * 400) / 5, y = src.y + sin(timer * 400) / 5, z = src.z, kept in a local
 *     of the function. ORIGINAL BUG: the record's +0x10 is set to the address of that stack local,
 *     which is dead once the function returns (the port stores env->local_handle there and keeps
 *     the point in its own local; see below).
 *   - d = end - src; 16 points: first p15 = end + rand / 0x333 per axis (x, y, z; three rand
 *     calls), p0 = src + (0, 0, 30); then p1..p14 = src + d * i / 16 (truncating) + (0, 0, 30),
 *     each jittered by (rand & 0x3f) - 0x20 in z, then y, then x.
 *   - rand < 0x200: fx_smoke_c2 0x00451274(end, 0); again rand < 0x200: fx_explosion8
 *     0x00450864(end, 0).
 *   - target >= 0 and (timer == 120 or timer <= 0): damage 0x004210e4(target, 10, 0, 2; stack
 *     owner). DECOMPILER TRAP: Ghidra drops the stack argument (the owner, i16 at +0x02).
 *   - timer -= frame_get_ticks60 (16 bit); below 0 -> 0.
 *   - 15 segments p[i] -> p[i+1] drawn through one line primitive (prim_init type 0,
 *     prim_set_38(0, 0) once per call; per segment prim_set_vec 0 and 1, prim_set_rgb 0 with
 *     (b, g, r) = (0xAA, 0xB4, 0xFF) for kind 0 else (0xFF, 0xC8, 0x32), then 0x00491698(ot,
 *     prim)).
 *
 * The primitive helpers are the verified ports in src/engine/en_misc.c and sin / cos are
 * src/math/tm2math.c; the rest goes through Cb2ArcsEnv.
 *
 * Verified against the original under emulation (tools/emu/check_cb2_arcs.sh).
 */
#ifndef TM2_CB2_ARCS_H
#define TM2_CB2_ARCS_H

#include <stdint.h>

typedef struct {
    uint8_t* arcs;                  /* 0x004ff7f8, 5 x 0x14 */
    int32_t* active;                /* 0x004fef68 */
    int32_t* (*resolve)(void* user, uint32_t handle);   /* position handle -> i32[3] */
    uint32_t local_handle;          /* what the original stores for its stack local (esp + 0x10c) */
    uint32_t (*rand)(void* user);                       /* crt_rand 0x004899c2 */
    void (*smoke)(void* user, uint32_t pos);            /* 0x00451274(pos, 0) */
    void (*explosion)(void* user, uint32_t pos);        /* 0x00450864(pos, 0) */
    void (*damage)(void* user, int32_t target, int32_t a, int32_t b, int32_t c, int32_t owner); /* 0x004210e4 */
    int16_t (*ticks)(void* user);                       /* frame_get_ticks60 0x004931d4 */
    void (*draw)(void* user, uint32_t ot, uint8_t* prim);  /* 0x00491698 */
    void* user;
} Cb2ArcsEnv;

void cb2_arcs_draw(const Cb2ArcsEnv* env, uint32_t ot);

#endif /* TM2_CB2_ARCS_H */
