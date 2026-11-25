/*
 * pf2_snd.c: TM2.EXE's small sound / CD audio (MCI) / movie wrappers (docs/PLATFORM.md Part C / D),
 * ported over Pf2Env (pf2_env.h). Emulator diff: tools/emu/check_pf2_snd.sh.
 *
 * Arguments are the original's registers in order (eax, edx, ebx, ecx) then its stack words.
 * CD player 0x00c850bc: +0x00 MCI device id, +0x04 / +0x08 from / to track, +0x18 open, +0x1c use
 * aux volume, +0x20 end of track callback. DirectSound device 0x00c850ec: +0x00 IDirectSound.
 * Sound bank (0x1c bytes): +0x00 program count, +0x04 programs (12 bytes each), +0x08 / +0x0c
 * tones, +0x10 / +0x14 samples. Program ("group"): +0x00 voice count, +0x04 voices (0x18 bytes
 * each), +0x08 voice id table. Voice: +0x10 sample count, +0x14 samples (0x44 bytes, +0x40
 * IDirectSoundBuffer).
 */
#include "pf2_env.h"

#define R32 pf2_r32
#define W32 pf2_w32
#define CD 0x00c850bcu
#define DSDEV 0x00c850ecu

#define I_mciSendCommandA 0x00c90614u
#define I_mciSendStringA 0x00c90618u
#define I_auxSetVolume 0x00c90600u

#define F_CD_COMMAND 0x004b8b40u     /* cd_command(eax player, edx command) */
#define F_CD_AUX_DEVICE 0x004b8eacu  /* cd_find_aux_device(eax player, edx 1) */
#define F_DSBUF_STOP 0x004b9898u     /* dsbuf_stop(eax device, edx buffer): pf_dsound.c */
#define F_BANK_LOAD 0x004bb1f0u
#define F_SFX_STOP_ALL 0x004bc5ccu
#define F_SFX_UPDATE 0x004bc4e4u
#define F_BASE_CTOR 0x004c9f04u
#define F_BASE_OPEN 0x004c9f00u
#define F_MUSIC_NOTIFY 0x004931fcu
#define F_DELETE_ARRAY 0x004a9ff8u   /* C++ array delete helper (eax array, edx destructor) */
#define F_FREE 0x004aa018u

/* 0x004b8990 cd_set_end_callback(eax cd, edx fn): +0x20 = fn; returns cd */
uint32_t pf2_cd_set_end_callback(Pf2Env* e, uint32_t cd, uint32_t fn) { W32(e, cd + 0x20, fn); return cd; }

/* 0x004b8a28 cd_set_tracks(eax cd, edx from, ebx to): not open -> 0; +8 = to, +4 = from; 1 */
uint32_t pf2_cd_set_tracks(Pf2Env* e, uint32_t cd, uint32_t from, uint32_t to) {
    if (R32(e, cd + 0x18) == 0) return 0;
    W32(e, cd + 8, to);
    W32(e, cd + 4, from);
    return 1;
}

/* 0x004b8ad4 cd_get_mode(eax cd, edx out): not open -> 0; MCI_STATUS_PARMS on the stack with
   only dwItem (+8) = MCI_STATUS_MODE (4) set (dwCallback and dwTrack are stack garbage);
   mciSendCommandA(id, MCI_STATUS 0x814, MCI_STATUS_ITEM 0x100, &parms); success: *out =
   dwReturn (+4). Returns the MCI error. */
uint32_t pf2_cd_get_mode(Pf2Env* e, uint32_t cd, uint32_t out) {
    if (R32(e, cd + 0x18) == 0) return 0;
    uint32_t p = e->scratch;
    W32(e, p + 8, 4);
    uint32_t r = pf2_call4(e, I_mciSendCommandA, R32(e, cd), 0x814, 0x100, p);
    if (r == 0) W32(e, out, R32(e, p + 4));
    return r;
}

/* 0x004b8f48 cd_set_volume(eax cd, edx volume): not open or not using the aux volume (+0x1c)
   -> 0; aux device 0x004b8eac(cd, 1) < 0 -> -1; auxSetVolume(device, volume) == 0 -> 1, else 0 */
uint32_t pf2_cd_set_volume(Pf2Env* e, uint32_t cd, uint32_t vol) {
    if (R32(e, cd + 0x18) == 0 || R32(e, cd + 0x1c) == 0) return 0;
    uint32_t dev = pf2_call2(e, F_CD_AUX_DEVICE, cd, 1);
    if ((int32_t)dev < 0) return 0xffffffffu;
    return pf2_call2(e, I_auxSetVolume, dev, vol) == 0;
}

/* 0x004b8fc8 cd_set_use_aux_volume(eax cd, edx on): +0x1c = on; returns cd */
uint32_t pf2_cd_set_use_aux_volume(Pf2Env* e, uint32_t cd, uint32_t on) { W32(e, cd + 0x1c, on); return cd; }

/* 0x004b9140 / 0x004b9268 / 0x004b9270: jmp thunks to the sound bank loader 0x004bb1f0, the voice
   list's stop all 0x004bc5cc and update 0x004bc4e4 (every register passed on) */
uint32_t pf2_sound_bank_load_thunk(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c)
{ return pf2_call4(e, F_BANK_LOAD, a, d, b, c); }
uint32_t pf2_sfx_stop_all_thunk(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c)
{ return pf2_call4(e, F_SFX_STOP_ALL, a, d, b, c); }
uint32_t pf2_sfx_update_thunk(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c)
{ return pf2_call4(e, F_SFX_UPDATE, a, d, b, c); }

uint32_t pf2_sound_get_level(Pf2Env* e) { return R32(e, 0x00c850e0u); }   /* 0x004b9150 */

/* 0x004bb738 bank_program(eax bank, edx index): index < count (signed; no lower bound) ->
   programs + 12 * index, else 0 */
uint32_t pf2_bank_program(Pf2Env* e, uint32_t bank, uint32_t i) {
    if ((int32_t)i >= (int32_t)R32(e, bank)) return 0;
    return i * 12 + R32(e, bank + 4);
}

/* 0x004baf9c group_find_voice(eax group, edx id): the voice whose id table entry equals id
   (count re-read each step), else 0 */
uint32_t pf2_group_find_voice(Pf2Env* e, uint32_t g, uint32_t id) {
    if ((int32_t)R32(e, g) <= 0) return 0;
    uint32_t p = R32(e, g + 8);
    for (uint32_t i = 0;; i++, p += 4) {
        if (id == R32(e, p)) return i * 24 + R32(e, g + 4);
        if ((int32_t)(i + 1) >= (int32_t)R32(e, g)) return 0;
    }
}

/* 0x004baac0 voice_stop(eax voice, (edx)): dsbuf_stop(0x00c850ec, sample +0x40) for each sample;
   returns the last result. ORIGINAL BUG: with no samples it returns edx as the caller left it
   (edx_in here). */
uint32_t pf2_voice_stop(Pf2Env* e, uint32_t v, uint32_t edx_in) {
    uint32_t r = edx_in;
    if ((int32_t)R32(e, v + 0x10) > 0) {
        uint32_t i = 0;
        do {
            r = pf2_call2(e, F_DSBUF_STOP, DSDEV, R32(e, R32(e, v + 0x14) + 0x44 * i + 0x40));
            i++;
        } while ((int32_t)i < (int32_t)R32(e, v + 0x10));
    }
    return r;
}

/* 0x004bafdc group_stop_all(eax group): dsbuf_stop on every sample of every voice (counts
   re-read); returns 0 */
uint32_t pf2_group_stop_all(Pf2Env* e, uint32_t g) {
    for (uint32_t i = 0; (int32_t)i < (int32_t)R32(e, g); i++) {
        uint32_t v = R32(e, g + 4) + 0x18 * i;
        for (uint32_t j = 0; (int32_t)j < (int32_t)R32(e, v + 0x10); j++)
            pf2_call2(e, F_DSBUF_STOP, DSDEV, R32(e, R32(e, v + 0x14) + 0x44 * j + 0x40));
    }
    return 0;
}

/* 0x004b9240 sound_stop(eax bank, edx program, ebx id): id -1: group_stop_all(program's group);
   else the voice with that id: voice_stop (none: 0). The edx voice_stop would return with no
   samples is group_find_voice's leftover edx (not modelled: 0 passed). */
uint32_t pf2_sound_stop(Pf2Env* e, uint32_t bank, uint32_t prog, uint32_t id) {
    uint32_t g = pf2_bank_program(e, bank, prog);
    if (id == 0xffffffffu) return pf2_group_stop_all(e, g);
    uint32_t v = pf2_group_find_voice(e, g, id);
    if (!v) return 0;
    return pf2_voice_stop(e, v, 0);
}

/* 0x004b92b0 music_cd_ctor(eax obj): base ctor 0x004c9f04, then +8 = 0x004e62ac (the CD music
   vtable, 0x004e62ac is a game address kept as a value); returns the base ctor's eax */
uint32_t pf2_music_cd_ctor(Pf2Env* e, uint32_t obj) {
    uint32_t r = pf2_call1(e, F_BASE_CTOR, obj);
    W32(e, r + 8, 0x004e62acu);
    return r;
}

/* 0x004b92c0 music_cd_open(eax obj): 0x004c9f00(obj), cd_command(CD, 0 open), current track
   0x00c850e8 = 2; returns cd_command's result */
uint32_t pf2_music_cd_open(Pf2Env* e, uint32_t obj) {
    pf2_call1(e, F_BASE_OPEN, obj);
    uint32_t r = pf2_call2(e, F_CD_COMMAND, CD, 0);
    W32(e, 0x00c850e8u, 2);
    return r;
}

/* 0x004b92e0 music_cd_play_track(eax obj, edx track): -1: nothing. Else cd_set_tracks(CD, track,
   track + 1), cd_command(CD, 1 play), 0x00c850e8 = track, 0x004931fc(cd_command's result in eax),
   obj +4 = track. Returns track. */
uint32_t pf2_music_cd_play_track(Pf2Env* e, uint32_t obj, uint32_t track) {
    if (track == 0xffffffffu) return track;
    pf2_cd_set_tracks(e, CD, track, track + 1);
    uint32_t r = pf2_call2(e, F_CD_COMMAND, CD, 1);
    W32(e, 0x00c850e8u, track);
    pf2_call1(e, F_MUSIC_NOTIFY, r);
    W32(e, obj + 4, track);
    return track;
}

/* 0x004b931c / 0x004b9334 music_cd_pause / resume(eax obj, edx): cd_command(CD, 3 / 2); returns edx */
uint32_t pf2_music_cd_pause(Pf2Env* e, uint32_t obj, uint32_t v) { (void)obj; pf2_call2(e, F_CD_COMMAND, CD, 3); return v; }
uint32_t pf2_music_cd_resume(Pf2Env* e, uint32_t obj, uint32_t v) { (void)obj; pf2_call2(e, F_CD_COMMAND, CD, 2); return v; }

/* 0x004b94c4 dserr_filter(eax, edx hr): 0 for DSERR_BUFFERLOST 0x88780096 (and 0), else hr
   (the range compares around it select nothing else) */
uint32_t pf2_dserr_filter(Pf2Env* e, uint32_t a, uint32_t hr) {
    (void)e; (void)a;
    return hr == 0x88780096u ? 0 : hr;
}

/* 0x004b97b0 dsound_create_buffer(eax device, edx out, ebx bytes, ecx WAVEFORMATEX*, stack flags;
   ret 4): no IDirectSound -> E_FAIL; DSBUFFERDESC {0x14, flags, bytes, 0, wfx};
   IDirectSound::CreateSoundBuffer(&desc, out, NULL) */
uint32_t pf2_dsound_create_buffer(Pf2Env* e, uint32_t dev, uint32_t out, uint32_t bytes, uint32_t wfx, uint32_t flags) {
    if (R32(e, dev) == 0) return 0x80004005u;
    uint32_t s = e->scratch;
    pf2_zero(e, s, 0x14);
    W32(e, s + 8, bytes);
    W32(e, s + 0x10, wfx);
    W32(e, s, 0x14);
    W32(e, s + 4, flags);
    return PF2_C3(e, PF2_IDS, 0xc, R32(e, dev), s, out, 0);
}

/* 0x004ba39c movie_notify_done(eax): 0x00c85110 (playing) = 0; returns eax */
uint32_t pf2_movie_notify_done(Pf2Env* e, uint32_t a) { W32(e, 0x00c85110u, 0); return a; }

/* 0x004ba67c sample_stop(eax sample): dsbuf_stop(0x00c850ec, sample +0x40) */
uint32_t pf2_sample_stop(Pf2Env* e, uint32_t s) { return pf2_call2(e, F_DSBUF_STOP, DSDEV, R32(e, s + 0x40)); }

/* 0x004b9bf0 movie_init: mciSendStringA(0x004e3770 "open avivideo", NULL, 0, NULL) (the string is
   a game address passed as is); 0x00c8511c = (result == 0); returns the result */
uint32_t pf2_movie_init(Pf2Env* e) {
    uint32_t r = pf2_call4(e, I_mciSendStringA, 0x004e3770u, 0, 0, 0);
    W32(e, 0x00c8511cu, r == 0);
    return r;
}

/* 0x004b9c7c movie_set_double_size(eax on): 0x00c85118 = on; returns on */
uint32_t pf2_movie_set_double_size(Pf2Env* e, uint32_t on) { W32(e, 0x00c85118u, on); return on; }

/* 0x004bb1b0 sound_bank_construct(eax bank): the seven dwords zeroed; returns bank */
uint32_t pf2_sound_bank_construct(Pf2Env* e, uint32_t b) {
    for (uint32_t o = 4; o <= 0x18; o += 4) W32(e, b + o, 0);
    W32(e, b, 0);
    return b;
}

/* 0x004bb6c4 sound_bank_free(eax bank): programs +4: array delete 0x004a9ff8(+4, dtor 0x004e6318)
   then free 0x004aa018(its result), +4 = +0 = 0; tones +0x0c: free, +0x0c = +8 = 0; samples
   +0x14: array delete (dtor 0x004e62f0) + free, +0x14 = +0x10 = 0. Returns 0. */
uint32_t pf2_sound_bank_free(Pf2Env* e, uint32_t b) {
    if (R32(e, b + 4)) {
        pf2_call1(e, F_FREE, pf2_call2(e, F_DELETE_ARRAY, R32(e, b + 4), 0x004e6318u));
        W32(e, b + 4, 0);
        W32(e, b, 0);
    }
    if (R32(e, b + 0xc)) {
        pf2_call1(e, F_FREE, R32(e, b + 0xc));
        W32(e, b + 0xc, 0);
        W32(e, b + 8, 0);
    }
    if (R32(e, b + 0x14)) {
        pf2_call1(e, F_FREE, pf2_call2(e, F_DELETE_ARRAY, R32(e, b + 0x14), 0x004e62f0u));
        W32(e, b + 0x14, 0);
        W32(e, b + 0x10, 0);
    }
    return 0;
}

/* 0x004bb1e4 sound_bank_destruct(eax bank): sound_bank_free; returns bank */
uint32_t pf2_sound_bank_destruct(Pf2Env* e, uint32_t b) { pf2_sound_bank_free(e, b); return b; }
