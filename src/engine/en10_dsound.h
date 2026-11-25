/*
 * en10_dsound.h: the DirectSound device's destructor and close, TM2.EXE 0x004b93cc
 * (dsound_device_dtor) and 0x004b9674 (dsound_close), in the En2Env of en2_env.h. COM methods are
 * called through the objects' vtables (stdcall, this first). x87 at 0x127f; the runtime's
 * logarithm helper 0x004c9f14 (fldln2, fyl2x: the natural log) is a boundary: host libm log (the
 * user's decision for the x87 transcendental question).
 *
 * Device: +0 IDirectSound*, +4 the primary IDirectSoundBuffer*, +0x10 i32 volume (percent),
 * +0x18 open flag.
 * Verified against the original under emulation (tools/emu/check_en10_dsound.sh), trig / log
 * through host libm at the x87 helper boundary.
 */
#ifndef TM2_EN10_DSOUND_H
#define TM2_EN10_DSOUND_H

#include <stdint.h>
#include "en2_env.h"

/* the volume in hundredths of a decibel: 0 -> -10000, else chop(1000 * ln(v * 0.01)) (doubles
 * 0x004e3710 / 18), -10000 (0x004e3720) when not above it (NaN included) */
uint32_t en10_dsound_volume_mb(En2Env* e, int32_t v, uint32_t k_scale, uint32_t k_mul, uint32_t k_min);
/* 0x004b93cc (eax = device): when open: the primary's volume restored (0x004b99b4), the primary
 * stopped (+0x48) and, if the pointer changed meanwhile, rewound (SetCurrentPosition +0x34(buf, 0)) through 0x004b94c4;
 * released (+8) when the DirectSound object is there too, the DirectSound object released;
 * open flag and pointers cleared. Returns the device. */
uint32_t en10_dsound_device_dtor(En2Env* e, uint32_t dev);
/* 0x004b9674 (eax = device): the same close with the volume set by the buffer's SetVolume (+0x3c)
 * through 0x004b94c4 and no error filter after the rewind; returns 0 when not open, else the
 * DirectSound Release result (E_FAIL 0x80004005 without one). */
uint32_t en10_dsound_close(En2Env* e, uint32_t dev);

#endif
