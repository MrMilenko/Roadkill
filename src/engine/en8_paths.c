/*
 * en8_paths.c: the game's root paths found at start up, TM2.EXE 0x00498ddc (see en8_paths.h).
 * Verified under emulation: tools/emu/check_en8_paths.sh.
 */
#include "en8_paths.h"

#define R_SET_PATH 0x004ac4b0u
#define R_OPEN     0x004ac528u
#define R_INT      0x004ac5bcu
#define R_FLUSH    0x004ac590u
#define R_DATA     0x004ac5e4u
#define I_wsprintfA 0x00c90658u   /* cdecl */
#define I_GetDriveTypeA 0x00c902c8u
#define I_CreateFileA 0x00c902a8u
#define I_GetFileSize 0x00c902d0u
#define I_CloseHandle 0x00c902a4u
#define I_lstrcpyA 0x00c9031cu
#define F_ERROR_BOX 0x004bc660u   /* cdecl (message id) */

enum { P_OWN = 0, P_INST = 0x12c, P_FILE = 0x258, P_ROOT = 0x384, P_SIZE = 0x4b0, P_VAL = 0x4b4, P_KEY = 0x4b8,
       P_FSIZE = 0x4bc, P_DRIVE = 0x4c0, P_BAD = 0x4c4 };

/* the original's copy loop (two bytes a step, stops after the zero) */
static void copy_str(En2Env* e, uint32_t dst, uint32_t src)
{
    for (;;) {
        uint8_t c = en2_r8(e, src);
        en2_w8(e, dst, c);
        if (c == 0) return;
        c = en2_r8(e, src + 1);
        src += 2;
        en2_w8(e, dst + 1, c);
        dst += 2;
        if (c == 0) return;
    }
}
static int value_ok(En2Env* e, uint32_t F, uint32_t name)
{
    return en2_call3(e, R_INT, F + P_KEY, name, F + P_VAL) == 0 && en2_r32(e, F + P_VAL) != 0;
}
/* a slot from its value: 1 = the install directory, else the CD root */
static uint32_t slot_src(En2Env* e, uint32_t F, uint32_t name)
{
    if (en2_call3(e, R_INT, F + P_KEY, name, F + P_VAL) == 0 && en2_r32(e, F + P_VAL) == 1) return F + P_INST;
    return F + P_ROOT;
}

uint32_t en8_paths_setup(En2Env* e, uint32_t name)
{
    const uint32_t F = e->scratch;
    en2_call2(e, R_SET_PATH, F + P_KEY, 0);
    copy_str(e, 0x00a3a49cu, name);
    en2_w32(e, F + P_BAD, 0);
    if (en2_call3(e, R_OPEN, F + P_KEY, 0x80000002u, 0) != 0) {
        en2_w32(e, F + P_BAD, 1);
    } else {
        en2_w32(e, F + P_SIZE, 0x12c);
        if (en2_call4(e, R_DATA, F + P_KEY, 0x004e2fbcu, F + P_INST, F + P_SIZE) != 0) en2_w32(e, F + P_BAD, 1);
        static const uint32_t names[4] = {0x004e2fceu, 0x004e2fd9u, 0x004e2fdfu, 0x004e2fe8u};
        for (int k = 0; k < 4; k++)
            if (!value_ok(e, F, names[k])) en2_w32(e, F + P_BAD, 1);
        en2_call1(e, R_FLUSH, F + P_KEY);
    }
    if (en2_r32(e, F + P_BAD) == 0) {
        en2_call2(e, I_wsprintfA, F + P_ROOT, 0x004e2fffu);   /* "." */
    } else {
        int32_t d;
        for (;;) {
            for (d = 'A'; d <= 'Z'; d++) {
                en2_call3(e, I_wsprintfA, F + P_DRIVE, 0x004e2fefu, (uint32_t)d);   /* "%c:\" */
                if (en2_call1(e, I_GetDriveTypeA, F + P_DRIVE) != 5 /* DRIVE_CDROM */) continue;
                en2_call4(e, I_wsprintfA, F + P_FILE, 0x004e2ff4u, (uint32_t)d, name);   /* "%c:\%s" */
                uint32_t v[7] = {F + P_FILE, 0x80000000u, 1, 0, 3, 0x10000080u, 0};
                uint32_t h = e->call(e->user, I_CreateFileA, v, 7);
                en2_w32(e, F + P_FSIZE, h != 0xffffffffu ? en2_call2(e, I_GetFileSize, h, 0) : 0);
                if (h == 0xffffffffu) continue;
                en2_call1(e, I_CloseHandle, h);
                break;
            }
            if (d > 'Z') d = -1;
            en2_w32(e, 0x00a3a5d0u, (uint32_t)d);
            if (d >= 0) break;
            if (en2_call1(e, F_ERROR_BOX, 0x16) != 4 /* IDRETRY */) return 0;
        }
        en2_call3(e, I_wsprintfA, F + P_ROOT, 0x004e2ffbu, (uint32_t)d);   /* "%c:" */
    }
    if (en2_call3(e, R_OPEN, F + P_KEY, 0x80000002u, 0) != 0) {
        static const uint32_t dot[7] = {0x004e3066u, 0x004e3068u, 0x004e306au, 0x004e306cu, 0x004e306eu,
                                        0x004e3070u, 0x004e3072u};
        for (int k = 0; k < 7; k++) en2_call2(e, I_lstrcpyA, 0x00a39c68u + 0x12cu * (uint32_t)k, dot[k]);
        return 1;
    }
    en2_w32(e, F + P_SIZE, 0x12c);
    if (en2_call4(e, R_DATA, F + P_KEY, 0x004e3001u, F + P_INST, F + P_SIZE) != 0)
        en2_call2(e, I_lstrcpyA, F + P_INST, F + P_ROOT);
    static const uint32_t slot_names[6] = {0x004e3013u, 0x004e301eu, 0x004e3024u, 0x004e302du, 0x004e3037u, 0x004e3045u};
    for (int k = 0; k < 6; k++)
        en2_call2(e, I_lstrcpyA, 0x00a39c68u + 0x12cu * (uint32_t)k, slot_src(e, F, slot_names[k]));
    uint32_t src;
    if (en2_call3(e, R_INT, F + P_KEY, 0x004e304cu, F + P_VAL) != 0) {
        src = 0x004e3065u;
    } else {
        uint32_t v = en2_r32(e, F + P_VAL);
        if (v == 1) src = F + P_INST;
        else if (v == 0) src = F + P_ROOT;
        else if (v == 0x63) {
            en2_w32(e, F + P_SIZE, 0x12c);
            src = en2_call4(e, R_DATA, F + P_KEY, 0x004e3053u, F + P_OWN, F + P_SIZE) == 0 ? F + P_OWN : 0x004e3063u;
        } else src = 0x004e3064u;
    }
    copy_str(e, 0x00a3a370u, src);
    en2_call1(e, R_FLUSH, F + P_KEY);
    return 1;
}
