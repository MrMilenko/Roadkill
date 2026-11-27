/*
 * host/qol/hidden_arenas.c: [gameplay] hidden_arenas (host/qol/qol.h, docs/QOL.md). Port code,
 * not from the original game. The runtime's arena hooks (src/rt/qol_arenas.c) ask it.
 */
#include "host/qol/qol.h"

#include "host/settings/settings.h"

int qol_hidden_arenas(void)
{
    static int v = -1;
    if (v < 0) v = settings_bool("gameplay.hidden_arenas", 0);
    return v;
}
