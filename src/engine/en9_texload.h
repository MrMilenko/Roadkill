/*
 * en9_texload.h: the hardware 3D texture upload with its memory budget, TM2.EXE 0x00497390, in
 * the En2Env of en2_env.h (frame at En2Env.scratch: +0 the progress text buffer). x87 code at
 * control word 0x127f computed in double in the original's order (-ffp-contract=off).
 *
 * Texture pages 0x00a30c48 + 0x30 * k (k < 0x200): +0 present, +4 / +8 width / height, +0x28
 * fixed size. Budget records 0x00a36c58 + 0x18 * k: +0 page, +4 bytes, +8 bytes after scaling,
 * +0xc / +0x10 float scale x / y. Hardware textures 0x00a2dc48 + 0x18 * page.
 * Verified against the original under emulation (tools/emu/check_en9_texload.sh).
 */
#ifndef TM2_EN9_TEXLOAD_H
#define TM2_EN9_TEXLOAD_H

#include <stdint.h>
#include "en2_env.h"

/* 0x00497390: only with the hardware 3D device (0x004958ac == 1): the texture format (16 bit,
 * 0x004bfe50), each page's bytes = w * h * bytes per pixel fitted (0x004bfc88) and its size
 * after scaling chop(bytes / (sx * sy)); the records sorted (qsort 0x004c1132, 0x004977cc);
 * then up to 9 passes: old textures released, scales doubled (pages without a fixed size, in
 * sorted order, round robin) until the total fits the budget (0x08000000 at first), every page
 * created (0x004bff08) with a "Please Wait" / "Pass %i" / "Loading %i" progress screen whose
 * grey level rises by 255 / count a page; when a creation runs out of video memory
 * (DDERR_OUTOFVIDEOMEMORY 0x8876017c, 0x887602d1) the budget becomes what was created minus
 * 0xc0 per page not created and the next pass starts. Returns 0.
 * ORIGINAL BUG: the creation result is kept across passes and tested after every record, so a
 * pass after an out of memory one stops at its first record without a page (nothing created,
 * budget = -0xc0 * pages) and the scaling loop of the next pass never ends. Also endless: a
 * budget that the pages of fixed size (+0x28) alone exceed. The port keeps the same behaviour. */
uint32_t en9_textures_load(En2Env* e);

#endif
