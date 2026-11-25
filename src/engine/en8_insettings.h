/*
 * en8_insettings.h: the input settings loaded from the registry, TM2.EXE 0x004a97f4, in the En2Env
 * of en2_env.h (the registry key object, the device record and the read sizes live at
 * En2Env.scratch at the original's frame offsets: +0 device record (0x194 bytes), +0x194 value,
 * +0x198 key, +0x19c size).
 * Verified against the original under emulation (tools/emu/check_en8_insettings.sh).
 */
#ifndef TM2_EN8_INSETTINGS_H
#define TM2_EN8_INSETTINGS_H

#include <stdint.h>
#include "en2_env.h"

/* 0x004a97f4: HKEY_LOCAL_MACHINE key (reg_open's error returned as is); the device flags
 * (0x004e84e0) and the three devices' on / off (names 0x004e84e8 / 0x004e84f0 / 0x004e84f8, used
 * where the device flag functions 0x00493380 / 0x00493398 / 0x0049338c allow it); the joystick's
 * saved device record (0x004e8500) against the current one (0x004c5d9c: GUID words, name, three
 * fields): a different or missing joystick turns device 4 off; the flags rebuilt from the device
 * types; the three key tables (0x004e8508 / 0x004e8510 / 0x004e8518) read whole into their edit
 * copies, a short or failed read restores the tables (0x004a2018 / 0x004a27d4 / 0x004a2290) and
 * returns -1; success sets 0x00bdfb30..33 and returns 1. Any failed value read returns -1. */
uint32_t en8_input_settings_load(En2Env* e);

#endif
