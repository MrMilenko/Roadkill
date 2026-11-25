/*
 * pf_sfx.c: TM2.EXE 0x004bbaf8 sfx_slot_apply and the request setters (see pf_sfx.h).
 */
#include "pf_sfx.h"

#include <math.h>

/* x87 sequence of 0x004bbaf8: fild v; fmul 1/127.0 (0x004e396c); log 0x004c9f14 (the natural
   log: it loads ln 2 for fyl2x); fmul 2000.0 (0x004e3974); x < 0 ? x : 0 (NaN -> 0); truncate
   (0x004979a2 is frndint with the rounding control set to chop); > -10000.0 (0x004e397c) ? it :
   -10000; truncate; fistp.
   ORIGINAL BUG: DirectSound wants 2000 * log10 (hundredths of a dB); the natural log makes
   the curve 2.3 times steeper (volume 64 -> -1370 instead of -595). Kept: it is how the game
   sounds. v <= 0: log's error path (0x004cd015) returns -infinity -> -10000. */
int32_t pf_sfx_volume_db(int32_t v) {
    double x = (v > 0) ? log((double)v * (1.0 / 127.0)) * 2000.0 : -INFINITY;
    if (!(x < 0.0)) x = 0.0;
    double y = trunc(x);
    if (!(y > -10000.0)) y = -10000.0;
    return (int32_t)trunc(y);
}

/* 0x004bbaf8 (eax slot). Returns 1 when the instance cannot be made (flags = 0) or when the
   last Play failed; else 0. The flags are cleared on every path. */
int32_t pf_sfx_slot_apply(const PfSfxBackend* be, PfSfxSlot* s) {
    PfSfxInstance* in = be->instance_make(be->user, s->program, s->key);
    s->instance = in;
    if (!in) {
        s->flags = 0;
        return 1;
    }
    if (s->flags & 4) {
        int32_t v1 = (int32_t)((uint32_t)in->volume * (uint32_t)s->volume) / 127;   /* imul, idiv */
        for (int32_t i = 0; i < in->voice_count; i++) {
            PfSfxVoice* v = &in->voices[i];
            int32_t v2 = (int32_t)((uint32_t)v->volume * (uint32_t)v1) / 127;
            be->set_volume(be->user, v->buffer, pf_sfx_volume_db(v2));
        }
    }
    if (s->flags & 8) {
        for (int32_t i = 0; i < in->voice_count; i++) be->set_pan(be->user, &in->voices[i], s->pan);
    }
    if (s->flags & 0x10) {
        for (int32_t i = 0; i < in->voice_count; i++) be->set_pitch(be->user, &in->voices[i], s->pitch);
    }
    if (!(s->flags & 1)) {
        s->flags = 0;
        return 0;
    }
    /* play: only when no voice is playing (GetStatus bit 0, DSBSTATUS_PLAYING), every voice, the
       loop flag from loop mode == 1. With no voices the original reads its "playing" and
       "result" stack words uninitialised (ORIGINAL BUG); the port takes "not playing", 0. */
    uint32_t playing = 0;
    for (int32_t i = 0; i < in->voice_count; i++) {
        uint32_t st = 0;
        be->get_status(be->user, in->voices[i].buffer, &st);
        playing = (st & 1) != 0;
        if (playing == 1) break;
    }
    if (playing) {
        s->flags = 0;
        return 0;
    }
    int32_t res = 0;
    for (int32_t i = 0; i < in->voice_count; i++)
        res = be->play(be->user, in->voices[i].buffer, in->voices[i].loop == 1);
    s->flags = 0;
    return res != 0 ? 1 : 0;
}

/* the setters set a request bit (the byte at +0x0c) and return 0 */
int32_t pf_sfx_request_play(PfSfxSlot* s) { s->flags |= 1; return 0; }
int32_t pf_sfx_request_2(PfSfxSlot* s) { s->flags |= 2; return 0; }
int32_t pf_sfx_request_volume(PfSfxSlot* s, int32_t v) { s->volume = v; s->flags |= 4; return 0; }
int32_t pf_sfx_request_pan(PfSfxSlot* s, int32_t p) { s->pan = p; s->flags |= 8; return 0; }
int32_t pf_sfx_request_pitch(PfSfxSlot* s, int32_t p) { s->pitch = p; s->flags |= 0x10; return 0; }
