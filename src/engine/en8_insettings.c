/*
 * en8_insettings.c: the input settings loaded from the registry, TM2.EXE 0x004a97f4
 * (see en8_insettings.h). Verified under emulation: tools/emu/check_en8_insettings.sh.
 */
#include "en8_insettings.h"

#define R_SET_PATH 0x004ac4b0u
#define R_OPEN     0x004ac528u
#define R_INT      0x004ac5bcu
#define R_FLUSH    0x004ac590u
#define R_DATA     0x004ac5e4u
#define IN_FLAGS   0x0049334cu
#define IN_SET     0x004932c4u
#define IN_TYPE    0x004932a0u
#define I_lstrcmpA 0x00c90318u

enum { S_REC = 0, S_VAL = 0x194, S_KEY = 0x198, S_SIZE = 0x19c };

static uint32_t fail(En2Env* e, uint32_t F)
{
    en2_call1(e, R_FLUSH, F + S_KEY);
    return 0xffffffffu;
}
static int read_int(En2Env* e, uint32_t F, uint32_t name_ptr)
{
    return en2_call3(e, R_INT, F + S_KEY, en2_r32(e, name_ptr), F + S_VAL) != 0;
}
/* a device's on / off when its flag allows, else only its type asked */
static void device(En2Env* e, uint32_t F, uint32_t flag_fn, uint32_t dev)
{
    if (en2_call0(e, flag_fn) & 0xff)
        en2_call2(e, IN_SET, dev, en2_r32(e, F + S_VAL) == 1 ? 1 : 0);
    else
        en2_call1(e, IN_TYPE, dev);
}
/* a key table: info (size into S_VAL, returns the table), read whole */
static int table(En2Env* e, uint32_t F, uint32_t info_fn, uint32_t name_ptr)
{
    uint32_t t = en2_call1(e, info_fn, F + S_VAL);
    en2_w32(e, F + S_SIZE, en2_r32(e, F + S_VAL));
    return en2_call4(e, R_DATA, F + S_KEY, en2_r32(e, name_ptr), t, F + S_SIZE) == 0 &&
           en2_r32(e, F + S_SIZE) == en2_r32(e, F + S_VAL);
}

uint32_t en8_input_settings_load(En2Env* e)
{
    const uint32_t F = e->scratch;
    int changed = 0;
    en2_call2(e, R_SET_PATH, F + S_KEY, 0);
    uint32_t r = en2_call3(e, R_OPEN, F + S_KEY, 0x80000002u /* HKEY_LOCAL_MACHINE */, 0);
    if (r != 0) {
        en2_call1(e, R_FLUSH, F + S_KEY);
        return r;
    }
    if (read_int(e, F, 0x004e84e0u)) return fail(e, F);
    en2_call1(e, IN_FLAGS, en2_r32(e, F + S_VAL));
    if (read_int(e, F, 0x004e84e8u)) return fail(e, F);
    if (en2_call0(e, 0x00493380u) & 0xff) en2_call2(e, IN_SET, 1, 1);
    if (read_int(e, F, 0x004e84f0u)) return fail(e, F);
    device(e, F, 0x00493398u, 4);
    if (read_int(e, F, 0x004e84f8u)) return fail(e, F);
    device(e, F, 0x0049338cu, 2);
    en2_w32(e, F + S_SIZE, 0x194);
    if (en2_call4(e, R_DATA, F + S_KEY, en2_r32(e, 0x004e8500u), F + S_REC, F + S_SIZE) != 0) return fail(e, F);
    if (en2_call0(e, 0x00493398u) & 0xff) {   /* the saved joystick still the one attached? */
        uint32_t p = en2_call0(e, 0x004c5d9cu);
        if (p == 0) {
            changed = 1;
            en2_call2(e, IN_SET, 4, 0);
        } else if (en2_r16(e, F) != en2_r16(e, p) || en2_r16(e, F + 2) != en2_r16(e, p + 2) ||
                   en2_call2(e, I_lstrcmpA, F + 4, p + 4) != 0 ||
                   en2_r32(e, F + 0x3c) != en2_r32(e, p + 0x3c) || en2_r32(e, F + 0x60) != en2_r32(e, p + 0x60) ||
                   en2_r32(e, F + 0x68) != en2_r32(e, p + 0x68)) {
            if ((en2_call1(e, IN_TYPE, 4) & 0xff) == 1) en2_call2(e, IN_SET, 4, 0);
            changed = 1;
        }
    }
    uint32_t flags = 1;
    if ((en2_call1(e, IN_TYPE, 2) & 0xff) == 1) flags = 3;
    if ((en2_call1(e, IN_TYPE, 4) & 0xff) == 1) flags |= 4;
    en2_call1(e, IN_FLAGS, flags);
    if (!table(e, F, 0x004a217cu, 0x004e8508u)) {
        en2_call0(e, 0x004a2018u);
        en2_call0(e, 0x004a27d4u);
        en2_call0(e, 0x004a2290u);
        return fail(e, F);
    }
    en2_w8(e, 0x00bdfb31u, 1);
    if (!table(e, F, 0x004a2d9cu, 0x004e8510u)) {
        en2_call0(e, 0x004a27d4u);
        en2_call0(e, 0x004a2290u);
        return fail(e, F);
    }
    en2_w8(e, 0x00bdfb33u, 1);
    if (!table(e, F, 0x004a25ecu, 0x004e8518u)) {
        en2_call0(e, 0x004a2290u);
        return fail(e, F);
    }
    en2_w8(e, 0x00bdfb32u, 1);
    if (changed) en2_call0(e, 0x004a27d4u);
    en2_w8(e, 0x00bdfb30u, 1);
    en2_call1(e, R_FLUSH, F + S_KEY);
    return 1;
}
