/*
 * en5_joyobj.h: the joystick object enumeration callback 0x004a8330 of TM2.EXE (fourth wave, en5).
 * Environment: En2Env (en2_env.h). Verified under emulation: tools/emu/check_en5_joyobj.sh.
 */
#ifndef TM2_EN5_JOYOBJ_H
#define TM2_EN5_JOYOBJ_H
#include "en2_env.h"

uint32_t en5_joy_enum_object(En2Env* e, uint32_t ddoi, uint32_t o);   /* 0x004a8330 (stdcall) */

#endif
