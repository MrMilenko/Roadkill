/*
 * pf6_cdmon.c: TM2.EXE 0x004b902c cd_monitor_thread (CD audio by MCI; docs/PLATFORM.md Part C),
 * ported over Pf2Env (pf2_env.h). Emulator diff: tools/emu/check_pf6_cdmon.sh.
 *
 * The original never returns (cd_command's stop / pause end it with TerminateThread). PORT SEAM, not
 * in the original: the loop ends when the backend's Sleep returns PF6_STOP (the stand in for
 * TerminateThread; a real Sleep returns nothing the original looks at).
 */
#include "pf2_env.h"

#define R32 pf2_r32
#define PF6_STOP 0x5709570fu
#define CD 0x00c850bcu

/* 0x004b902c cd_monitor_thread(stack param, unused): forever: if the player is open (0x00c850d4),
   MCI_STATUS of the mode (parms +8 = 4); on success: MCI_MODE_STOP (0x20d): the end of track
   callback 0x00c850dc (eax = the mode, edx = 0) or, without one, cd_command(player, 1 play);
   MCI_MODE_PAUSE (0x211): cd_command(player, 2). Then Sleep(3000). */
void pf6_cd_monitor_thread(Pf2Env* e, uint32_t param) {
    (void)param;
    uint32_t p = e->scratch;
    for (;;) {
        if (R32(e, CD + 0x18) != 0) {
            pf2_w32(e, p + 8, 4);
            uint32_t r = pf2_call4(e, 0x00c90614u, R32(e, CD), 0x814, 0x100, p);
            if (r == 0) {
                uint32_t mode = R32(e, p + 4);
                if (mode == 0x20d) {
                    if (R32(e, 0x00c850dcu)) pf2_call2(e, R32(e, 0x00c850dcu), mode, 0);
                    else pf2_call2(e, 0x004b8b40u, CD, 1);
                } else if (mode == 0x211) {
                    pf2_call2(e, 0x004b8b40u, CD, 2);
                }
            }
        }
        if (pf2_call1(e, 0x00c90304u, 3000) == PF6_STOP) return;
    }
}
