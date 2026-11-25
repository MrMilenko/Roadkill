/*
 * pf3_bank.c: TM2.EXE 0x004bb1f0 sound_bank_load (docs/PLATFORM.md Part D), ported over Pf2Env
 * (pf2_env.h). Emulator diff: tools/emu/check_pf3_bank.sh. x87 at 0x127f: the frequency is
 * computed in double, chopped (0x004979a2) and stored with fistp m64 (low dword kept).
 *
 * sound_bank_load(eax bank, edx file name): bank (0x1c bytes): +0x00 program count, +0x04
 * programs (12 bytes), +0x08 tone count, +0x0c tones (0x18 bytes), +0x10 sample count, +0x14
 * samples (0x44 bytes: +0x00 wave id, +0x10 root note, +0x24 frequency, +0x2c WAVEFORMATEX
 * (+0x30 rate), +0x40 IDirectSoundBuffer), +0x18 wave count. File: magic 0x03067027, the four
 * counts, the program records (0x004bb03c reads each), then the waves (0x1c byte header with the
 * data size at +0x18, then the data). Returns 0, or -1 on a failure.
 */
#include "pf2_env.h"
#include "../render/gpu3d_float.h"

#define R32 pf2_r32
#define W32 pf2_w32

#define F_OPEN 0x004ca197u        /* open(name, 0x200), cdecl */
#define F_FILELENGTH 0x004ca372u  /* (eax handle) */
#define F_READ 0x004c9ff0u        /* (eax handle, edx buffer, ebx bytes) -> bytes read */
#define F_CLOSE 0x004ca3cbu
#define F_NEW_ARRAY 0x004c4508u   /* (eax bytes, edx count, ebx element destructor) */
#define F_ARRAY_INIT 0x004ca4d3u  /* (eax) */
#define F_CTOR_ARRAY 0x004bf848u  /* (eax, edx count, ebx) */
#define F_GROUP_READ 0x004bb03cu  /* (eax program, edx handle, ebx tones, ecx samples) -> bytes */
#define F_LSEEK 0x004ca4e2u       /* (eax handle, edx 1, ebx 1, ecx position) */
#define F_MALLOC 0x004bf680u
#define F_FREE 0x004bf76fu
#define F_DUPLICATE 0x004b990cu   /* dsound_duplicate_buffer(eax device, edx out, ebx source) */
#define F_SET_FREQ 0x004b9a18u    /* dsbuf_set_frequency(eax device, edx buffer, ebx hz) */
#define F_SAMPLE_CREATE 0x004ba4ecu /* (eax sample, edx wave header, ebx wave data, ecx -1) */
#define DSDEV 0x00c850ecu

#define MAXWAVES 70               /* the original's two 0x118 byte tables */

static uint32_t fail_close(Pf2Env* e, uint32_t f) { pf2_call1(e, F_CLOSE, f); return 0xffffffffu; }

uint32_t pf3_sound_bank_load(Pf2Env* e, uint32_t bank, uint32_t name) {
    uint32_t magic = e->scratch;
    uint32_t hdr[MAXWAVES], dat[MAXWAVES];
    uint32_t f = pf2_call2(e, F_OPEN, name, 0x200);
    if (f == 0xffffffffu) return f;
    uint32_t len = pf2_call1(e, F_FILELENGTH, f);
    uint32_t c = pf2_call3(e, F_READ, f, magic, 4);
    /* ORIGINAL BUG: the magic is only checked when the read returned 4 bytes */
    if (c == 4 && R32(e, magic) != 0x03067027u) return fail_close(e, f);
    uint32_t pos = c;
    for (uint32_t off = 0; off <= 0x18; off += 8) {         /* the four counts */
        c = pf2_call3(e, F_READ, f, bank + off, 4);
        if (c != 4) return fail_close(e, f);
        pos += c;
    }
    uint32_t n = R32(e, bank);
    uint32_t p = pf2_call3(e, F_NEW_ARRAY, n * 12 + 4, n, 0x004e6318u);
    W32(e, bank + 4, pf2_call1(e, F_ARRAY_INIT, p));
    if (R32(e, bank + 4) == 0) return 0xffffffffu;           /* (the file stays open) */
    n = R32(e, bank + 8);
    p = pf2_call3(e, F_NEW_ARRAY, n * 24, n, 0x004e6304u);
    W32(e, bank + 0xc, pf2_call3(e, F_CTOR_ARRAY, p, n, 0x004e6304u));
    if (R32(e, bank + 0xc) == 0) return 0xffffffffu;
    n = R32(e, bank + 0x10);
    p = pf2_call3(e, F_NEW_ARRAY, n * 0x44 + 4, n, 0x004e62f0u);
    W32(e, bank + 0x14, pf2_call1(e, F_ARRAY_INIT, p));
    /* ORIGINAL BUG: tests the tone array (+0x0c) again, not the samples */
    if (R32(e, bank + 0xc) == 0) return 0xffffffffu;

    uint32_t tones = R32(e, bank + 0xc), samples = R32(e, bank + 0x14);
    for (uint32_t i = 0; (int32_t)i < (int32_t)R32(e, bank); i++) {
        uint32_t prog = R32(e, bank + 4) + 12 * i;
        pos += pf2_call4(e, F_GROUP_READ, prog, f, tones, samples);
        uint32_t voices = R32(e, R32(e, bank + 4) + 12 * i);
        samples += R32(e, tones + 0x10) * voices * 0x44;
        tones += voices * 0x18;
    }
    pf2_call4(e, F_LSEEK, f, 1, 1, pos);
    pos += 1;                     /* the original's ecx after the call, plus one */
    uint32_t buf = pf2_call1(e, F_MALLOC, 0x2625a0);
    if (buf == 0) return 0xffffffffu;
    pf2_call3(e, F_READ, f, buf, len - pos);
    uint32_t q = buf;
    for (uint32_t k = 0; (int32_t)k < (int32_t)R32(e, bank + 0x18); k++) {
        hdr[k] = q;
        q += 0x1c;
        dat[k] = q;
        q += R32(e, hdr[k] + 0x18);
    }
    pf2_call1(e, F_CLOSE, f);

    for (uint32_t i = 0; (int32_t)i < (int32_t)R32(e, bank + 0x10); i++) {
        uint32_t s = R32(e, bank + 0x14) + 0x44 * i, j = 0;
        /* an earlier sample of the same wave: share its buffer */
        for (uint32_t t = R32(e, bank + 0x14); t < s; t += 0x44)
            if (R32(e, s) == R32(e, t)) { j = t; break; }
        if (j) {
            W32(e, s, R32(e, j));
            memcpy(e->mem(e->user, s + 0x2c), e->mem(e->user, j + 0x2c), 18);
            pf2_call3(e, F_DUPLICATE, DSDEV, s + 0x40, R32(e, j + 0x40));
        } else {
            uint32_t id = R32(e, s);
            pf2_call4(e, F_SAMPLE_CREATE, s, hdr[id], dat[id], 0xffffffffu);
        }
        /* frequency = chop(rate * 130.81 / T[24 - root note]) (T: note frequencies 0x004e86d0;
           ORIGINAL BUG: the note is not range checked) */
        uint32_t ti = 0x18 - R32(e, s + 0x10);
        double x = g3_mul((double)R32(e, s + 0x30), (double)pf2_rf(e, 0x004e8730u));
        x = g3_div(x, (double)pf2_rf(e, 0x004e86d0u + 4 * ti));
        x = __builtin_trunc(x);
        uint32_t hz = (!g3_isnan(x) && x >= -9223372036854775808.0 && x < 9223372036854775808.0)
                          ? (uint32_t)(uint64_t)(int64_t)x : 0;    /* fistp m64: indefinite = 0x8000.. */
        W32(e, s + 0x24, hz);
        pf2_call3(e, F_SET_FREQ, DSDEV, R32(e, s + 0x40), hz);
    }
    pf2_call1(e, F_FREE, buf);
    return 0;
}
