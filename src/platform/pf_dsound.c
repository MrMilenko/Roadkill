/*
 * pf_dsound.c: TM2.EXE's DirectSound buffer wrappers and sample sound math (see pf_dsound.h).
 *
 * The x87 code is ported in double precision: the original runs with the Win32 default FPU
 * control word 0x027f (53 bit mantissa), which the emulator diff also uses. 0x004979a2 is
 * frndint with the rounding control set to chop: trunc().
 */
#include "pf_dsound.h"

#include <math.h>

#define BE (d->be)

/* fistp of a value that is already whole: out of the int32 range (or NaN) gives the x87
   "integer indefinite" 0x80000000 */
static int32_t fist(double v) {
    if (!(v >= -2147483648.0 && v < 2147483648.0)) return INT32_MIN;
    return (int32_t)v;
}

/* 0x004b987c play(eax dev, edx buf, ebx flags): Play(buf, 0, 0, flags) */
int32_t pf_ds_play(PfDsDevice* d, PfDsObj* b, uint32_t flags) {
    if (!b) return PF_DS_E_FAIL;
    return BE->play(BE->user, b, 0, 0, flags);
}

/* 0x004b9898 stop(eax dev, edx buf): Stop(buf); not the primary: SetCurrentPosition(buf, 0)
   (result dropped); returns Stop's */
int32_t pf_ds_stop(PfDsDevice* d, PfDsObj* b) {
    if (!b) return PF_DS_E_FAIL;
    int32_t r = BE->stop(BE->user, b);
    if (b != d->primary) BE->set_position(BE->user, b, 0);
    return r;
}

/* 0x004b98cc (eax dev, edx buf): no IDirectSound -> DSERR_UNINITIALIZED; Release(buf) */
int32_t pf_ds_release_buffer(PfDsDevice* d, PfDsObj* b) {
    if (!d->ds) return PF_DSERR_UNINITIALIZED;
    if (!b) return PF_DS_E_FAIL;
    return BE->release(BE->user, b);
}

/* 0x004b98ec (eax dev): Release(IDirectSound); the field is not cleared */
int32_t pf_ds_release_device(PfDsDevice* d) {
    if (!d->ds) return PF_DS_E_FAIL;
    return BE->release(BE->user, d->ds);
}

PfDsObj* pf_ds_primary(const PfDsDevice* d) { return d->primary; }   /* 0x004b9908 */
uint32_t pf_ds_field8(const PfDsDevice* d) { return d->field8; }     /* 0x004b9a48 */

/* 0x004b990c (eax dev, edx out, ebx source): needs the IDirectSound and the source (the out
   pointer is not checked); IDirectSound::DuplicateSoundBuffer(source, out) */
int32_t pf_ds_duplicate(PfDsDevice* d, PfDsObj* src, PfDsObj** out) {
    if (!d->ds || !src) return PF_DS_E_FAIL;
    return BE->duplicate(BE->user, d->ds, src, out);
}

/* 0x004b992c (eax dev, edx buf, ebx bytes, ecx ptr1 out, stack: len1 out, ptr2 out, len2 out;
   ret 0xc): Lock(buf, offset 0, bytes, ...,  flags 0) */
int32_t pf_ds_lock(PfDsDevice* d, PfDsObj* b, uint32_t bytes, void* p1, void* n1, void* p2, void* n2) {
    if (!b) return PF_DS_E_FAIL;
    return BE->lock(BE->user, b, 0, bytes, p1, n1, p2, n2, 0);
}

/* 0x004b995c (eax dev, edx buf, ebx ptr1, ecx len1, stack: ptr2, len2; ret 8): Unlock */
int32_t pf_ds_unlock(PfDsDevice* d, PfDsObj* b, void* p1, uint32_t n1, void* p2, uint32_t n2) {
    if (!b) return PF_DS_E_FAIL;
    return BE->unlock(BE->user, b, p1, n1, p2, n2);
}

int32_t pf_ds_set_position(PfDsDevice* d, PfDsObj* b, uint32_t pos) {   /* 0x004b9984 SetCurrentPosition */
    if (!b) return PF_DS_E_FAIL;
    return BE->set_position(BE->user, b, pos);
}

int32_t pf_ds_get_status(PfDsDevice* d, PfDsObj* b, uint32_t* status) {  /* 0x004b999c GetStatus */
    if (!b) return PF_DS_E_FAIL;
    return BE->get_status(BE->user, b, status);
}

int32_t pf_ds_set_volume(PfDsDevice* d, PfDsObj* b, int32_t v) {        /* 0x004b99b4 SetVolume */
    if (!b) return PF_DS_E_FAIL;
    return BE->set_volume(BE->user, b, v);
}

/* 0x004b99cc (eax dev, edx buf, ebx out): GetVolume into a local, copied to *out whatever the
   result (on failure the local is uninitialised in the original; 0 here) */
int32_t pf_ds_get_volume(PfDsDevice* d, PfDsObj* b, int32_t* v) {
    if (!b) return PF_DS_E_FAIL;
    int32_t t = 0;
    int32_t r = BE->get_volume(BE->user, b, &t);
    *v = t;
    return r;
}

/* 0x004b99f8 (eax dev, edx buf, ebx pan): negated when +0x1c (reverse stereo) is set; SetPan */
int32_t pf_ds_set_pan(PfDsDevice* d, PfDsObj* b, int32_t pan) {
    if (!b) return PF_DS_E_FAIL;
    if (d->reverse_stereo) pan = (int32_t)(0u - (uint32_t)pan);
    return BE->set_pan(BE->user, b, pan);
}

int32_t pf_ds_set_frequency(PfDsDevice* d, PfDsObj* b, uint32_t hz) {   /* 0x004b9a18 SetFrequency */
    if (!b) return PF_DS_E_FAIL;
    return BE->set_frequency(BE->user, b, hz);
}

int32_t pf_ds_get_frequency(PfDsDevice* d, PfDsObj* b, void* hz) {      /* 0x004b9a30 GetFrequency */
    if (!b) return PF_DS_E_FAIL;
    return BE->get_frequency(BE->user, b, hz);
}

/* 0x004ba76c's curve: a = |pan| (imul -1: INT_MIN stays negative); a <= 64: a * 125 * 0.125;
   a <= 96: a * 125 * 0.25 - 1000; else a * 250 - 22000 (the PS1 0..127 pan to DirectSound's
   hundredths of a dB, 64 -> 1000, 96 -> 2000, 127 -> 9750); chop, fistp; the sign back (imul -1);
   then clamped to > -10000 and < 10000 (else -10000 / 10000), chop, fistp. */
int32_t pf_ds_pan_value(int32_t pan) {
    int32_t a = pan, neg = 0;
    if (pan != 0) {
        if (pan < 0) { a = (int32_t)(0u - (uint32_t)pan); neg = 1; }
        double x;
        if (a <= 0x40) x = (double)a * 125.0 * 0.125;
        else if (a <= 0x60) x = (double)a * 125.0 * 0.25 + -1000.0;
        else x = (double)a * 250.0 + -22000.0;
        a = fist(trunc(x));
        if (neg) a = (int32_t)(0u - (uint32_t)a);
    }
    double v = (double)a;
    if (!(v > -10000.0)) v = -10000.0;
    v = trunc(v);
    if (!(v < 10000.0)) v = 10000.0;
    return fist(trunc(v));
}

/* 0x004ba76c (eax sample, edx pan): set pan 0x004b99f8 on the sample's buffer */
int32_t pf_ds_sample_set_pan(PfDsDevice* d, const PfDsSample* s, int32_t pan) {
    return pf_ds_set_pan(d, s->buffer, pf_ds_pan_value(pan));
}

/* 0x004ba890's rate: n = min(n, 99) (no lower clamp); x = bend * n * 0.01 + note; i = chop(x);
   f = T[24-i] + (T[23-i] - T[24-i]) * (x - i) (T = the note frequencies 0x004e86d0, floats);
   rate = chop(rate * T[24] / f) (T[24] = 130.81, the constant 0x004e8730; rate as an unsigned
   64 bit integer). ORIGINAL BUG: i is not range checked (x outside -1..24 reads past the table). */
int32_t pf_ds_pitch_value(const PfDsDevice* d, const PfDsSample* s, int32_t n) {
    const float* T = d->note_freq;
    if (n > 0x63) n = 0x63;
    double x = (double)s->bend * (double)n * 0.01 + (double)s->note;
    int32_t i = fist(trunc(x));
    double f = ((double)T[23 - i] - (double)T[24 - i]) * (x - (double)i) + (double)T[24 - i];
    double r = (double)(uint64_t)s->rate * (double)T[24] / f;
    return fist(trunc(r));
}

/* 0x004ba890 (eax sample, edx n): set frequency 0x004b9a18 on the sample's buffer */
int32_t pf_ds_sample_set_pitch(PfDsDevice* d, const PfDsSample* s, int32_t n) {
    return pf_ds_set_frequency(d, s->buffer, (uint32_t)pf_ds_pitch_value(d, s, n));
}

/* 0x004ba938 (eax sample, edx out): GetStatus into *out, then *out = DSBSTATUS_PLAYING bit */
void pf_ds_sample_is_playing(PfDsDevice* d, const PfDsSample* s, uint32_t* out) {
    pf_ds_get_status(d, s->buffer, out);
    *out = (*out & 1) != 0;
}

/* 0x004b9168 play_at(eax, edx, ebx passed on; ecx volume 0..99; stack: distance, lateral,
   arg, unused; ret 0x10). dist = min(distance, max 0x00c85040); vol = (max - dist) *
   (volume * 127 / 99) / max (32 bit, idiv); pan = 0 at distance 0; |lateral| <= |dist|:
   chop(asin(lateral / |dist|) * 255 / pi) (asin = 0x004c9eca: atan2(x, sqrt(1 - x * x)));
   else lateral / 5. -> sfx_start 0x004bb9c4(eax, edx, ebx, ecx vol, stack pan, arg) */
int32_t pf_ds_play_at(PfDsDevice* d, uint32_t a, uint32_t b, uint32_t c, int32_t volume, int32_t distance,
                      int32_t lateral, uint32_t arg) {
    int32_t max = d->max_distance;
    int32_t dist = distance < max ? distance : max;
    int32_t v99 = (int32_t)((uint32_t)volume * 127u) / 99;
    int32_t vol = (int32_t)((uint32_t)(max - dist) * (uint32_t)v99) / max;
    int32_t pan;
    if (dist == 0) {
        pan = 0;
    } else {
        double ad = fabs((double)dist), ax = fabs((double)lateral);
        if (!(ax <= ad)) {
            pan = lateral / 5;
        } else {
            double q = (double)lateral / ad;
            double as = atan2(q, sqrt(1.0 - q * q));
            pan = fist(trunc(as * 255.0 * 0.31830988614222805));
        }
    }
    return BE->sfx_start(BE->user, a, b, c, vol, pan, arg);
}
