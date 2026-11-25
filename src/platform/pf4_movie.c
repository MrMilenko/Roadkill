/*
 * pf4_movie.c: TM2.EXE 0x004ba15c movie_play (MCI avivideo; docs/PLATFORM.md Part C), ported over
 * Pf2Env (pf2_env.h). Emulator diff: tools/emu/check_pf4_movie.sh.
 *
 * Globals: 0x00c8511c movie device usable (movie_init), 0x00c85110 playing (cleared by the notify
 * handler 0x004ba39c), 0x00c85114 window flag, 0x004e86cc a volume / setting value (x 10 in the
 * command string 0x004e3824).
 */
#include "pf2_env.h"

#define R32 pf2_r32
#define W32 pf2_w32
#define I_wsprintfA 0x00c90658u        /* cdecl */
#define I_mciSendStringA 0x00c90618u
#define I_WaitMessage 0x00c90594u
#define PLAYING 0x00c85110u

static uint32_t mcis(Pf2Env* e, uint32_t cmd, uint32_t cb) { return pf2_call4(e, I_mciSendStringA, cmd, 0, 0, cb); }

/* the setting command: wsprintfA(buf, 0x004e3824, [0x004e86cc] * 10), mciSendStringA(buf) */
static void setting(Pf2Env* e, uint32_t buf) {
    pf2_call1(e, 0x004a6dccu, 0x00bdfcd0u);
    pf2_call3(e, I_wsprintfA, buf, 0x004e3824u, R32(e, 0x004e86ccu) * 10);
    mcis(e, buf, 0);
}

/* 0x004ba15c movie_play(eax file, edx (unused), stack arg, flags; ret 8): needs 0x00c8511c == 1
   (else -1); with the hardware 3D device (0x004958ac) returns 0. flags 1: 0x00496184. flags 2:
   0x0049364c, movie_open 0x004b9de0(file, arg) (failure: 0), the setting command, 0x004a8e6c,
   "play" 0x004e383e with notify to the main window; playing = 1. Then waits (WaitMessage) while
   playing; unless flags 0x10, each message 0x00493748 and 0x00493694(0) (a key) skips: playing = 0,
   result 1. Not skipped: if still playing, the setting command again and 0x004e3874, playing = 0;
   then 0x004e3890. 0x00c85114 == 1: 0x004e377e. Both flags cleared. Returns 1 when skipped, else 0. */
uint32_t pf4_movie_play(Pf2Env* e, uint32_t file, uint32_t edx, uint32_t arg, uint32_t flags) {
    (void)edx;
    uint32_t buf1 = e->scratch, buf2 = e->scratch + 0x100;
    if (R32(e, 0x00c8511cu) != 1) return 0xffffffffu;
    if (pf2_call0(e, 0x004958acu) & 0xff) return 0;
    if (flags & 1) pf2_call0(e, 0x00496184u);
    if (!(flags & 2) || R32(e, 0x00c8511cu) == 0) return 0;
    pf2_call0(e, 0x0049364cu);
    if (pf2_call2(e, 0x004b9de0u, file, arg)) return 0;
    uint32_t hwnd = pf2_call1(e, 0x004a6dccu, 0x00bdfcd0u);
    pf2_call3(e, I_wsprintfA, buf1, 0x004e3824u, R32(e, 0x004e86ccu) * 10);
    mcis(e, buf1, 0);
    pf2_call0(e, 0x004a8e6cu);
    uint32_t r = mcis(e, 0x004e383eu, hwnd);
    W32(e, PLAYING, 1);
    if (r) return 0;
    int esc = (flags & 0x10) == 0;
    pf2_call0(e, 0x0049364cu);
    pf2_call0(e, 0x00493748u);
    uint32_t skipped = 0;
    if (R32(e, PLAYING) == 1) {
        do {
            if (pf2_call0(e, I_WaitMessage) == 1) {
                pf2_call0(e, 0x00493748u);
                if (esc && (pf2_call1(e, 0x00493694u, 0) & 0xff)) {
                    skipped = 1;
                    W32(e, PLAYING, 0);
                }
            }
        } while (R32(e, PLAYING) == 1);
    }
    pf2_call0(e, 0x0049364cu);
    if (!skipped) {
        if (R32(e, PLAYING)) {
            setting(e, buf2);
            mcis(e, 0x004e3874u, 0);
            W32(e, PLAYING, 0);
        }
        mcis(e, 0x004e3890u, 0);
    }
    if (R32(e, 0x00c85114u) == 1) mcis(e, 0x004e377eu, 0);
    W32(e, 0x00c85114u, 0);
    W32(e, PLAYING, 0);
    return skipped;
}
