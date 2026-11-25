/*
 * host/qol/crash_fixes.c: [gameplay] fix_crashes (host/qol/qol.h, docs/QOL.md). Port code, not
 * from the original game. The guards themselves are small marked branches in the ports that
 * have the bugs (engine en9_texload.c, en_clut.c).
 */
#include "host/qol/qol.h"

#include <stdio.h>
#include <string.h>

#include "host/settings/settings.h"

int qol_fix_crashes(void)
{
    static int v = -1;
    if (v < 0) v = settings_bool("gameplay.fix_crashes", 0);
    return v;
}

void qol_fixed(const char* id)
{
    static const char* seen[32];
    static int nseen;
    for (int i = 0; i < nseen; i++) if (!strcmp(seen[i], id)) return;
    if (nseen < 32) seen[nseen++] = id;
    fprintf(stderr, "roadkill: fix_crashes: %s\n", id);
}
