/*
 * sh2_audio.h: the audio configuration object's destructor, TM2.EXE 0x0048620c (the object of
 * 0x00624edc: +0 u32 window, +0x34 vtable, +0x38.. volumes, the controls from +0x118 up to
 * +0x110c, +0x112c its registry key).
 *
 * Verified against the original under emulation (tools/emu/check_sh2_flow.sh, check_sh2_audio.sh).
 */
#ifndef TM2_SH2_AUDIO_H
#define TM2_SH2_AUDIO_H

#include <stdint.h>
#include "sh2_flow.h"

/* 0x0048620c (Watcom C++ scalar deleting destructor): flags bit 2 (array delete): 0x004a9ff8(obj,
 * 0x004e56ec) and its result to 0x004aa018, returns obj. Else: vtable 0x004e57f4, +0 = 0, the
 * registry key closed (0x004ac590(obj + 0x112c, 0)), every member destroyed from the last one
 * down (0x004aa064 / 0x004a3ff4 with flags 0; each returns the member, the next one is found
 * from that), the base destructor 0x004a6b10(base, 1) and, with flags bit 1, 0x004aa39c (delete).
 * Returns the base destructor's result. */
uint32_t sh2_audio_cfg_destroy(const Sh2Env* e, uint32_t obj, uint32_t flags);
/* 0x004868cc (audio_read_registry; edx unused): the sound card id "SoundCardID" (registry value
 * 0x004e2d03 into cfg +0x1128, written back as 0 when missing), the mixer probed
 * (mixerGetLineInfoA / mixerGetLineControlsA / mixerGetControlDetailsA through the import slots
 * 0x00c90628 / 0x00c90624 / 0x00c9061c on the speaker line, volume control 0x50030002):
 * cfg +0x1120 = 1 when all answer. Then "GameVolume" (0x004e2d1b, 0x3c bytes) into cfg +0x4c;
 * when read, the current volumes (cfg +0x38, 20 bytes) are copied into the card's row and 4 and
 * 8 bytes after it (ORIGINAL BUG: the intended rows are 20 bytes apart; kept), the defaults set
 * (0x00486b44) and the table written back (0x004ac65c). The Win32 structures are built in a 0x180
 * byte frame at the original's stack offsets, passed as SH_LOCAL(offset) with Sh2Env.local[0] =
 * the frame. */
void sh2_audio_read_settings(const Sh2Env* e, uint32_t cfg);
/* 0x00486bb4 (audio_apply_volume): volumes v[5] from cfg +0x38 (mode 0), the card's row cfg +0x4c
 * + card * 20 (mode 1) or cfg +0x88 (mode 2); 0x00489340(cfg, 4, v[0]), (cfg, 0x1005, v[2]),
 * (cfg, 0x1008, v[1]); then, while the mixer answers (cfg +0x1120, cleared on a failure), the
 * speaker line's volume control (0x50030002) set to v[3] and its mute control (0x50030003) to
 * v[4] on both channels (mixerSetControlDetails, import slot 0x00c90630).
 * ORIGINAL BUG: another mode leaves v[] uninitialized (stack contents); the port reads its zeroed
 * frame there (the test zeroes the original's stack the same way). */
void sh2_audio_apply_volume(const Sh2Env* e, uint32_t cfg, uint32_t mode);

#endif /* TM2_SH2_AUDIO_H */
