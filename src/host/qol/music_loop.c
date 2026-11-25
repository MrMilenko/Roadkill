/*
 * host/qol/music_loop.c: [gameplay] loop_music (host/qol/qol.h, docs/QOL.md). Port code, not
 * from the original game. The CD player (host/cd_audio.c) asks it when a play reaches its end.
 */
#include "host/qol/qol.h"

#include "host/settings/settings.h"

int qol_loop_music(void)
{
    static int v = -1;
    if (v < 0) v = settings_bool("gameplay.loop_music", 0);
    return v;
}
