/*
 * en5_dinput.c: 0x0049e110: the DirectInput keyboard. Input mode 0x004e8434 = 1, cooperative
 * flags 0x004e8438 = 6 (DISCL_NONEXCLUSIVE | DISCL_FOREGROUND), buffer entries 0x004e843c = 10,
 * acquired flags 0x00a5d5f0 / 0x00a5d5f4 = 0. DirectInputCreateA(hinst 0x004a8f40(), 0x500,
 * &0x00a5d374, 0); CreateDevice(+0x0c; GUID_SysKeyboard copied from 0x004e53d4, &0x00a5d378, 0);
 * SetDataFormat(+0x2c; c_dfDIKeyboard 0x004d10a0); SetProperty(+0x18; DIPROP_BUFFERSIZE, a
 * DIPROPDWORD {0x14, 0x10, 0, 0, 10}); SetCooperativeLevel(+0x34; hwnd 0x004a8f34(), 6);
 * GetCapabilities(+0x0c; DIDEVCAPS 0x00a5d5c0 cleared, dwSize 0x2c); Acquire(+0x1c). A failure
 * calls 0x004937b8, releases what was made (device, then DirectInput; both pointers cleared) and
 * returns 0 (a failed create releases nothing). Success: both flags 1, returns 1.
 */
#include "en5_dinput.h"

#define EN5_DirectInputCreateA 0x00c90810u
#define DI  0x00a5d374u
#define DEV 0x00a5d378u

static uint32_t com(En2Env* e, uint32_t obj, uint32_t off, const uint32_t* rest, int n)
{
    uint32_t v[4] = {obj, 0, 0, 0};
    for (int k = 0; k < n; k++) v[1 + k] = rest[k];
    return e->call(e->user, en2_r32(e, en2_r32(e, obj) + off), v, 1 + n);
}

static uint32_t fail(En2Env* e, int dev)
{
    en2_call0(e, 0x004937b8u);
    if (dev) com(e, en2_r32(e, DEV), 8, 0, 0);
    com(e, en2_r32(e, DI), 8, 0, 0);
    if (dev) en2_w32(e, DEV, 0);
    en2_w32(e, DI, 0);
    return 0;
}

uint32_t en5_dinput_keyboard_init(En2Env* e)
{
    const uint32_t P = e->scratch, G = e->scratch + 0x14;
    for (uint32_t k = 0; k < 16; k += 4) en2_w32(e, G + k, en2_r32(e, 0x004e53d4u + k));
    uint32_t hinst = en2_call0(e, 0x004a8f40u);
    uint32_t hwnd = en2_call0(e, 0x004a8f34u);
    en2_w32(e, 0x00a5d5f0u, 0);
    en2_w32(e, 0x00a5d5f4u, 0);
    en2_w32(e, 0x004e8434u, 1);
    en2_w32(e, 0x004e8438u, 6);
    en2_w32(e, 0x004e843cu, 10);
    if (en2_call4(e, EN5_DirectInputCreateA, hinst, 0x500, DI, 0) != 0) {
        en2_call0(e, 0x004937b8u);
        return 0;
    }
    uint32_t a[3] = {G, DEV, 0};
    if (com(e, en2_r32(e, DI), 0x0c, a, 3) != 0) return fail(e, 0);
    uint32_t f[1] = {0x004d10a0u};
    if (com(e, en2_r32(e, DEV), 0x2c, f, 1) != 0) return fail(e, 1);
    en2_w32(e, P + 8, 0);
    en2_w32(e, P + 0x0c, 0);
    en2_w32(e, P, 0x14);
    en2_w32(e, P + 0x10, en2_r32(e, 0x004e843cu));
    en2_w32(e, P + 4, 0x10);
    uint32_t sp[2] = {1, P};
    if (com(e, en2_r32(e, DEV), 0x18, sp, 2) != 0) return fail(e, 1);
    uint32_t cl[2] = {hwnd, en2_r32(e, 0x004e8438u)};
    if (com(e, en2_r32(e, DEV), 0x34, cl, 2) != 0) return fail(e, 1);
    en2_call3(e, 0x004bf830u, 0x00a5d5c0u, 0, 0x2c);
    en2_w32(e, 0x00a5d5c0u, 0x2c);
    uint32_t cp[1] = {0x00a5d5c0u};
    if (com(e, en2_r32(e, DEV), 0x0c, cp, 1) != 0) return fail(e, 1);
    if (com(e, en2_r32(e, DEV), 0x1c, 0, 0) != 0) return fail(e, 1);
    en2_w32(e, 0x00a5d5f4u, 1);
    en2_w32(e, 0x00a5d5f0u, 1);
    return 1;
}
