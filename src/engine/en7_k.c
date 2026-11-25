/*
 * en7_k.c: see en7_k.h.
 */
#include "en7_k.h"

static void light(En2Env* e, uint32_t a, uint32_t k, uint32_t s0, uint32_t s1, uint32_t s2)
{
    static const uint32_t col[3] = {0x00a04180u, 0x00a0418cu, 0x00a04198u};
    static const uint32_t dir[3] = {0x00a04178u, 0x00a04184u, 0x00a04190u};
    uint32_t v[7] = {a, k, en2_r32(e, col[k]), en2_r32(e, dir[k]), s0, s1, s2};
    e->call(e->user, 0x00495050u, v, 7);
}

static void sx3(En2Env* e, uint32_t dst)
{
    for (uint32_t k = 0; k < 3; k++)
        en2_w32(e, dst + 4 * k, (uint32_t)(int32_t)(int16_t)en2_r16(e, 0x00a0416cu + 4 * k));
}

/* 0x00495328 (subsys_timer_init: really the light defaults): ambient 0x20 x 3 (0x00a0416c..74),
 * three light colors / directions (0x00a04178.. 0x00a041bc: (0x1e, 0, 0xaa), (0xf, 0, -0x3c),
 * (0x14, 0, 0x3c), the 0xc8 / 0x80 / 0x4b / 0x3c words), the view lights (0x00495758), then four
 * light sets through 0x00495050: set 1 (0xff, 0, 0), set 2 (0x3c, 0x8c, 0xaa), set 3 (0x100 x 3
 * for light 0, 0 for 1 and 2), each set's ambient copy (0x00a0422c / 0x00a04298 / 0x00a04304). */
void en7_lights_init(En2Env* e)
{
    static const uint32_t st[][2] = {
        {0x00a0416cu, 0x20}, {0x00a04170u, 0x20}, {0x00a04174u, 0x20}, {0x00a04180u, 0xaa}, {0x00a04178u, 0x1e},
        {0x00a0417cu, 0}, {0x00a0418cu, 0xffffffc4u}, {0x00a04188u, 0}, {0x00a04198u, 0x3c}, {0x00a04194u, 0},
        {0x00a041a8u, 0x80}, {0x00a041b8u, 0x80}, {0x00a04184u, 0xf}, {0x00a04190u, 0x14}, {0x00a0419cu, 0xc8},
        {0x00a041a0u, 0xc8}, {0x00a041a4u, 0xc8}, {0x00a041acu, 0x4b}, {0x00a041b0u, 0x4b}, {0x00a041b4u, 0x3c},
        {0x00a041bcu, 0x3c}};
    for (unsigned k = 0; k < sizeof st / sizeof st[0]; k++) en2_w32(e, st[k][0], st[k][1]);
    en2_call0(e, 0x00495758u);
    sx3(e, 0x00a0422cu);
    for (uint32_t k = 0; k < 3; k++) light(e, 1, k, 0xff, 0, 0);
    sx3(e, 0x00a04298u);
    for (uint32_t k = 0; k < 3; k++) light(e, 2, k, 0x3c, 0x8c, 0xaa);
    en2_w32(e, 0x00a04304u, 0x100);
    en2_w32(e, 0x00a04308u, 0x100);
    en2_w32(e, 0x00a0430cu, 0x100);
    light(e, 3, 0, 0x100, 0x100, 0x100);
    light(e, 3, 1, 0, 0, 0);
    light(e, 3, 2, 0, 0, 0);
}

/* 0x004a9c38 (subsys_unknown_init: it saves the input configuration): the registry key object
 * (0x004ac4b0, then 0x004ac528 on HKEY_LOCAL_MACHINE; failure -> its answer); the integers
 * 0x00493344() and the device types of 1, 4, 2 (input_get_device_type, sign extended bytes) under
 * the names [0x004e84e0 / e8 / f0 / f8] (reg_write_int); the 0x194 byte key table (0x004c5d9c's
 * when 0x00493398 says so and it has one, else a cleared stack buffer) under [0x004e8500]; the
 * three binding blocks of 0x004a217c / 0x004a2d9c / 0x004a25ec (pointer, size out) under
 * [0x004e8508 / 10 / 18] (0x004ac65c); flushed (0x004ac590). Returns the last write's answer. */
uint32_t en7_input_config_save(En2Env* e)
{
    const uint32_t buf = e->scratch, R = e->scratch + 0x194, SZ = e->scratch + 0x198;
    en2_call2(e, 0x004ac4b0u, R, 0);
    uint32_t r = en2_call3(e, 0x004ac528u, R, 0x80000002u, 0);
    if (r == 0) {
        en2_call3(e, 0x004ac614u, R, en2_r32(e, 0x004e84e0u), en2_call0(e, 0x00493344u));
        static const uint32_t dv[3][2] = {{1, 0x004e84e8u}, {4, 0x004e84f0u}, {2, 0x004e84f8u}};
        for (int k = 0; k < 3; k++) {
            uint32_t t = (uint32_t)(int32_t)(int8_t)en2_call1(e, 0x004932a0u, dv[k][0]);
            en2_call3(e, 0x004ac614u, R, en2_r32(e, dv[k][1]), t);
        }
        en2_call3(e, 0x004bf830u, buf, 0, 0x194);
        uint32_t p = 0;
        if ((en2_call0(e, 0x00493398u) & 0xff) != 0) p = en2_call0(e, 0x004c5d9cu);
        en2_call4(e, 0x004ac65cu, R, en2_r32(e, 0x004e8500u), p != 0 ? p : buf, 0x194);
        static const uint32_t bl[3][2] = {{0x004a217cu, 0x004e8508u}, {0x004a2d9cu, 0x004e8510u},
                                          {0x004a25ecu, 0x004e8518u}};
        for (int k = 0; k < 3; k++) {
            uint32_t q = en2_call1(e, bl[k][0], SZ);
            r = en2_call4(e, 0x004ac65cu, R, en2_r32(e, bl[k][1]), q, en2_r32(e, SZ));
        }
    }
    en2_call1(e, 0x004ac590u, R);
    return r;
}

/* 0x00493444 (subsys_input_config: the input setup): keyboard / mouse / joystick present
 * (0x004a1f40 / 0x004a21d4 / 0x004a2644 -> 0x00a03a7c / 80 / 84), the device flags 0x00a03a70 and
 * their copy 0x00a03a74 (bits 0 / 1 / 2) with the state bytes 0x00a03a78..7a (1 = present, 2 =
 * absent; 0x00a03a78 is only set when present), the 34 default bindings of the EXE table 0x004e8310
 * (5 bytes: action, key, joy, mouse, key 2) through 0x004a1ff8 (action, key, 0), 0x004a1ff8
 * (action, key 2, 1), 0x004a2278 (action, joy), 0x004a27bc (action, mouse), the devices' own
 * setups (0x0045ee20, 0x004a1f9c, 0x004a2220, 0x004a2764, 0x004a2018, 0x004a2290, 0x004a27d4),
 * the key slot bytes 0x00a03670..0x00a03a6f cleared, 0x004a97f4(). Returns 0x00a03a74. */
uint32_t en7_input_setup(En2Env* e)
{
    en2_w32(e, 0x00a03a7cu, en2_call0(e, 0x004a1f40u));
    en2_w32(e, 0x00a03a80u, en2_call0(e, 0x004a21d4u));
    en2_w32(e, 0x00a03a84u, en2_call0(e, 0x004a2644u));
    en2_w32(e, 0x00a03a70u, 0);
    en2_w32(e, 0x00a03a74u, 0);
    if (en2_r32(e, 0x00a03a7cu) != 0) {
        en2_w32(e, 0x00a03a70u, 1);
        en2_w32(e, 0x00a03a74u, 1);
        en2_w8(e, 0x00a03a78u, 1);
    }
    if (en2_r32(e, 0x00a03a80u) != 0) {
        en2_w8(e, 0x00a03a70u, en2_r8(e, 0x00a03a70u) | 2);
        uint8_t b = en2_r8(e, 0x00a03a74u) | 2;
        en2_w8(e, 0x00a03a79u, 1);
        en2_w8(e, 0x00a03a74u, b);
    } else {
        en2_w8(e, 0x00a03a79u, 2);
    }
    if (en2_r32(e, 0x00a03a84u) != 0) {
        en2_w8(e, 0x00a03a70u, en2_r8(e, 0x00a03a70u) | 4);
        uint8_t b = en2_r8(e, 0x00a03a74u) | 4;
        en2_w8(e, 0x00a03a7au, 1);
        en2_w8(e, 0x00a03a74u, b);
    } else {
        en2_w8(e, 0x00a03a7au, 2);
    }
    for (uint32_t c = 0; c != 0xaa; c += 5) {
        const uint32_t t = 0x004e8310u + c;
        en2_call3(e, 0x004a1ff8u, en2_r8(e, t), en2_r8(e, t + 1), 0);
        en2_call3(e, 0x004a1ff8u, en2_r8(e, t), en2_r8(e, t + 4), 1);
        en2_call2(e, 0x004a2278u, en2_r8(e, t), en2_r8(e, t + 2));
        en2_call2(e, 0x004a27bcu, en2_r8(e, t), en2_r8(e, t + 3));
    }
    static const uint32_t f[] = {0x0045ee20u, 0x004a1f9cu, 0x004a2220u, 0x004a2764u, 0x004a2018u, 0x004a2290u,
                                 0x004a27d4u};
    for (unsigned k = 0; k < 7; k++) en2_call0(e, f[k]);
    for (uint32_t c = 0; c < 8; c++) {
        uint32_t a = c;
        do { a += 8; en2_w8(e, 0x00a03668u + a, 0); } while (a != 0x400 + c);
    }
    en2_call0(e, 0x004a97f4u);
    return en2_r32(e, 0x00a03a74u);
}
