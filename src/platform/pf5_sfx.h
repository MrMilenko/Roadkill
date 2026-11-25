/*
 * pf5_sfx.h: the sound effect voice list of TM2.EXE (docs/PLATFORM.md "Sound: voice list"):
 *   0x004bb9c4 sfx_start       find or make the slot for (bank program, key), set volume / pan /
 *                              pitch, flags |= 0x1d; 1 if no slot, else 0 (ret 8)
 *   0x004bbdcc sfx_slot_find   find (program, key), else insert a new slot (evicting a
 *                              "negative" last slot when all 50 are in use)
 *   0x004bc31c sfx_slot_alloc  first free slot 2..51, linked before a node; -1 if full
 *   0x004bc4e4 sfx_update      per frame: drop stopped / finished sounds, apply pending changes
 *                              within the voice budget, drop the rest (its body is shared with an
 *                              unlisted twin at 0x004bc4ec that takes the list in eax; the code
 *                              lies in 0x004bbfbd..0x004bc2b6 and 0x004bc4e4..0x004bc5c8)
 *   0x004bc5cc sfx_stop_all    stop every slot of the list 0x00c85124 and empty it
 *
 * List (0x00c85124): +0x000 i32 count, slots i = 2..51 at +0x04 + 0x28 i (nodes 0 = head, 1 =
 * tail are sentinels), +0x824 + 4 i in use (1), +0x8f4 + 4 i next, +0x9c4 + 4 i prev, +0xa94 i32
 * voice budget. Slot: +0x00 program, +0x04 instance, +0x08 key, +0x0c u8 flags (1 play, 2 stop,
 * 4 volume, 8 pan, 0x10 pitch), +0x10 volume, +0x14 pan, +0x18 pitch, +0x1c voice count, +0x20 /
 * +0x24 the signed order pair (from the program's first tone +0x0c / +0x08). Instance: +0x10
 * voice count, +0x14 voices (0x44 bytes, +0x40 sound buffer). Program: +0x00 i32 tone count,
 * +0x04 tones (tone +0x08 / +0x0c / +0x10). Bank: +0x04 programs (12 bytes each).
 *
 * Callees through env->call: 0x004ba67c(voice) release, 0x004b9898(0x00c850ec, buffer) stop and
 * rewind, 0x004b999c(0x00c850ec, buffer, &status) status, 0x004bbaf8(slot) apply (ported in
 * pf_sfx.c), 0x004baac0(instance) stop all voices.
 *
 * ORIGINAL BUG kept: in sfx_update an instance with no voices is judged by the "playing" word
 * left on the stack by the last status read (or what was there before the first): the port takes
 * that initial word as an argument (`stale`, 0 = "not playing" in a real port) and keeps it across
 * slots as the original does. A program with no tones makes sfx_slot_find read address 0x10.
 *
 * Verified against the original under emulation (tools/emu/check_pf5_sfx.sh).
 */
#ifndef PF5_SFX_H
#define PF5_SFX_H

#include "pf2_env.h"

#define PF5_SFX_LIST 0x00c85124u

uint32_t pf5_sfx_start(Pf2Env* e, uint32_t bank, uint32_t prog, uint32_t key, uint32_t volume, uint32_t pan,
                       uint32_t pitch);
uint32_t pf5_sfx_slot_find(Pf2Env* e, uint32_t list, uint32_t program, uint32_t key);
uint32_t pf5_sfx_slot_alloc(Pf2Env* e, uint32_t list, uint32_t before);
uint32_t pf5_sfx_update(Pf2Env* e, uint32_t stale);
uint32_t pf5_sfx_update_list(Pf2Env* e, uint32_t list, uint32_t stale);
uint32_t pf5_sfx_stop_all(Pf2Env* e);

#endif /* PF5_SFX_H */
