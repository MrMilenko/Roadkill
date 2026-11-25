/*
 * tm2.c: the game on the ported code. Loads the player's TM2.EXE into the runtime's address
 * space (src/rt/rt.h), binds the ported functions and the host services, and calls WinMain
 * 0x004a8b04 as the Watcom start up does.
 *
 *   tm2 [path to TM2.EXE]     (default disc/files/TM2.EXE)
 *   TM2_TRACE=1 prints every call.
 */
#include "rt/rt.h"

#include <stdio.h>
#include <stdlib.h>

/* The Watcom start up runs the init routines of the XI table before WinMain: 6 byte records
 * {type, priority, routine}, from 0x004ea5fc until the first record that does not point into the
 * code, lowest priority first (stable). The game's own routines (static constructors of its
 * global objects: the window object's procedure pointer among them) are ported; the runtime's
 * (0x004c0000 and up: heap, file table, exception chain) are host services, called when bound. */
static void run_init_routines(void)
{
    uint32_t va = 0x004ea5fcu, recs[64];
    int n = 0;
    while (n < 64) {
        uint32_t fn = rt_r32(va + 2);
        if (fn < 0x00410000u || fn >= 0x004e0000u) break;
        recs[n++] = va;
        va += 6;
    }
    for (int pri = 0; pri < 256; pri++)
        for (int i = 0; i < n; i++) {
            if (rt_r8(recs[i] + 1) != pri) continue;
            uint32_t fn = rt_r32(recs[i] + 2);
            if (fn >= 0x004c0000u && !rt_name(fn)) {
                if (getenv("TM2_TRACE")) fprintf(stderr, "tm2: init routine 0x%08x (runtime, host side)\n", fn);
                continue;
            }
            uint32_t none[1] = {0};
            rt_call(fn, none, 0);
        }
}

int main(int argc, char** argv)
{
    const char* exe = argc > 1 ? argv[1] : "disc/files/TM2.EXE";
    if (rt_init(exe) != 0) { fprintf(stderr, "tm2: cannot load %s\n", exe); return 1; }
    rt_bind_all();
    run_init_routines();
    /* the Watcom start up calls WinMain(instance, 0, command line, SW_SHOWNORMAL); the instance
     * is the image base, the command line an empty string in the frame stack */
    uint32_t cmd = rt_frame_push(16);
    uint32_t args[4] = {0x00400000u, 0, cmd, 1};
    uint32_t r = rt_call(0x004a8b04u, args, 4);
    rt_shutdown();
    return (int)r;
}
