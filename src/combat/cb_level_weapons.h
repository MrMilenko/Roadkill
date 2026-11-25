/*
 * cb_level_weapons.h: binds the level's weapon / object trees, TM2.EXE 0x0044f130 (old name
 * level_init_weapons).
 *
 *   base = dpc_get_base 0x0049ee98 (game address of the loaded level DPC); i32 0x00500028 = base,
 *   0x0050002c = base +0x0C (the file's load address), 0x00500018 = base +0x10; 0x004a1314(+0x10);
 *   0x00500020 = 0x00500024 = 0; list = base + 4 * ((base +0x14 - file) / 4) (C division);
 *   n = list[0] (unsigned); for each of the n entries after it: entry = base + 4 * ((entry - file)
 *   / 4) (rewritten in place, globals read again each time); 0 ends the walk; dpc_relocate_tree
 *   0x0049eee0(entry, file, 0) and 0x004a15d4(entry, +0x10 value, worldLoadObject 0x0044f20c).
 *
 * Verified against the original under emulation (tools/emu/check_combat_lweapons.sh).
 */
#ifndef TM2_CB_LEVEL_WEAPONS_H
#define TM2_CB_LEVEL_WEAPONS_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    uint32_t (*dpc_base)(void* user);                                     /* 0x0049ee98 */
    void (*fn_4a1314)(void* user, uint32_t v);                            /* 0x004a1314 */
    void (*relocate)(void* user, uint32_t tree, uint32_t file, int32_t a); /* 0x0049eee0 */
    /* 0x004a15d4(tree, v, handler): the original passes worldLoadObject 0x0044f20c */
    void (*walk)(void* user, uint32_t tree, uint32_t v, uint32_t handler);
    void* user;
} CbLevelWeaponsEnv;

void cb_level_init_weapons(const CbLevelWeaponsEnv* e);

#endif /* TM2_CB_LEVEL_WEAPONS_H */
