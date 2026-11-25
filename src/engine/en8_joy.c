/*
 * en8_joy.c: the joystick state snapshot, TM2.EXE 0x004a28d0 (see en8_joy.h).
 * Verified under emulation: tools/emu/check_en8_joy.sh.
 */
#include "en8_joy.h"

#define JOY 0x00a82560u
#define OUT 0x00a824fdu
#define HOLD 0x00a8255cu
#define I_timeGetTime 0x00c90640u

static void button(En2Env* e, uint32_t k) { en2_w8(e, OUT + k, (uint8_t)en2_call2(e, 0x004a8078u, JOY, k)); }

void en8_joy_snapshot(En2Env* e)
{
    if ((en2_call1(e, 0x004932a0u, 4) & 0xff) != 1) return;
    if (en2_call0(e, 0x0049e108u) != 0) en2_w32(e, HOLD, 0);
    if (en2_r32(e, HOLD) != 0) {
        uint32_t t = en2_call0(e, I_timeGetTime);
        en2_w32(e, 0x00a82554u, t);
        int32_t d = (int32_t)(t - en2_r32(e, 0x00a82550u));
        en2_w32(e, 0x00a82558u, (uint32_t)d);
        if (d > (int32_t)en2_r32(e, 0x004e84a8u)) en2_w32(e, HOLD, 0);
        return;
    }
    en2_call1(e, 0x004a8018u, JOY);                        /* poll */
    int32_t n = (int32_t)en2_call1(e, 0x004a8240u, JOY);   /* buttons */
    en2_call2(e, 0x004a822cu, JOY, 0);
    uint32_t k;
    for (k = 0; k < 4; k++) button(e, k);
    if (n > 4) for (k = 4; k < 10; k++) button(e, k);
    if (n > 10) for (k = 10; k < 18; k++) button(e, k);
    if (n > 18) for (k = 18; k < 32; k++) button(e, k);
    static const uint32_t axis[12] = {0x004a809cu, 0x004a80b0u, 0x004a80ccu, 0x004a80dcu, 0x004a80f8u, 0x004a8108u,
                                      0x004a8124u, 0x004a8138u, 0x004a8158u, 0x004a816cu, 0x004a818cu, 0x004a81a0u};
    for (k = 0; k < 4; k++) en2_w8(e, OUT + 0x20 + k, (uint8_t)en2_call1(e, axis[k], JOY));
    for (uint32_t a = 2; a < 6; a++) {
        if (en2_r32(e, 0x00a82540u + 4 * (a - 2)) == 0) continue;
        en2_w8(e, OUT + 0x20 + 2 * a, (uint8_t)en2_call1(e, axis[2 * a], JOY));
        en2_w8(e, OUT + 0x21 + 2 * a, (uint8_t)en2_call1(e, axis[2 * a + 1], JOY));
    }
    if (en2_r32(e, 0x00a8253cu) != 0) {
        static const uint32_t pov[5] = {0, 0x4650, 0x6978, 0x2328, 0xffff};
        for (k = 0; k < 5; k++) en2_w8(e, OUT + 0x2c + k, (uint8_t)en2_call3(e, 0x004a81b8u, JOY, 0, pov[k]));
    }
    if (en2_call0(e, 0x0049e108u) == 0) {
        en2_w32(e, HOLD, 1);
        en2_w32(e, 0x00a82550u, en2_call0(e, I_timeGetTime));
    }
}
