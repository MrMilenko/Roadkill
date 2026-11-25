/*
 * en6_audio.h: the audio configuration dialog's mixer and control helpers, TM2.EXE
 * 0x004880d8..0x004898f3 (the object of 0x00624edc, see src/shell/sh2_audio.h and
 * src/shell/en3_screens.h en3_audio_config_screen). Runs in the En2Env of src/engine/en2_env.h;
 * the Win32 mixer structures are built at En2Env.scratch at the original's stack offsets.
 *
 * cfg: +0 u32 window, +0x88 i32[5] volumes, +0x9c card combo box, +0x118 / +0x16c / +0x1c0 /
 * +0x214 / +0x268 sliders, +0x48c / +0x658 / +0x824 / +0x9f0 / +0xbbc / +0xd88 slider labels,
 * +0x488 the focused window, +0x1120 the mixer answered, +0x1128 the sound card id.
 * Mixer imports: mixerGetLineInfoA 0x00c90628, mixerGetNumDevs 0x00c905f8,
 * mixerGetLineControlsA 0x00c90624, mixerGetControlDetailsA 0x00c9061c,
 * mixerSetControlDetails 0x00c90630. MIXERLINE: +0x0c dwLineID, +0x18 dwComponentType,
 * +0x1c cChannels, +0x24 cControls, +0x38 szName.
 *
 * Verified against the original under emulation (tools/emu/check_en6_audio.sh).
 */
#ifndef TM2_EN6_AUDIO_H
#define TM2_EN6_AUDIO_H

#include <stdint.h>
#include "../engine/en2_env.h"

/* 0x004890c4 (eax = cfg, edx = component type): the line of that type on the card (for 0x1005
 * when that fails: the mixers from the last one down until one whose line name contains
 * 0x004e2d4e), its volume control (0x50030001) read: returns the value (0 when there is no
 * such control). Frame 0x174 bytes. */
uint32_t en6_mixer_get(En2Env* e, uint32_t cfg, uint32_t type);
/* 0x00489340 (eax = cfg, edx = type, ebx = value): as en6_mixer_get (name 0x004e2d51), the
 * control is set to value on two channels (the control query's result not checked). Returns
 * mixerSetControlDetails' result. */
uint32_t en6_mixer_set(En2Env* e, uint32_t cfg, uint32_t type, uint32_t value);
/* 0x004880d8 (eax = cfg): every slider label unhighlighted (0x004a4a94(label, 0)), then the label
 * of the control whose window is focused (+0x488) highlighted. */
void en6_audio_labels(En2Env* e, uint32_t cfg);
/* 0x00488260 (eax = cfg, edx = combo box): card = the combo's selection (0x00484260(0x1c) when
 * it changed); slider positions * 10 to the mixers (4, 0x1005, 0x1008); with the mixer: the
 * speaker line's volume (0x50030002, slider +0x268) and mute (0x50030003, slider +0x214) set,
 * a failure clears +0x1120. */
void en6_audio_apply_sliders(En2Env* e, uint32_t cfg, uint32_t combo);
/* 0x0048859c (eax = cfg): the reverse: volumes read (cfg +0x88 / +0x90 / +0x8c, +0x94 / +0x98
 * from the speaker line when the mixer answers; a failure reads 0 and clears +0x1120), each
 * slider set to value / 10 (unsigned). */
void en6_audio_read_sliders(En2Env* e, uint32_t cfg);
/* 0x00489800 (eax = obj, edx = flags): the dialog object's scalar deleting destructor: flags bit
 * 2: 0x004a9ff8(obj, 0x004e57b8) then 0x004aa018, returns obj; else the 16 GDI members freed
 * (0x004aa064, each one's result locates the next), the base destructor 0x004a3ff4(.., 1) and with
 * bit 1 delete (0x004aa39c). Returns the base destructor's result. */
uint32_t en6_audio_dialog_destroy(En2Env* e, uint32_t obj, uint32_t flags);

/* 0x004877e4 (eax = cfg): the dialog's loop until cfg +0x1124 (OK) is set: input polled; key
 * 0x1c when the focus is not on the OK button, or input checks 0x14 / 0x15 (then sound 0x1e):
 * OK (sound 0x1d, the volumes saved into the card's row, applied in mode 1, "GameVolume"
 * written to the registry, display rect 0x009f6cd8); a focus change: sound 0x1c, +0x488 and the
 * labels (0x004880d8); every 202nd pass the five sliders to the mixers and read back. */
void en6_audio_dialog_run(En2Env* e, uint32_t cfg);

#endif
