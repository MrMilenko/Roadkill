/*
 * sh5_level_load.h: the level and car files of a session, TM2.EXE 0x004753a4 (Ghidra's old name
 * level_load_assets).
 *
 * gs = the game state (see sh2_flow.h: +1 humans, +2 opponents, +5.. human car ids, +0x0d..
 * opponent car ids, +0x1f level). File names are built with sprintf 0x004b27e6 into 0x009f6920
 * from TM2.EXE's format strings and name tables (level 1..12 -> the level's file stem, else
 * the default; car id 0..14 -> the car's stem, else the default; all addresses passed on, no
 * text kept here), then 0x00499408(path buffer, name, root 2). The level DPC goes to 0x0074af98
 * (size into 0x009f691c), the level TPC is registered (0x00496fd8 page 0), the level's second
 * file goes to 0x00626018 (size into 0x009f6918); then the world systems are reset, the level
 * entity is created (0x00494610 with the DPC size and the end of the loaded data, scale 1.0f),
 * the level objects bound (0x0044f130). Then for every human car, then every opponent: its DPC
 * appended after the level's (size added to 0x009f691c, TPC page n registered), its second file
 * appended at 0x00626018 + loaded (0x004502b0 with the page), the DPC base moved on
 * (0x0049ee90). The texture pages are finally applied (0x00497390).
 *
 * Verified against the original under emulation (tools/emu/check_sh5_level_load.sh).
 */
#ifndef TM2_SH5_LEVEL_LOAD_H
#define TM2_SH5_LEVEL_LOAD_H

#include "sh3_screen.h"

/* fb = the game address of the original's frame base (ebp - 0x154: +0 the 0x12c byte path
 * buffer, then the loop locals). */
void sh5_level_load(const ShEnv* e, uint32_t fb, uint32_t gs);

#endif /* TM2_SH5_LEVEL_LOAD_H */
