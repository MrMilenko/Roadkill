/*
 * pf4_dsinit.c: TM2.EXE 0x004b94f4 dsound_init (docs/PLATFORM.md Part C), ported over Pf2Env
 * (pf2_env.h). Emulator diff: tools/emu/check_pf4_dsinit.sh.
 *
 * Device 0x00c850ec: +0x00 IDirectSound, +0x04 primary buffer, +0x0c, +0x10 saved primary volume in
 * percent (-1 = not read yet), +0x18 initialised.
 */
#include "pf2_env.h"

#define R32 pf2_r32
#define W32 pf2_w32
#define E_FAIL 0x80004005u

/* 0x004b94f4 dsound_init(eax device): DirectSoundCreate(NULL, &+0, NULL) (import thunk 0x004d18e4);
   SetCooperativeLevel(main window 0x004a8f34, DSSCL_PRIORITY 2); the primary buffer
   (DSBUFFERDESC {0x14, flags 0xc1 PRIMARYBUFFER | CTRLVOLUME | CTRLPAN... , 0, 0, NULL}); SetFormat
   {PCM, 2 channels, 11025 Hz, 44100 bytes/s, block 4, 16 bit}; Play(LOOPING). +0x0c = 0; volume
   not saved yet (+0x10 == -1): GetVolume -> +0x10 = mB / 100 + 100 (idiv); SetVolume(0);
   +0x18 = 1. Any failure before Play returns its result; then returns Play's result. */
uint32_t pf4_dsound_init(Pf2Env* e, uint32_t dev) {
    uint32_t r = pf2_call3(e, 0x004d18e4u, 0, dev, 0);
    if (r) return r;
    uint32_t ds = R32(e, dev);
    if (ds) r = PF2_C2(e, PF2_IDS, 0x18, ds, pf2_call0(e, 0x004a8f34u), 2);
    if (r) return r;
    if (R32(e, dev) == 0) return E_FAIL;
    uint32_t desc = e->scratch, wfx = e->scratch + 0x20, vol = e->scratch + 0x40;
    pf2_zero(e, desc, 0x14);
    W32(e, desc, 0x14);
    W32(e, desc + 4, 0xc1);
    r = PF2_C3(e, PF2_IDS, 0x0c, R32(e, dev), desc, dev + 4, 0);
    if (r) return r;
    uint32_t b = R32(e, dev + 4);
    if (!b) return E_FAIL;
    pf2_w16(e, wfx, 1);
    pf2_w16(e, wfx + 2, 2);
    W32(e, wfx + 4, 0x2b11);
    W32(e, wfx + 8, 0xac44);
    pf2_w16(e, wfx + 0xc, 4);
    pf2_w16(e, wfx + 0xe, 16);
    pf2_w16(e, wfx + 0x10, 0);
    r = PF2_C1(e, PF2_IDSBUF, 0x38, b, wfx);
    if (r) return r;
    uint32_t play = R32(e, dev + 4) ? PF2_C3(e, PF2_IDSBUF, 0x30, R32(e, dev + 4), 0, 0, 1) : E_FAIL;
    W32(e, dev + 0xc, 0);
    if (R32(e, dev + 0x10) == 0xffffffffu && R32(e, dev + 4)) {
        PF2_C1(e, PF2_IDSBUF, 0x18, R32(e, dev + 4), vol);
        W32(e, dev + 0x10, (uint32_t)((int32_t)R32(e, vol) / 100 + 100));
    }
    if (R32(e, dev + 4)) PF2_C1(e, PF2_IDSBUF, 0x3c, R32(e, dev + 4), 0);
    W32(e, dev + 0x18, 1);
    return play;
}
