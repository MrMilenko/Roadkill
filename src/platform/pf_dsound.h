/*
 * pf_dsound.h: TM2.EXE's DirectSound buffer wrappers and the sample level sound math above them,
 * ported over a backend (docs/PLATFORM.md, "Sound: device" and "Sound: samples").
 *
 * The sound device object 0x00c850ec (PfDsDevice): +0x00 IDirectSound, +0x04 primary buffer,
 * +0x08 (read by 0x004b9a48), +0x1c reverse stereo (pan negated).
 * Sample record (0x44 bytes, PfDsSample): +0x18 base note, +0x20 pitch bend range (in 1/100 of a
 * note per bend step), +0x24 base rate (Hz), +0x40 its sound buffer.
 *
 * Buffer wrappers (eax device, edx buffer, ebx argument; a NULL buffer -> E_FAIL, no call):
 *   0x004b987c play  0x004b9898 stop  0x004b98cc release buffer  0x004b98ec release device
 *   0x004b9908 primary  0x004b990c duplicate  0x004b992c lock  0x004b995c unlock
 *   0x004b9984 set position  0x004b999c get status  0x004b99b4 set volume  0x004b99cc get volume
 *   0x004b99f8 set pan  0x004b9a18 set frequency  0x004b9a30 get frequency  0x004b9a48 +8
 * Sample math: 0x004ba76c set pan, 0x004ba890 set pitch, 0x004ba938 is playing,
 *   0x004b9168 play at a distance (volume / pan from distance and lateral offset).
 * Emulator diff: tools/emu/check_pf_dsound.sh.
 */
#ifndef PF_DSOUND_H
#define PF_DSOUND_H

#include <stdint.h>

#define PF_DS_E_FAIL ((int32_t)0x80004005)
#define PF_DSERR_UNINITIALIZED ((int32_t)0x887800aa)

typedef struct PfDsObj PfDsObj;   /* opaque: an IDirectSound / IDirectSoundBuffer of the backend */

typedef struct {
    void* user;
    /* IDirectSoundBuffer */
    int32_t (*play)(void* user, PfDsObj* b, uint32_t r1, uint32_t r2, uint32_t flags);        /* slot 0x30 */
    int32_t (*stop)(void* user, PfDsObj* b);                                                  /* slot 0x48 */
    int32_t (*set_position)(void* user, PfDsObj* b, uint32_t pos);                            /* slot 0x34 */
    int32_t (*release)(void* user, PfDsObj* o);                                               /* slot 0x08 */
    int32_t (*lock)(void* user, PfDsObj* b, uint32_t offset, uint32_t bytes, void* p1, void* n1, void* p2,
                    void* n2, uint32_t flags);                                                 /* slot 0x2c */
    int32_t (*unlock)(void* user, PfDsObj* b, void* p1, uint32_t n1, void* p2, uint32_t n2);  /* slot 0x4c */
    int32_t (*get_status)(void* user, PfDsObj* b, uint32_t* status);                          /* slot 0x24 */
    int32_t (*set_volume)(void* user, PfDsObj* b, int32_t v);                                 /* slot 0x3c */
    int32_t (*get_volume)(void* user, PfDsObj* b, int32_t* v);                                /* slot 0x18 */
    int32_t (*set_pan)(void* user, PfDsObj* b, int32_t pan);                                  /* slot 0x40 */
    int32_t (*set_frequency)(void* user, PfDsObj* b, uint32_t hz);                            /* slot 0x44 */
    int32_t (*get_frequency)(void* user, PfDsObj* b, void* hz);                               /* slot 0x20 */
    /* IDirectSound */
    int32_t (*duplicate)(void* user, PfDsObj* ds, PfDsObj* src, PfDsObj** out);              /* slot 0x14 */
    /* game: 0x004bb9c4 sfx_start(eax, edx, ebx, ecx volume, stack pan, stack arg) */
    int32_t (*sfx_start)(void* user, uint32_t a, uint32_t b, uint32_t c, int32_t volume, int32_t pan, uint32_t arg);
} PfDsBackend;

typedef struct {
    PfDsObj* ds;              /* +0x00 */
    PfDsObj* primary;         /* +0x04 */
    uint32_t field8;          /* +0x08 */
    int32_t reverse_stereo;   /* +0x1c */
    const PfDsBackend* be;
    const float* note_freq;   /* 0x004e86d0: 26 floats C1.. (read from TM2.EXE), [24] = 130.81 (0x004e8730) */
    int32_t max_distance;     /* 0x00c85040 */
} PfDsDevice;

typedef struct {
    int32_t note;       /* +0x18 */
    int32_t bend;       /* +0x20 */
    uint32_t rate;      /* +0x24 */
    PfDsObj* buffer;    /* +0x40 */
} PfDsSample;

int32_t pf_ds_play(PfDsDevice* d, PfDsObj* b, uint32_t flags);
int32_t pf_ds_stop(PfDsDevice* d, PfDsObj* b);
int32_t pf_ds_release_buffer(PfDsDevice* d, PfDsObj* b);
int32_t pf_ds_release_device(PfDsDevice* d);
PfDsObj* pf_ds_primary(const PfDsDevice* d);
uint32_t pf_ds_field8(const PfDsDevice* d);
int32_t pf_ds_duplicate(PfDsDevice* d, PfDsObj* src, PfDsObj** out);
int32_t pf_ds_lock(PfDsDevice* d, PfDsObj* b, uint32_t bytes, void* p1, void* n1, void* p2, void* n2);
int32_t pf_ds_unlock(PfDsDevice* d, PfDsObj* b, void* p1, uint32_t n1, void* p2, uint32_t n2);
int32_t pf_ds_set_position(PfDsDevice* d, PfDsObj* b, uint32_t pos);
int32_t pf_ds_get_status(PfDsDevice* d, PfDsObj* b, uint32_t* status);
int32_t pf_ds_set_volume(PfDsDevice* d, PfDsObj* b, int32_t v);
int32_t pf_ds_get_volume(PfDsDevice* d, PfDsObj* b, int32_t* v);
int32_t pf_ds_set_pan(PfDsDevice* d, PfDsObj* b, int32_t pan);
int32_t pf_ds_set_frequency(PfDsDevice* d, PfDsObj* b, uint32_t hz);
int32_t pf_ds_get_frequency(PfDsDevice* d, PfDsObj* b, void* hz);

int32_t pf_ds_pan_value(int32_t pan);                 /* the 0x004ba76c curve, -10000..10000 */
int32_t pf_ds_sample_set_pan(PfDsDevice* d, const PfDsSample* s, int32_t pan);
int32_t pf_ds_pitch_value(const PfDsDevice* d, const PfDsSample* s, int32_t n);  /* the 0x004ba890 rate */
int32_t pf_ds_sample_set_pitch(PfDsDevice* d, const PfDsSample* s, int32_t n);
void pf_ds_sample_is_playing(PfDsDevice* d, const PfDsSample* s, uint32_t* out);
int32_t pf_ds_play_at(PfDsDevice* d, uint32_t a, uint32_t b, uint32_t c, int32_t volume, int32_t distance,
                      int32_t lateral, uint32_t arg);

#endif /* PF_DSOUND_H */
