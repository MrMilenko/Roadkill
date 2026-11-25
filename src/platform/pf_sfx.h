/*
 * pf_sfx.h: the per slot update of TM2.EXE's sound effect voice list, the code between the game's
 * sound requests and DirectSound (docs/PLATFORM.md, "Sound: voice list"):
 *   0x004bbaf8 sfx_slot_apply   (re)make the slot's instance, then apply the pending volume /
 *                               pan / pitch / play requests to its DirectSound buffers
 *   0x004bbd80 / 0x004bbd88 / 0x004bbd90 / 0x004bbda4 / 0x004bbdb8  the request setters
 *
 * Slot record (offsets in the original): +0x00 program, +0x04 instance (written), +0x08 key,
 * +0x0c request flags (1 play, 2 (set, not read here), 4 volume, 8 pan, 0x10 pitch; cleared by
 * apply), +0x10 volume 0..127, +0x14 pan, +0x18 pitch.
 * Instance (from 0x004bae50): +0x00 volume 0..127, +0x10 voice count, +0x14 voices (0x44 bytes
 * each: +0x04 volume 0..127, +0x0c loop mode (1 = loop), +0x40 sound buffer handle).
 */
#ifndef PF_SFX_H
#define PF_SFX_H

#include <stdint.h>

typedef struct {
    int32_t volume;     /* voice +0x04 */
    int32_t loop;       /* voice +0x0c */
    uintptr_t buffer;   /* voice +0x40: the sound device's buffer handle */
} PfSfxVoice;

typedef struct {
    int32_t volume;         /* +0x00 */
    int32_t voice_count;    /* +0x10 */
    PfSfxVoice* voices;     /* +0x14 */
} PfSfxInstance;

typedef struct {
    uintptr_t program;          /* +0x00 */
    PfSfxInstance* instance;    /* +0x04 */
    int32_t key;                /* +0x08 */
    uint32_t flags;             /* +0x0c */
    int32_t volume, pan, pitch; /* +0x10, +0x14, +0x18 */
} PfSfxSlot;

typedef struct {
    void* user;
    PfSfxInstance* (*instance_make)(void* user, uintptr_t program, int32_t key);       /* 0x004bae50 */
    /* the sound device 0x00c850ec's buffer wrappers (docs/PLATFORM.md "Sound: device") */
    void (*set_volume)(void* user, uintptr_t buffer, int32_t hundredths_db);            /* 0x004b99b4: SetVolume */
    void (*set_pan)(void* user, PfSfxVoice* v, int32_t pan);                            /* 0x004ba76c */
    void (*set_pitch)(void* user, PfSfxVoice* v, int32_t pitch);                        /* 0x004ba890 */
    void (*get_status)(void* user, uintptr_t buffer, uint32_t* status);                 /* 0x004b999c: GetStatus */
    int32_t (*play)(void* user, uintptr_t buffer, int32_t loop);                        /* 0x004b987c: Play */
} PfSfxBackend;

/* DirectSound volume (hundredths of a dB, -10000..0) for a 0..127 voice volume, as 0x004bbaf8 */
int32_t pf_sfx_volume_db(int32_t v);
int32_t pf_sfx_slot_apply(const PfSfxBackend* be, PfSfxSlot* s);
int32_t pf_sfx_request_play(PfSfxSlot* s);                  /* 0x004bbd80 */
int32_t pf_sfx_request_2(PfSfxSlot* s);                     /* 0x004bbd88 */
int32_t pf_sfx_request_volume(PfSfxSlot* s, int32_t v);     /* 0x004bbd90 */
int32_t pf_sfx_request_pan(PfSfxSlot* s, int32_t p);        /* 0x004bbda4 */
int32_t pf_sfx_request_pitch(PfSfxSlot* s, int32_t p);      /* 0x004bbdb8 */

#endif /* PF_SFX_H */
