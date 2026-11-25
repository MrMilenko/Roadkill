/*
 * en5_joyobj.c: 0x004a8330, DIEnumDeviceObjectsCallback (stdcall: DIDEVICEOBJECTINSTANCE ddoi,
 * the joystick manager o). The current device n = o +0x50c9c, its record r = o + 0x5098 n, its
 * IDirectInputDevice o +0x2dc + 4 n. The object is stored in the record's table (entry r + 400 i,
 * i = r +0x53a8, then counted): +0x588 dwSize, +0x58c guidType, +0x59c dwOfs, +0x5a0 dwType,
 * +0x5a4 dwFlags, +0x5a8 tszName (strcpy), +0x6ac / +0x6b0 dwFFMaxForce / dwFFForceResolution,
 * +0x6b4..+0x6ba the four collection / usage words. Then by guidType (the DirectInput GUIDs in
 * the EXE at 0x004e5314 + 0x10 k): X, Y set r +0x538 / +0x53c and get the range (-1, 1) when r
 * +0x53b0 == 1 else (-127, 127); Z, Rx, Ry set +0x540 / +0x544 / +0x548 and get (-127, 127); Rz the
 * same, its flag +0x54c only when dwOfs == 0x14. Each axis: SetProperty(+0x18; DIPROP_RANGE,
 * DIPROPRANGE {0x18, 0x10, DIJOFS of the axis (0, 4, 8, 0x0c, 0x10, 0x14), DIPH_BYOFFSET, min,
 * max}) and SetProperty(DIPROP_DEADZONE, DIPROPDWORD {0x14, 0x10, ofs, DIPH_BYOFFSET, 2000}).
 * Button: byte r +0x568 + instance (dwType >> 8, 16 bits) = 1; POV: dword r +0x558 + 4 instance
 * = 1; slider, key, unknown and others: nothing. Returns 1 (DIENUM_CONTINUE).
 */
#include "en5_joyobj.h"

static int guid_is(En2Env* e, uint32_t a, uint32_t g)
{
    for (uint32_t k = 0; k < 16; k++)
        if (en2_r8(e, a + k) != en2_r8(e, g + k)) return 0;
    return 1;
}

static void axis(En2Env* e, uint32_t dev, uint32_t ofs, int32_t lo, int32_t hi)
{
    const uint32_t R = e->scratch, D = e->scratch + 0x20;
    uint32_t r[6] = {0x18, 0x10, ofs, 1, (uint32_t)lo, (uint32_t)hi};
    for (int k = 0; k < 6; k++) en2_w32(e, R + 4u * (uint32_t)k, r[k]);
    en2_call3(e, en2_r32(e, en2_r32(e, dev) + 0x18), dev, 4, R);
    uint32_t d[5] = {0x14, 0x10, ofs, 1, 2000};
    for (int k = 0; k < 5; k++) en2_w32(e, D + 4u * (uint32_t)k, d[k]);
    en2_call3(e, en2_r32(e, en2_r32(e, dev) + 0x18), dev, 5, D);
}

uint32_t en5_joy_enum_object(En2Env* e, uint32_t dd, uint32_t o)
{
    uint32_t n = en2_r32(e, o + 0x50c9c);
    uint32_t r = o + n * 0x5098u;
    uint32_t dev = en2_r32(e, o + n * 4 + 0x2dc);
    int32_t hi = en2_r32(e, r + 0x53b0) == 1 ? 1 : 0x7f, lo = -hi;
    uint32_t ent = r + 400u * en2_r32(e, r + 0x53a8);
    en2_w32(e, ent + 0x588, en2_r32(e, dd));
    for (uint32_t k = 0; k < 16; k += 4) en2_w32(e, ent + 0x58c + k, en2_r32(e, dd + 4 + k));
    en2_w32(e, ent + 0x59c, en2_r32(e, dd + 0x14));
    en2_w32(e, ent + 0x5a0, en2_r32(e, dd + 0x18));
    en2_w32(e, ent + 0x5a4, en2_r32(e, dd + 0x1c));
    uint32_t s = dd + 0x20, d = ent + 0x5a8;
    uint8_t c;
    do { c = en2_r8(e, s++); en2_w8(e, d++, c); } while (c != 0);
    en2_w32(e, ent + 0x6ac, en2_r32(e, dd + 0x124));
    en2_w32(e, ent + 0x6b0, en2_r32(e, dd + 0x128));
    for (uint32_t k = 0; k < 8; k += 2) en2_w16(e, ent + 0x6b4 + k, en2_r16(e, dd + 0x12c + k));
    en2_w32(e, r + 0x53a8, en2_r32(e, r + 0x53a8) + 1);
    const uint32_t g = dd + 4;
    if (guid_is(e, g, 0x004e5314u)) { en2_w32(e, r + 0x538, 1); axis(e, dev, 0, lo, hi); }
    else if (guid_is(e, g, 0x004e5324u)) { en2_w32(e, r + 0x53c, 1); axis(e, dev, 4, lo, hi); }
    else if (guid_is(e, g, 0x004e5334u)) { en2_w32(e, r + 0x540, 1); axis(e, dev, 8, -0x7f, 0x7f); }
    else if (guid_is(e, g, 0x004e5344u)) { en2_w32(e, r + 0x544, 1); axis(e, dev, 0x0c, -0x7f, 0x7f); }
    else if (guid_is(e, g, 0x004e5354u)) { en2_w32(e, r + 0x548, 1); axis(e, dev, 0x10, -0x7f, 0x7f); }
    else if (guid_is(e, g, 0x004e5364u)) {
        if (en2_r32(e, dd + 0x14) == 0x14) en2_w32(e, r + 0x54c, 1);
        axis(e, dev, 0x14, -0x7f, 0x7f);
    } else if (guid_is(e, g, 0x004e5374u)) {
        /* slider: nothing */
    } else if (guid_is(e, g, 0x004e5384u)) {
        en2_w8(e, r + 0x568 + ((en2_r32(e, dd + 0x18) >> 8) & 0xffff), 1);
    } else if (guid_is(e, g, 0x004e5394u)) {
        /* key: nothing */
    } else if (guid_is(e, g, 0x004e53a4u)) {
        en2_w32(e, r + 0x558 + 4 * ((en2_r32(e, dd + 0x18) >> 8) & 0xffff), 1);
    }
    return 1;
}
