/*
 * mt_registry.h: the arena object registry, ported from TM2.EXE 0x0046ff40 (PS1 decomp:
 * objRegistryAdd). Level objects are registered by their id into fixed slots of the block
 * 0x0062129c..0x0062163c (u32 object handles), offsets below from 0x0062129c:
 *   0x10 < id < 0x190 (signed): (id - 6) % 16 == 0 -> +0x000 + (id >> 4) * 4,
 *                               else (id - 7) % 16 == 0 -> +0x040 + (id >> 4) * 4 (any object)
 *   then by id (unsigned), only for objects whose type byte (+0) is 9:
 *   0xc -> +0x398; 0x914 -> +0x080; 0x915 -> +0x084; 0x202 / 0x204 / 0x205 -> +0x088 + 4 * u16 +4;
 *   0x2b1 -> +0x0a4; 0xc1f -> +0x0a8; 0x2ac -> +0x0ac + 4k; 0x2b3 -> +0x0c0 + 4k;
 *   0x2ae -> +0x0d4 + 4k; 0x29d -> +0x0e8; 0xfa0 -> +0x344 + 4k; 0xfa5 -> +0x358 + 4k;
 *   0xbbb -> +0x36c + 4k; 0xbbc -> +0x380; 0xbbd -> +0x384; 0xc1d -> +0x388; 0xc80 -> +0x38c
 *   (k = the object's u16 +4); 0x1f4..0x1fe, 0x207, 0x208: appended to the list +0x1b4 (50 max,
 *   count i32 +0x39c), the object's u16 +4 = its index.
 *
 * Verified against the original under emulation (tools/emu/check_match_registry.sh).
 */
#ifndef MT_REGISTRY_H
#define MT_REGISTRY_H

#include <stdint.h>

typedef struct {
    uint8_t* block;             /* 0x0062129c (0x3a0 bytes) */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    void* user;
} MtRegistry;

void mt_obj_registry_add(const MtRegistry* r, uint32_t obj, int32_t id);

#endif /* MT_REGISTRY_H */
