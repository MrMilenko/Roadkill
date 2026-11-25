/*
 * mt_misc.h: small shell / car helpers in TM2.EXE 0x0045edd0..0x0045ee90, ported from the PC
 * machine code.
 *
 * Verified against the original under emulation (tools/emu/check_match_misc.sh).
 */
#ifndef MT_MISC_H
#define MT_MISC_H

#include <stdint.h>

typedef uint32_t (*MtMiscFn)(void* user, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx);

typedef struct {
    uint8_t* human;             /* 0x005030a8 human cars (0x898): +0x090 i16 car type, +0x1d4 i32 */
    const int32_t* human_count; /* 0x005075e0 */
    const uint32_t* human_objects; /* 0x00503088 (handles; object +8 i32) */
    uint8_t* flags_568;         /* 0x00507568 u8 per human car */
    const uint8_t* key_defaults; /* 0x004e7448: 51 entries {u8 key, a, b, c, d} */
    uint32_t* list_mode;        /* 0x004e7550 */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    MtMiscFn car_type_release;  /* 0x00453c38 (car type) */
    MtMiscFn key_set_pad;       /* 0x004a1ff8 (key, code, slot 0 / 1) */
    MtMiscFn key_set_2278;      /* 0x004a2278 (key, code) */
    MtMiscFn key_set_27bc;      /* 0x004a27bc (key, code) */
    MtMiscFn list_reset;        /* 0x004a2e43 (0x004e7548) */
    MtMiscFn list_create;       /* 0x004a2e60 (list, 1, 2, 3) -> list record handle */
    MtMiscFn list_462168;       /* 0x00462168 (0x0050e708) */
    MtMiscFn list_4a3620;       /* 0x004a3620 (0x0050ea98) */
    void* user;
} MtMisc;

/* 0x0045edd0: human car i leaves: its object +8 = 0, car +0x1d4 = 0, car_type_release(type),
 * flag 0x00507568[i] = 0. The original then counts eax from i up to count - 1 doing nothing (a
 * dead loop) and returns that value: max(i, count - 1). */
int32_t mt_human_car_leave(const MtMisc* m, int32_t i);
/* 0x0045ee20: the default key table: for each of the 51 entries key_set_pad(key, a, 0),
 * key_set_pad(key, d, 1), key_set_2278(key, b), key_set_27bc(key, c). */
void mt_key_defaults(const MtMisc* m);
/* 0x0045ee90: shell widget lists: list_reset(0x004e7548); the lists 0x0050e444 and 0x0050e4f8
 * created (record +0x30 = 0x004e50d8), 0x004e7550 = 1 / 2 / 3 around list_462168(0x0050e708)
 * and list_4a3620(0x0050ea98), whose result is returned. */
uint32_t mt_shell_lists_init(const MtMisc* m);

#endif /* MT_MISC_H */
