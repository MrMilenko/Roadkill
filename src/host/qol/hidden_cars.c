/*
 * host/qol/hidden_cars.c: [gameplay] unlock_hidden_cars and play_dark_tooth (host/qol/qol.h,
 * docs/QOL.md). Port code, not from the original game. The runtime's car hooks
 * (src/rt/qol_cars.c) ask them.
 */
#include "host/qol/qol.h"

#include "host/settings/settings.h"

int qol_unlock_hidden_cars(void)
{
    static int v = -1;
    if (v < 0) v = settings_bool("gameplay.unlock_hidden_cars", 0);
    return v;
}

int qol_play_dark_tooth(void)
{
    static int v = -1;
    if (v < 0) v = settings_bool("gameplay.play_dark_tooth", 0);
    return v;
}
