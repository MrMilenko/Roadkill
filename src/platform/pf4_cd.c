/*
 * pf4_cd.c: TM2.EXE's CD audio open / play / aux device (MCI and the aux API; docs/PLATFORM.md
 * Part C), ported over Pf2Env (pf2_env.h). Emulator diff: tools/emu/check_pf4_cd.sh.
 * Player 0x00c850bc: +0x00 MCI device id, +0x04 / +0x08 from / to track, +0x0c aux volume saved at
 * open, +0x10 monitor thread, +0x14 thread id, +0x18 open, +0x1c use aux volume.
 * Only the MCI parameter fields named below are set by the original (the rest is stack garbage).
 */
#include "pf2_env.h"

#define R32 pf2_r32
#define W32 pf2_w32
#define I_mciSendCommandA 0x00c90614u
#define I_auxGetNumDevs 0x00c905f8u
#define I_auxGetDevCapsA 0x00c905f4u
#define I_auxGetVolume 0x00c905fcu
#define I_wsprintfA 0x00c90658u
#define I_CreateThread 0x00c902b4u

/* 0x004b8eac cd_find_aux_device(eax player, edx kind): not open: 0; kind 0: the MCI device id;
   else the first aux device whose AUXCAPS wTechnology (+0x28) is AUXCAPS_CDAUDIO (1), else the
   first AUXCAPS_AUXIN (2); none: 1 (not -1, though the callers test for < 0) */
uint32_t pf4_cd_find_aux_device(Pf2Env* e, uint32_t cd, uint32_t kind) {
    if (R32(e, cd + 0x18) == 0) return 0;
    if (kind == 0) return R32(e, cd);
    uint32_t caps = e->scratch + 0x80;
    int32_t n = (int32_t)pf2_call0(e, I_auxGetNumDevs);
    for (uint32_t t = 1; t <= 2; t++)
        for (int32_t i = 0; i < n; i++)
            if (pf2_call3(e, I_auxGetDevCapsA, (uint32_t)i, caps, 0x30) == 0 && pf2_r16(e, caps + 0x28) == t)
                return (uint32_t)i;
    return 1;
}

/* 0x004b858c cd_open(eax player, edx drive letter): 'A'..'Z': MCI_OPEN_PARMS zeroed, device type
   (LPCSTR)0x204 (MCI_DEVTYPE_CD_AUDIO), element = "X:" (wsprintfA 0x004e36f4), flags 0x3302;
   other values: type "cdaudio" (0x004e36f8), wDeviceID 0, flags MCI_OPEN_TYPE 0x2000 (rest not
   cleared). Failure: open = 0, returns it. Success: open = 1, id = wDeviceID, to -1, from 1; the
   aux volume of cd_find_aux_device(1) (if >= 0) into +0x0c; +0x10 = 0; MCI_SET time format TMSF
   (10); returns MCI_SET's result. */
uint32_t pf4_cd_open(Pf2Env* e, uint32_t cd, uint32_t drive) {
    uint32_t p = e->scratch, buf = e->scratch + 0x20, set = e->scratch + 0x40;
    uint32_t r;
    if ((int32_t)drive >= 0x41 && (int32_t)drive <= 0x5a) {
        pf2_zero(e, p, 0x14);
        pf2_call3(e, I_wsprintfA, buf, 0x004e36f4u, drive);
        W32(e, p + 8, 0x204);
        W32(e, p + 0xc, buf);
        r = pf2_call4(e, I_mciSendCommandA, 0, 0x803, 0x3302, p);
    } else {
        W32(e, p + 4, 0);
        W32(e, p + 8, 0x004e36f8u);
        r = pf2_call4(e, I_mciSendCommandA, 0, 0x803, 0x2000, p);
    }
    if (r) {
        W32(e, cd + 0x18, 0);
        return r;
    }
    W32(e, cd + 0x18, 1);
    W32(e, cd, R32(e, p + 4));
    W32(e, cd + 8, 0xffffffffu);
    W32(e, cd + 4, 1);
    uint32_t dev = pf4_cd_find_aux_device(e, cd, 1);
    if ((int32_t)dev >= 0) pf2_call2(e, I_auxGetVolume, dev, cd + 0xc);
    W32(e, cd + 0x10, 0);
    W32(e, set + 4, 10);
    return pf2_call4(e, I_mciSendCommandA, R32(e, cd), 0x80d, 0x400, set);
}

/* 0x004b86dc cd_play(eax player): not open: 0. from > 0: MCI_FROM with (byte) from. The track
   count (MCI_STATUS NUMBER_OF_TRACKS 3; dwReturn taken even when the call failed); to > from and
   to <= count (unsigned): MCI_TO with (byte) to. MCI_PLAY; on success the monitor thread 0x004b902c
   is started once. Returns MCI_PLAY's result. */
uint32_t pf4_cd_play(Pf2Env* e, uint32_t cd) {
    if (R32(e, cd + 0x18) == 0) return 0;
    uint32_t st = e->scratch, pl = e->scratch + 0x20, fl = 0;
    if ((int32_t)R32(e, cd + 4) > 0) {
        fl = 4;
        W32(e, pl + 4, pf2_r8(e, cd + 4));
    }
    /* (a second open test here cannot fail) */
    W32(e, st + 8, 3);
    pf2_call4(e, I_mciSendCommandA, R32(e, cd), 0x814, 0x100, st);
    uint32_t ntr = R32(e, st + 4);
    if ((int32_t)R32(e, cd + 8) > (int32_t)R32(e, cd + 4) && R32(e, cd + 8) <= ntr) {
        fl |= 8;
        W32(e, pl + 8, pf2_r8(e, cd + 8));
    }
    uint32_t r = pf2_call4(e, I_mciSendCommandA, R32(e, cd), 0x806, fl, pl);
    if (r == 0 && R32(e, cd + 0x10) == 0)
        W32(e, cd + 0x10, pf2_call6(e, I_CreateThread, 0, 0, 0x004b902cu, 0, 0, cd + 0x14));
    return r;
}
