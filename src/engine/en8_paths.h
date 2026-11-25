/*
 * en8_paths.h: the game's root paths found at start up, TM2.EXE 0x00498ddc (old name
 * win32_load_icon: wrong, it finds the CD and sets the path slots), in the En2Env of en2_env.h.
 * Frame (En2Env.scratch, the original's offsets): +0 / +0x12c / +0x384 path strings (0x12c),
 * +0x258 the marker file path, +0x4b0 size, +0x4b4 value, +0x4b8 key, +0x4bc file size,
 * +0x4c0 drive string, +0x4c4 "registry incomplete" flag.
 * Verified against the original under emulation (tools/emu/check_en8_paths.sh).
 */
#ifndef TM2_EN8_PATHS_H
#define TM2_EN8_PATHS_H

#include <stdint.h>
#include "en2_env.h"

/* 0x00498ddc (eax = the marker file name, copied to 0x00a3a49c): from the HKLM key: the install
 * directory (0x004e2fbc) and four nonzero values (0x004e2fce / d9 / df / e8); when any is missing
 * the CD ROM drives A..Z are scanned (GetDriveTypeA == DRIVE_CDROM and "<d>:\<name>" opens):
 * none found -> 0x00a3a5d0 = -1 and the error box 0x16 (retry rescans, else returns 0); the CD
 * root is "<d>:" (0x004e2ffb), else "." (0x004e2fff). Then the six path slots 0x00a39c68 + k * 0x12c
 * are each the install directory (value 1) or the CD root (value names 0x004e3013..0x004e3045), the
 * seventh (0x00a3a370, value 0x004e304c) also 0x63 = a directory of its own (0x004e3053); without
 * the key all slots are "." (0x004e3066..72). Returns 1. */
uint32_t en8_paths_setup(En2Env* e, uint32_t name);

#endif
