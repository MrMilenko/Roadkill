/*
 * pf6_sound.c: TM2.EXE's sound bank sample / voice readers and sample buffer creation
 * (docs/PLATFORM.md Part C / D), ported over Pf2Env (pf2_env.h). Emulator diff: tools/emu/check_pf6_sound.sh.
 * Sample (0x44 bytes): +0x00 id, +0x04, +0x08, +0x0c loop, +0x10 root note, +0x14, +0x18 / +0x1c loop
 * start / end, +0x20 = end - start, +0x24 frequency, +0x2c WAVEFORMATEX, +0x40 IDirectSoundBuffer.
 */
#include "pf2_env.h"

#define R32 pf2_r32
#define W32 pf2_w32
#define F_READ 0x004c9ff0u      /* read(eax handle, edx buffer, ebx bytes) -> count or -1 */
#define DSDEV 0x00c850ecu

static uint32_t rd4(Pf2Env* e, uint32_t f, uint32_t dst, uint32_t n) {
    uint32_t r = pf2_call3(e, F_READ, f, dst, 4);
    return r == 0xffffffffu ? n : n + r;
}

/* 0x004ba95c sample_read_header(eax sample, edx handle): eight 4 byte reads into +0, +4, +8,
   +0x10, +0x14, +0x0c, +0x18, +0x1c (a -1 result is not counted); +0x20 = +0x1c - +0x18; returns
   the bytes read */
uint32_t pf6_sample_read_header(Pf2Env* e, uint32_t s, uint32_t f) {
    static const uint32_t off[8] = {0, 4, 8, 0x10, 0x14, 0xc, 0x18, 0x1c};
    uint32_t n = 0;
    for (int k = 0; k < 8; k++) n = rd4(e, f, s + off[k], n);
    W32(e, s + 0x20, R32(e, s + 0x1c) - R32(e, s + 0x18));
    return n;
}

/* 0x004bac7c voice_read(eax voice, edx handle, ebx samples): five dwords +0..+0x10 (+0x10 = sample
   count), +0x14 = samples, then sample_read_header for each sample (count re-read); returns the
   bytes read */
uint32_t pf6_voice_read(Pf2Env* e, uint32_t v, uint32_t f, uint32_t samples) {
    uint32_t n = 0;
    for (uint32_t o = 0; o <= 0x10; o += 4) n = rd4(e, f, v + o, n);
    W32(e, v + 0x14, samples);
    for (uint32_t i = 0; (int32_t)i < (int32_t)R32(e, v + 0x10); i++)
        n += pf6_sample_read_header(e, R32(e, v + 0x14) + 0x44 * i, f);
    return n;
}

static void copy(Pf2Env* e, uint32_t dst, uint32_t src, uint32_t n) {
    for (uint32_t k = 0; k < n; k++) pf2_w8(e, dst + k, pf2_r8(e, src + k));
}

/* 0x004ba4ec sample_create(eax sample, edx wave header, ebx wave data, ecx id): WAVEFORMATEX at
   +0x2c from the header (tag 1, channels +6, rate +8, bytes/s +0x0c, block +0x10, bits +0x12,
   cbSize 0); id != -1: +0 = id. dsound_create_buffer(+0x40, header +0x18 bytes, flags 0xea)
   (failure: -1); dsbuf_lock of the whole buffer (failure: -1); the data copied into both locked
   parts -- ORIGINAL BUG: the second part also from the start of the data; dsbuf_unlock; returns 0 */
uint32_t pf6_sample_create(Pf2Env* e, uint32_t s, uint32_t h, uint32_t data, uint32_t id) {
    pf2_w16(e, s + 0x2c, 1);
    pf2_w16(e, s + 0x2e, pf2_r16(e, h + 6));
    W32(e, s + 0x30, R32(e, h + 8));
    W32(e, s + 0x34, R32(e, h + 0xc));
    pf2_w16(e, s + 0x38, pf2_r16(e, h + 0x10));
    pf2_w16(e, s + 0x3c, 0);
    pf2_w16(e, s + 0x3a, pf2_r16(e, h + 0x12));
    if (id != 0xffffffffu) W32(e, s, id);
    uint32_t a[5] = {DSDEV, s + 0x40, R32(e, h + 0x18), s + 0x2c, 0xea};
    if (e->call(e->user, 0x004b97b0u, a, 5)) return 0xffffffffu;
    uint32_t p1 = e->scratch, p2 = e->scratch + 4, n1 = e->scratch + 8, n2 = e->scratch + 0xc;
    uint32_t b[7] = {DSDEV, R32(e, s + 0x40), R32(e, h + 0x18), p1, n1, p2, n2};
    if (e->call(e->user, 0x004b992cu, b, 7)) return 0xffffffffu;
    copy(e, R32(e, p1), data, R32(e, n1));
    copy(e, R32(e, p2), data, R32(e, n2));
    uint32_t c[6] = {DSDEV, R32(e, s + 0x40), R32(e, p1), R32(e, n1), R32(e, p2), R32(e, n2)};
    e->call(e->user, 0x004b995cu, c, 6);
    return 0;
}
