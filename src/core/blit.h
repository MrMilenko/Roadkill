/*
 * blit.h: 16 bit rectangle copy, ported from TM2.EXE 0x00497854.
 * Verified against the original under emulation (tools/emu/check_shell_bg.sh).
 */
#ifndef TM2_BLIT_H
#define TM2_BLIT_H

#include <stdint.h>

/* Copies w x h 16 bit pixels from src (x, y, row pitch in bytes) to dst (x, y, pitch). Each row is
 * w * 2 bytes, copied as (w & 0x7fffffff) >> 1 dwords then (w * 2) & 3 bytes, as the original.
 * Returns 0 when src or dst is NULL (nothing copied), else 1. The original takes an eleventh
 * argument on the stack that it never reads. */
int tm2_blit16(const uint8_t* src, int32_t sx, int32_t sy, uint32_t w, int32_t h, int32_t src_pitch,
               uint8_t* dst, int32_t dx, int32_t dy, int32_t dst_pitch);

#endif /* TM2_BLIT_H */
