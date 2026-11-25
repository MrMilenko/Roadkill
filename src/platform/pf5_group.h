/*
 * pf5_group.h: group_read, TM2.EXE 0x004bb03c (a sound bank program's voice group loader).
 * eax = group, edx = file handle, ebx = voice array (0x18 bytes each), ecx = sample array
 * (0x44 bytes each); returns the bytes read.
 *
 * read 0x004c9ff0(fd, group, 4) -> the voice count at +0x00 (its result counts unless -1);
 * +0x08 = new 0x004c4508(count * 4); +0x04 = the voice array; voice_read 0x004bac7c(voices, fd,
 * samples) reads voice 0 (its result is added). Voices 1..count-1 (count re-read each round) are
 * copies of voice 0: +0x00..+0x10 copied, +0x14 = the sample cursor, which moves on by voice 0's
 * sample count (+0x10) * 0x44 before each copy; each of its samples gets voice 0's sample fields
 * +0x04..+0x20, +0x28 and +0x00 (in that order; the format / buffer fields are filled later).
 *
 * Verified against the original under emulation (tools/emu/check_pf5_group.sh).
 */
#ifndef PF5_GROUP_H
#define PF5_GROUP_H

#include "pf2_env.h"

uint32_t pf5_group_read(Pf2Env* e, uint32_t group, uint32_t fd, uint32_t voices, uint32_t samples);

#endif /* PF5_GROUP_H */
