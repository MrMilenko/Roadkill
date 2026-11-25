/*
 * host/qol/controller.c: [input] controller, the controller working without a visit to the
 * game's input screen (host/qol/qol.h, docs/QOL.md). Port code, not from the original game.
 * The registry host (host/win32_reg.c) asks it when the game reads InputJoystickStatus.
 */
#include "host/qol/qol.h"

#include "host/input_joy.h"
#include "host/settings/settings.h"

int qol_controller_default(void)
{
    static int v = -1;
    if (v < 0) v = settings_bool("input.controller", 1) && joy_count() > 0;
    return v;
}
