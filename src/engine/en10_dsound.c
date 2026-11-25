/*
 * en10_dsound.c: the DirectSound device's destructor and close, TM2.EXE 0x004b93cc / 0x004b9674
 * (see en10_dsound.h). Verified under emulation: tools/emu/check_en10_dsound.sh.
 */
#include <math.h>
#include "en10_dsound.h"
#include "en5_float.h"

static double rd(En2Env* e, uint32_t a) { double d; memcpy(&d, e->mem(e->user, a), 8); return d; }
static uint32_t chop_int(double d)
{
    d = en5_float_round(d);
    if (!(d >= -2147483648.0 && d < 2147483648.0)) return 0x80000000u;
    return (uint32_t)(int32_t)d;
}
static uint32_t vt(En2Env* e, uint32_t o, uint32_t slot) { return en2_r32(e, en2_r32(e, o) + slot); }
static uint32_t com1(En2Env* e, uint32_t o, uint32_t slot) { return en2_call1(e, vt(e, o, slot), o); }
static uint32_t com2(En2Env* e, uint32_t o, uint32_t slot, uint32_t a) { return en2_call2(e, vt(e, o, slot), o, a); }

uint32_t en10_dsound_volume_mb(En2Env* e, int32_t v, uint32_t k_scale, uint32_t k_mul, uint32_t k_min)
{
    if (v == 0) return 0xffffd8f0u;   /* -10000 */
    double d = log((double)v * rd(e, k_scale)) * rd(e, k_mul);
    if (!(d > rd(e, k_min))) d = -10000.0;   /* jbe: not above, or unordered */
    return chop_int(d);
}

/* the primary stopped; rewound when the pointer changed meanwhile; released with the device */
static uint32_t stop_release(En2Env* e, uint32_t dev, int filter)
{
    uint32_t buf = en2_r32(e, dev + 4);
    if (buf == 0) return 0;
    com1(e, buf, 0x48);   /* Stop */
    if (buf != en2_r32(e, dev + 4)) {
        uint32_t r = com2(e, buf, 0x34, 0);   /* SetCurrentPosition(0) */
        if (filter) en2_call2(e, 0x004b94c4u, dev, r);
    }
    if (en2_r32(e, dev) != 0 && en2_r32(e, dev + 4) != 0) com1(e, en2_r32(e, dev + 4), 8);   /* Release */
    en2_w32(e, dev + 4, 0);
    return 1;
}

uint32_t en10_dsound_device_dtor(En2Env* e, uint32_t dev)
{
    if (en2_r32(e, dev + 0x18) == 0) return dev;
    int32_t v = (int32_t)en2_r32(e, dev + 0x10);
    if (en2_r32(e, dev + 4) != 0) {
        uint32_t mb = en10_dsound_volume_mb(e, v, 0x004e3710u, 0x004e3718u, 0x004e3720u);
        en2_call3(e, 0x004b99b4u, dev, en2_r32(e, dev + 4), mb);
    }
    stop_release(e, dev, 1);
    if (en2_r32(e, dev) != 0) com1(e, en2_r32(e, dev), 8);
    en2_w32(e, dev + 0x18, 0);
    en2_w32(e, dev, 0);
    return dev;
}

uint32_t en10_dsound_close(En2Env* e, uint32_t dev)
{
    if (en2_r32(e, dev + 0x18) == 0) return 0;
    int32_t v = (int32_t)en2_r32(e, dev + 0x10);
    if (en2_r32(e, dev + 4) != 0) {
        uint32_t mb = en10_dsound_volume_mb(e, v, 0x004e3728u, 0x004e3730u, 0x004e3738u);
        uint32_t buf = en2_r32(e, dev + 4);
        if (buf != 0) en2_call2(e, 0x004b94c4u, dev, com2(e, buf, 0x3c, mb));   /* SetVolume */
    }
    stop_release(e, dev, 0);
    uint32_t r = 0x80004005u;
    uint32_t ds = en2_r32(e, dev);
    if (ds != 0) r = com1(e, ds, 8);
    en2_w32(e, dev + 0x18, 0);
    en2_w32(e, dev, 0);
    return r;
}
