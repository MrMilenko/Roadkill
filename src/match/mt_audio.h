/*
 * mt_audio.h: sound, music, force feedback and display mode wrappers in TM2.EXE
 * 0x0046f51c..0x0046feb0, ported from the PC machine code.
 *
 * Most of these hand their arguments on to the sound driver object (pointer at 0x00626010) or
 * the music player (0x00c850bc) with the registers shuffled; the callees are outside this code
 * and are callbacks of one shape: fn(user, eax, edx, ebx, ecx, stack arguments). The handles
 * passed are the original's pointers.
 *
 * Globals (pointers in MtAudio):
 *   0x00626010 u32 sound driver (its +0x1c: a table of methods), 0x00626014 u32 music object
 *   (its +8: a vtable, [vt+4](obj, track)), 0x004e7cc4 i32 music track (10 / 11),
 *   0x004e7cc0 i32 music volume (0..100), 0x00c8507c i32 volume floor, 0x00c85098 i32 volume,
 *   0x00c8500c, 0x00c85058 u32, 0x0058a950 / 0x0058a958 / 0x0058a95c i32 display state,
 *   0x00621168 u32, 0x0062116c i32 countdown, 0x00621180..0x00621186 u8 flags
 *
 * Verified against the original under emulation (tools/emu/check_match_audio.sh).
 */
#ifndef MT_AUDIO_H
#define MT_AUDIO_H

#include <stdint.h>

typedef uint32_t (*MtCallFn)(void* user, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx,
                             const uint32_t* stack, int nstack);

typedef struct {
    uint32_t* sound;            /* 0x00626010 */
    uint32_t* music_obj;        /* 0x00626014 */
    int32_t* music_track;       /* 0x004e7cc4 */
    int32_t* music_volume;      /* 0x004e7cc0 */
    int32_t* volume_floor;      /* 0x00c8507c */
    int32_t* volume;            /* 0x00c85098 */
    uint32_t* v_c8500c;         /* 0x00c8500c */
    uint32_t* v_c85058;         /* 0x00c85058 */
    int32_t* disp_950;          /* 0x0058a950 */
    int32_t* disp_958;          /* 0x0058a958 */
    int32_t* disp_95c;          /* 0x0058a95c */
    uint32_t* v_621168;         /* 0x00621168 */
    int32_t* countdown;         /* 0x0062116c */
    uint8_t* flags;             /* 0x00621180 .. 0x00621186 */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    MtCallFn display_save;      /* 0x0046cd70 () */
    MtCallFn display_apply;     /* 0x0046ecb0 () */
    MtCallFn text_set;          /* 0x004a6f08 (eax, edx, ebx) */
    MtCallFn snd_effect;        /* 0x00472f8c (snd, a, b, c, stack c', s) */
    MtCallFn snd_stop_effect;   /* 0x0047304c (snd, a, b) */
    MtCallFn snd_loop;          /* 0x004730c8 (snd, a, b, c, stack c', s) */
    MtCallFn snd_loop_stop;     /* 0x00473104 (snd, a, b) */
    MtCallFn snd_stop;          /* 0x00472f74 (snd, a, b) */
    MtCallFn snd_flush;         /* 0x0047314c (snd) */
    MtCallFn snd_method0;       /* the sound driver's method [[snd +0x1c] + 0] */
    MtCallFn snd_method1;       /* the sound driver's method [[snd +0x1c] + 4] */
    MtCallFn music_call;        /* 0x004b8010 (eax, edx) */
    MtCallFn music_set_20;      /* 0x004b8990 (obj, v) */
    MtCallFn music_volume_set;  /* 0x004b8f48 (obj, stereo volume) */
    MtCallFn track_set;         /* music object vtable[1] (obj, track) */
    MtCallFn sprintf_s;         /* 0x004b27e6 (cdecl: dest, format, arg) */
    void* user;
} MtAudio;

/* 0x0046f51c / 0x0046f544 / 0x0046f570 / 0x0046f59c / 0x0046f5c8 / 0x0046f5f4: display mode n
 * (1..6): display_save, 0x0058a95c = n, display_apply, 0x0058a950 = 1 when 0x0058a958 */
void mt_display_mode(const MtAudio* a, int32_t mode, uint32_t eax);
/* 0x0046f620 / 0x0046f644: 0x0058a950 = 1, 0x0058a958 = 0 / -1, text_set(eax, 0x004e21a8 /
 * 0x004e21a9, 0 / -1) */
void mt_display_text_off(const MtAudio* a, uint32_t eax);
void mt_display_text_on(const MtAudio* a, uint32_t eax);
/* 0x0046f66c: in place: '&' doubled (flag != 0) or dropped (flag == 0), through
 * sprintf(str, "%s" (0x004e21aa), buffer). str NULL: nothing. ORIGINAL BUG (not reproduced): the
 * buffer is 0x80 bytes on the stack with no length check. */
void mt_text_escape_amp(const MtAudio* a, uint32_t str, uint8_t flag);
/* 0x0046f830: min(v, 99) (two stack arguments ignored) */
int32_t mt_clamp_99(int32_t v);
/* 0x0046f840 rumble_play / 0x0046f878: (a, b, c, d, stack s) -> snd_effect / snd_loop(snd, a, b, c, d, s) */
uint32_t mt_rumble_play(const MtAudio* a, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t s);
uint32_t mt_sound_loop(const MtAudio* a, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t s);
/* 0x0046f860 rumble_stop / 0x0046f898 / 0x0046f934: (a, b) -> snd_stop_effect / snd_loop_stop / snd_stop(snd, a, b) */
uint32_t mt_rumble_stop(const MtAudio* a, uint32_t p1, uint32_t p2);
uint32_t mt_sound_loop_stop(const MtAudio* a, uint32_t p1, uint32_t p2);
uint32_t mt_sound_stop_instance(const MtAudio* a, uint32_t p1, uint32_t p2);
/* 0x0046f918: snd_stop(snd, a, -1) */
uint32_t mt_sound_stop(const MtAudio* a, uint32_t p1);
/* 0x0046f8b0: method 0 (snd, a, b, c, stack c, -1, 0); 0x0046f8cc: method 0 (snd, a, b, c, stack c, s, 0) */
uint32_t mt_sound_play(const MtAudio* a, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4);
uint32_t mt_sound_play_variant(const MtAudio* a, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t s);
/* 0x0046f94c: snd_flush(snd); 0x0046f958: method 1 (snd) */
uint32_t mt_sound_flush(const MtAudio* a);
uint32_t mt_sound_method1(const MtAudio* a);
/* 0x0046f974: music_call(a, 0); 0x0046f980: music_call(a, b) */
uint32_t mt_music_call0(const MtAudio* a, uint32_t p1);
uint32_t mt_music_call(const MtAudio* a, uint32_t p1, uint32_t p2);
/* 0x0046fcb0: music_set_20(0x00c850bc, a) */
uint32_t mt_music_set_20(const MtAudio* a, uint32_t p1);
/* 0x0046fcc0 audioAlternateMusicTrack: track 10 <-> 11, track_set, then the volume step of
 * 0x004e7cc0. 0x0046fdf8 audioFadeOutMusic(v): for v-1 down to 0: the volume step, then 800
 * sprintf(buffer, 0x004e21b0) calls as a delay (delay callback: sprintf_s). Volume step:
 * 0x00c85098 = max(v, floor), w = min(v * 0xffff / 100, 0xffff) (32 bit wrap, signed divide),
 * music_volume_set(0x00c850bc, w + (w << 16)). */
void mt_music_alternate_track(const MtAudio* a);
void mt_music_fade_out(const MtAudio* a, int32_t v);
/* getters / setters */
uint32_t mt_get_c8500c(const MtAudio* a);       /* 0x0046f988 */
uint32_t mt_get_c85058(const MtAudio* a);       /* 0x0046f990 */
int32_t mt_get_music_volume(const MtAudio* a);  /* 0x0046f9dc */
void mt_flag81_set(const MtAudio* a);           /* 0x0046fc0c: 0x00621181 = 1 */
void mt_flag81_clear(const MtAudio* a);         /* 0x0046fc14: 0x00621181 = 0 */
uint8_t mt_flag84_get(const MtAudio* a);        /* 0x0046fc30 */
uint8_t mt_flag85_get(const MtAudio* a);        /* 0x0046fc38 */
void mt_flag82_set(const MtAudio* a, uint8_t v); /* 0x0046fc58 */
void mt_flag84_set(const MtAudio* a, uint8_t v); /* 0x0046fc68 */
void mt_flag86_set(const MtAudio* a, uint8_t v); /* 0x0046fc78 */
void mt_set_621168(const MtAudio* a, uint32_t v); /* 0x0046fc90 */
void mt_flag80_set(const MtAudio* a, uint8_t v); /* 0x0046fca0 */
uint8_t mt_flag80_get(const MtAudio* a);        /* 0x0046fca8 */
void mt_countdown_start(const MtAudio* a);      /* 0x0046fea4: 6 */
int32_t mt_countdown_step(const MtAudio* a);    /* 0x0046feb0: max(v - 1, 0), stored and returned */
void mt_nop_46f780(void);                       /* 0x0046f780 */
void mt_nop_46f828(void);                       /* 0x0046f828 */

#endif /* MT_AUDIO_H */
