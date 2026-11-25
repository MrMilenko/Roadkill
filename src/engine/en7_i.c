/*
 * en7_i.c: see en7_i.h.
 */
#include "en7_i.h"

/* 0x004a7dbc: adds the device dev (DIDEVICEINSTANCE ddi) as joystick n = +0x50c9c (at most 16, else
 * E_OUTOFMEMORY): QueryInterface (method 0) for IDirectInputDevice2 (IID 0x004e52e4) into +0x2dc +
 * 4 n (its HRESULT returned on failure); the record r = o + 0x31c + 0x5098 n cleared (memset) with
 * the instance and product names (ddi +0x28, +0x12c) at r and r + 0x104; GetCapabilities (+0x0c;
 * DIDEVCAPS dwSize 0x2c) -> o + 0x5098 n + 0x524..0x534 (flags, type, axes, buttons, POVs), flag
 * bit 0 -> +0x53ac = 1, device subtype 4 (byte 1 of the type) -> +0x53b0 = 1; EnumObjects (+0x10;
 * callback 0x004a8330, o, DIDFT_ALL); then the count goes up. Failures of the last two call
 * 0x004937b8 and return the HRESULT; success 0. */
uint32_t en7_joy_device_add(En2Env* e, uint32_t o, uint32_t dev, uint32_t ddi)
{
    if ((int32_t)en2_r32(e, o + 0x50c9c) >= 0x10) return 0x8007000eu;
    uint32_t r = en2_call3(e, en2_r32(e, en2_r32(e, dev)), dev, 0x004e52e4u, o + 0x2dc + 4 * en2_r32(e, o + 0x50c9c));
    if (r != 0) return r;
    const uint32_t L = e->scratch;
    en2_call3(e, 0x004bf830u, o + 0x31c + en2_r32(e, o + 0x50c9c) * 0x5098u, 0, 0x5098);
    en2_call2(e, 0x00c9031cu, o + 0x31c + en2_r32(e, o + 0x50c9c) * 0x5098u, ddi + 0x28);
    en2_call2(e, 0x00c9031cu, o + 0x31c + en2_r32(e, o + 0x50c9c) * 0x5098u + 0x104, ddi + 0x12c);
    en2_call3(e, 0x004bf830u, L, 0, 0x2c);
    en2_w32(e, L, 0x2c);
    uint32_t d2 = en2_r32(e, o + 0x2dc + 4 * en2_r32(e, o + 0x50c9c));
    r = en2_call2(e, en2_r32(e, en2_r32(e, d2) + 0x0c), d2, L);
    if (r != 0) {
        en2_call0(e, 0x004937b8u);
        return r;
    }
    for (uint32_t k = 0; k < 5; k++)
        en2_w32(e, o + en2_r32(e, o + 0x50c9c) * 0x5098u + 0x524 + 4 * k, en2_r32(e, L + 4 + 4 * k));
    if (en2_r8(e, L + 4) & 1) en2_w32(e, o + en2_r32(e, o + 0x50c9c) * 0x5098u + 0x53ac, 1);
    uint32_t rec = o + en2_r32(e, o + 0x50c9c) * 0x5098u;
    if (((en2_r16(e, rec + 0x528) >> 8) & 0xff) == 4) en2_w32(e, rec + 0x53b0, 1);
    d2 = en2_r32(e, o + 0x2dc + 4 * en2_r32(e, o + 0x50c9c));
    uint32_t v[4] = {d2, 0x004a8330u, o, 0};
    r = e->call(e->user, en2_r32(e, en2_r32(e, d2) + 0x10), v, 4);
    if (r != 0) {
        en2_call0(e, 0x004937b8u);
        return r;
    }
    en2_w32(e, o + 0x50c9c, en2_r32(e, o + 0x50c9c) + 1);
    return 0;
}
