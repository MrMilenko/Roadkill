/*
 * sh_sound.h: the game's sound request wrappers, ported from TM2.EXE 0x00472ed0..0x00473104
 * (PS1 decomp: soundMgrStop, soundMgrStopGeneric, soundMgrPlay* in sound_manager_*.c).
 * Each asks the sound manager whether sound is on (0x004b9150: u32 0x00c850e0 != 0), then calls
 * its play (0x004b9168, ShEnv.snd_play) or stop (0x004b9240, ShEnv.snd_stop). Sound 9 (a
 * looping engine sound) is played / stopped as 0x11 with the shared loop flag 0x00621188 set /
 * cleared; sounds 0, 9 and 0x20 also set their own loop flags (0x00621184, 0x00621186, 0x00621182)
 * (the setters 0x0046fc58 / 0x0046fc68 / 0x0046fc78 / 0x0046fc88 are single byte stores).
 * Play arguments: a[0..3] = eax, edx, ebx, ecx, a[4..7] = the four stack words (the last is 13
 * in every caller here; the decompiled C drops it).
 *
 * Verified against the original under emulation (tools/emu/check_shell_sound.sh).
 */
#ifndef TM2_SH_SOUND_H
#define TM2_SH_SOUND_H

#include <stdint.h>
#include "sh_mem.h"

/* 0x00472ed0 (two used stack arguments p5, p6 plus an unused third; ret 0xc): car sounds
 * 0x24..0x3c (even) and 0x41 play with ebx -1, ecx 99, a[6] = p5; sound 0x19 with p5 != 99 plays
 * with ecx 0x4b, a[6] = 99, ebx = p6; else ecx = p5, a[6] = 0, ebx = p6. */
void sh_sound_play_car(const ShEnv* e, int32_t a, int32_t id, int32_t b, int32_t c, int32_t p5, int32_t p6);
/* 0x00472f74: stop(a, id, b) */
void sh_sound_stop(const ShEnv* e, int32_t a, int32_t id, int32_t b);
/* 0x00472f8c (stack p5, p6; ret 8) */
void sh_sound_play_loop(const ShEnv* e, int32_t a, uint32_t id, int32_t b, int32_t c, int32_t p5, int32_t p6);
/* 0x0047304c */
void sh_sound_stop_loop(const ShEnv* e, int32_t a, uint32_t id, int32_t b);
/* 0x004730c8 (stack p5, p6; ret 8) */
void sh_sound_play_generic(const ShEnv* e, int32_t a, int32_t id, int32_t b, int32_t c, int32_t p5, int32_t p6);
/* 0x00473104 */
void sh_sound_stop_generic(const ShEnv* e, int32_t a, int32_t id, int32_t b);

#endif /* TM2_SH_SOUND_H */
