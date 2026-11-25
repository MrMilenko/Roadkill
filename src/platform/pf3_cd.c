/*
 * pf3_cd.c: TM2.EXE 0x004b8b40 cd_command (CD audio by MCI; docs/PLATFORM.md Part C), ported over
 * Pf2Env (pf2_env.h). Emulator diff: tools/emu/check_pf3_cd.sh.
 *
 * cd_command(eax player, dl command 0..8). Player 0x00c850bc: +0x00 MCI device id, +0x04 from track,
 * +0x08 to track, +0x10 monitor thread handle, +0x14 thread id, +0x18 open, +0x20 end of track
 * callback. The MCI parameter blocks live on the stack; only the fields named below are set (the
 * rest is stack garbage the driver ignores for those flags). Returns the MCI error (0 = ok).
 */
#include "pf2_env.h"

#define R32 pf2_r32
#define W32 pf2_w32

#define I_mciSendCommandA 0x00c90614u
#define I_mciGetErrorStringA 0x00c90610u
#define I_CreateThread 0x00c902b4u
#define I_TerminateThread 0x00c90308u
#define F_CD_DRIVE 0x00499958u    /* the CD drive letter setting */
#define F_CD_OPEN 0x004b858cu     /* cd_open(eax player, edx drive) */
#define F_CD_PLAY 0x004b86dcu     /* cd_play(eax player) */
#define F_ERROR_BOX 0x004bc660u
#define MCI_MODE_STOP 0x20du
#define MCI_MODE_PAUSE 0x211u

static uint32_t mci(Pf2Env* e, uint32_t cd, uint32_t msg, uint32_t flags, uint32_t parms) {
    return pf2_call4(e, I_mciSendCommandA, R32(e, cd), msg, flags, parms);
}

/* MCI_STATUS of MCI_STATUS_MODE (parms +8 = 4): the mode on success */
static uint32_t status_mode(Pf2Env* e, uint32_t cd, uint32_t parms, uint32_t* mode) {
    W32(e, parms + 8, 4);
    uint32_t r = mci(e, cd, 0x814, 0x100, parms);
    if (r == 0) *mode = R32(e, parms + 4);
    return r;
}

static void kill_monitor(Pf2Env* e, uint32_t cd) {
    uint32_t t = R32(e, cd + 0x10);
    if (t) {
        pf2_call2(e, I_TerminateThread, t, 0);
        W32(e, cd + 0x10, 0);
    }
}

/* esi_in: command numbers above 8 take esi as the caller left it as the error (ORIGINAL BUG:
   it then goes through the error string / box path), passed here.
   ORIGINAL BUG, NOT reproduced: on error 0x123 (MCIERR_DEVICE_NOT_READY) the error box 0x004bc660(0)
   answering 4 (retry) sets a flag the original never clears, so it repeats the command forever
   (even after a success). The port repeats the command while the box answers retry. That path is
   not compared (the original does not return). */
uint32_t pf3_cd_command(Pf2Env* e, uint32_t cd, uint32_t cmd, uint32_t esi_in) {
    uint32_t p = e->scratch, err = esi_in;
    int retry = 0;
    do {
        uint32_t c = cmd & 0xff, mode = 0;
        if (c > 8) goto check;
        switch (c) {
        case 0:   /* open */
            err = pf2_call2(e, F_CD_OPEN, cd, pf2_call0(e, F_CD_DRIVE));
            break;
        case 1:   /* play from the set tracks */
            err = pf2_call1(e, F_CD_PLAY, cd);
            break;
        case 2:   /* resume: MCI_PLAY (to = (byte) +8 with MCI_TO when to > from); start the monitor */
            if (!R32(e, cd + 0x18)) { err = 0; break; }
            {
                uint32_t fl = 0;
                if ((int32_t)R32(e, cd + 8) > (int32_t)R32(e, cd + 4)) {
                    W32(e, p + 8, pf2_r8(e, cd + 8));
                    fl |= 8;
                }
                err = mci(e, cd, 0x806, fl, p);
                if (err == 0 && R32(e, cd + 0x10) == 0)
                    W32(e, cd + 0x10, pf2_call6(e, I_CreateThread, 0, 0, 0x004b902cu, 0, 0, cd + 0x14));
            }
            break;
        case 3:   /* pause: stop the monitor; MCI_PAUSE unless the mode is already "stopped" */
            if (!R32(e, cd + 0x18)) { err = 0; break; }
            kill_monitor(e, cd);
            /* the original tests +0x18 again here (always set): its else path is unreachable */
            err = status_mode(e, cd, p, &mode);
            if (err == 0 && mode != MCI_MODE_STOP) err = mci(e, cd, 0x809, 0, p + 0x10);
            break;
        case 4:   /* stop */
            if (!R32(e, cd + 0x18)) { err = 0; break; }
            kill_monitor(e, cd);
            err = mci(e, cd, 0x808, 0, p);
            break;
        case 5:   /* seek to (byte) +4 */
            if (!R32(e, cd + 0x18)) { err = 0; break; }
            W32(e, p + 4, pf2_r8(e, cd + 4));
            err = mci(e, cd, 0x807, 8, p);
            break;
        case 6:   /* door open */
            if (!R32(e, cd + 0x18)) { err = 0; break; }
            err = mci(e, cd, 0x80d, 0x100, p);
            break;
        case 7:   /* door closed */
            if (!R32(e, cd + 0x18)) { err = 0; break; }
            err = mci(e, cd, 0x80d, 0x200, p);
            break;
        case 8:   /* poll: a stopped track -> the end callback or play again; paused -> resume */
            if (!R32(e, cd + 0x18)) { err = 0; break; }
            {
                uint32_t r = status_mode(e, cd, p, &mode);
                if (r == 0) {
                    if (mode == MCI_MODE_STOP) {
                        uint32_t cb = R32(e, cd + 0x20);
                        if (cb) pf2_call2(e, cb, 0, mode);
                        else pf3_cd_command(e, cd, 1, cd);
                    } else if (mode == MCI_MODE_PAUSE) {
                        pf3_cd_command(e, cd, 2, cd);
                    }
                }
                err = r;
            }
            break;
        }
    check:
        if (err) {
            pf2_call3(e, I_mciGetErrorStringA, err, p + 0x40, 0x80);
            if (err == 0x123 && pf2_call1(e, F_ERROR_BOX, 0) == 4) retry = 1;
        }
    } while (retry && (retry = 0, 1));
    return err;
}
