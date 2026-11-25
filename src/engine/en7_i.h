/*
 * en7_i.h: the joystick device add (DIEnumDevices helper) of TM2.EXE (fourth wave, en7 batch i).
 * Environment: En2Env (en2_env.h). Verified under emulation: tools/emu/check_en7_i.sh.
 */
#ifndef TM2_EN7_I_H
#define TM2_EN7_I_H
#include "en2_env.h"

uint32_t en7_joy_device_add(En2Env* e, uint32_t o, uint32_t dev, uint32_t ddi);   /* 0x004a7dbc */

#endif
