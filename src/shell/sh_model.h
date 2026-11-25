/*
 * sh_model.h: the shell's car model display object update, ported from TM2.EXE 0x0047791c.
 * Game memory through ShEnv (sh_mem.h); other systems through ShEnv.call.
 *
 * The display object (as touched): +4 u32 part value, +0xC i16 shown, +0xE i16 camera mode
 * (0: fixed), +0x10 i16 model id (-1: keep), +0x14 i32 zoom (< 0: 8 / |zoom|, else zoom * 8),
 * +0x18 u32 entity (allocated on first use), +0x1C/+0x20/+0x24 i32 position, +0x28 / +0x2C /
 * +0x30 i16 heading / pitch / roll, +0x50 model node, +0xBE + 0x18 k {u8 on, +2 u32 value} for
 * 16 parts (on for k < u8 +0x22C).
 * The entity: +0 i32 model, +4 u32 damage variant (0x00495864(0)), +0xB0 i32[9] matrix,
 * +0xE0 i16[3] angles (pitch, roll, heading), +0xE8 i32[3] position.
 *
 * Verified against the original under emulation (tools/emu/check_shell_screen.sh).
 */
#ifndef TM2_SH_MODEL_H
#define TM2_SH_MODEL_H

#include <stdint.h>
#include "sh_mem.h"

/* 0x0047791c: when shown: entity set up, parts, position, angles, matrix (tm2_build_matrix), the
 * camera (0x004948b4(0) when +0xE != 0, else 0x00494ec4(0, zoom, 1)), then the entity drawn
 * into list (0x004a10fc(list, entity, 0)). */
void sh_model_display_update(const ShEnv* e, uint32_t obj, uint32_t list);

#endif /* TM2_SH_MODEL_H */
