/*
 * en5_kbdev.c: 0x004a3964, a keyboard device record o (the window layer's copy of 0x0049e638):
 *   +0x004 IDirectInputDevice, +0x27c acquired, +0x284 shift, +0x288 alt, +0x28c control,
 *   +0x290 caps lock toggle, +0x294 its latch, +0x298 buffered mode, +0x2a0 buffer entries,
 *   +0x2a4 key states (0x100), +0x4a4 their copy, +0x5a4 key queue (0x100), +0x6a4 queue count,
 *   +0x6a8 read index, +0x6b0 write index.
 * Immediate mode: GetDeviceState (+0x24, 0x100 bytes into +0x2a4); DIERR_INPUTLOST -> +0x27c = 0
 * and Acquire (+0x1c), 0x004937b8 if that fails; other errors 0x004937b8; these return at once.
 * Then every state with bit 7 set becomes 0x80.
 * Buffered mode: GetDeviceData (+0x28) until no entries or an error (DIERR_INPUTLOST also clears
 * +0x27c; errors call 0x004937b8): press -> 0x80, release -> 1 if it was 0x80 else 0. Then every
 * key whose state has bit 7, and again every key whose state has bit 0, is queued (the read index
 * pushed when the queue is full; indexes wrap after 0xff) and fed to 0x0049dfac.
 * Both: the modifier flags from the state bytes 0x2a / 0x36, 0x38 / 0xb8, 0x1d / 0x9d, caps lock
 * 0x3a toggling +0x290 once per press, and the states copied to +0x4a4 (DISASSEMBLY TRAP: f2 a5 is
 * a full rep copy, shown by capstone as one movsd). Return not defined.
 */
#include "en5_kbdev.h"

static void queue(En2Env* e, uint32_t o, uint32_t k)
{
    en2_w8(e, o + 0x5a4 + en2_r32(e, o + 0x6b0), (uint8_t)k);
    en2_w32(e, o + 0x6a4, en2_r32(e, o + 0x6a4) + 1);
    int32_t c = (int32_t)en2_r32(e, o + 0x6b0);
    en2_w32(e, o + 0x6b0, 0xff > c ? (uint32_t)(c + 1) : 0);
    uint32_t w = en2_r32(e, o + 0x6b0);
    if (w == en2_r32(e, o + 0x6a8)) {
        w++;
        en2_w32(e, o + 0x6a8, w);
        if (0xff < (int32_t)w) en2_w32(e, o + 0x6a8, 0);
    }
    en2_call1(e, 0x0049dfacu, k & 0xff);
}

static void finish(En2Env* e, uint32_t o)
{
    const uint32_t st = o + 0x2a4;
    en2_w32(e, o + 0x284, (en2_r8(e, st + 0x2a) || en2_r8(e, st + 0x36)) ? 1 : 0);
    en2_w32(e, o + 0x288, (en2_r8(e, st + 0x38) || en2_r8(e, st + 0xb8)) ? 1 : 0);
    en2_w32(e, o + 0x28c, (en2_r8(e, st + 0x1d) || en2_r8(e, st + 0x9d)) ? 1 : 0);
    if (en2_r8(e, st + 0x3a) != 0 && en2_r32(e, o + 0x294) != 1) {
        uint32_t t = en2_r32(e, o + 0x290);
        en2_w32(e, o + 0x294, 1);
        en2_w32(e, o + 0x290, ~t);
    } else if (en2_r8(e, st + 0x3a) == 0) {
        en2_w32(e, o + 0x294, 0);
    }
    for (uint32_t k = 0; k < 0x100; k += 4) en2_w32(e, o + 0x4a4 + k, en2_r32(e, st + k));
}

static uint32_t com(En2Env* e, uint32_t off, const uint32_t* v, int n)
{
    return e->call(e->user, en2_r32(e, en2_r32(e, v[0]) + off), v, n);
}

void en5_kbdev_update(En2Env* e, uint32_t o)
{
    const uint32_t st = o + 0x2a4;
    if (en2_r32(e, o + 0x298) == 0) {
        uint32_t v[3] = {en2_r32(e, o + 4), 0x100, st};
        uint32_t r = com(e, 0x24, v, 3);
        if (r != 0) {
            if (r == 0x8007001eu) {
                en2_w32(e, o + 0x27c, 0);
                uint32_t a[1] = {en2_r32(e, o + 4)};
                if (com(e, 0x1c, a, 1) == 0) return;
            }
            en2_call0(e, 0x004937b8u);
            return;
        }
        for (uint32_t k = 0; k < 0x100; k++)
            if (en2_r8(e, st + k) & 0x80) en2_w8(e, st + k, 0x80);
        finish(e, o);
        return;
    }
    const uint32_t S = e->scratch, CNT = S + 0xa0;
    for (;;) {
        en2_w32(e, CNT, en2_r32(e, o + 0x2a0));
        uint32_t v[5] = {en2_r32(e, o + 4), 0x10, S, CNT, 0};
        uint32_t r = com(e, 0x28, v, 5);
        if (r != 0 && r != 1) {
            en2_call0(e, 0x004937b8u);
            if (r == 0x8007001eu) en2_w32(e, o + 0x27c, 0);
            break;
        }
        if (en2_r32(e, CNT) == 0) break;
        for (uint32_t i = 0, off = 0; i < en2_r32(e, CNT); i++, off += 0x10) {
            uint32_t a = st + en2_r32(e, S + off);
            if (en2_r8(e, S + off + 4) & 0x80) en2_w8(e, a, 0x80);
            else en2_w8(e, a, en2_r8(e, a) == 0x80 ? 1 : 0);
        }
    }
    for (uint32_t k = 0; k < 0x100; k++) {
        if (en2_r8(e, st + k) & 0x80) queue(e, o, k);
        if (en2_r8(e, st + k) & 1) queue(e, o, k);
    }
    finish(e, o);
}
