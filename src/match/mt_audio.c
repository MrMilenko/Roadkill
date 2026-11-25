/*
 * mt_audio.c: sound, music, force feedback and display mode wrappers (see mt_audio.h).
 */
#include "mt_audio.h"

#include <string.h>

#define MUSIC_PLAYER 0x00c850bcu

static uint32_t call(MtCallFn f, const MtAudio* a, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx) {
    return f(a->user, eax, edx, ebx, ecx, 0, 0);
}

void mt_display_mode(const MtAudio* a, int32_t mode, uint32_t eax) {
    (void)eax;
    call(a->display_save, a, 0, 0, 0, 0);
    *a->disp_95c = mode;
    call(a->display_apply, a, 0, 0, 0, 0);
    if (*a->disp_958) *a->disp_950 = 1;
}

void mt_display_text_off(const MtAudio* a, uint32_t eax) {
    *a->disp_950 = 1;
    *a->disp_958 = 0;
    call(a->text_set, a, eax, 0x004e21a8u, 0, 0);
}

void mt_display_text_on(const MtAudio* a, uint32_t eax) {
    *a->disp_950 = 1;
    *a->disp_958 = -1;
    call(a->text_set, a, eax, 0x004e21a9u, 0xffffffffu, 0);
}

void mt_text_escape_amp(const MtAudio* a, uint32_t str, uint8_t flag) {
    if (!str) return;
    uint8_t* s = a->resolve(a->user, str);
    uint8_t buf[0x80];
    int32_t len = (int32_t)strlen((const char*)s), k = 0;
    for (int32_t i = 0; i < len; i++) {
        uint8_t c = s[i];
        if (c == '&') {
            if (!flag) continue;
            buf[k++] = c;
        }
        buf[k++] = s[i];
    }
    buf[k] = 0;
    memcpy(s, buf, (size_t)k + 1);          /* sprintf(str, "%s", buf) */
}

int32_t mt_clamp_99(int32_t v) { return v >= 100 ? 99 : v; }

static uint32_t snd5(MtCallFn f, const MtAudio* a, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t s) {
    uint32_t st[2] = {p4, s};
    return f(a->user, *a->sound, p1, p2, p3, st, 2);
}
uint32_t mt_rumble_play(const MtAudio* a, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t s) {
    return snd5(a->snd_effect, a, p1, p2, p3, p4, s);
}
uint32_t mt_sound_loop(const MtAudio* a, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t s) {
    return snd5(a->snd_loop, a, p1, p2, p3, p4, s);
}
uint32_t mt_rumble_stop(const MtAudio* a, uint32_t p1, uint32_t p2) { return call(a->snd_stop_effect, a, *a->sound, p1, p2, 0); }
uint32_t mt_sound_loop_stop(const MtAudio* a, uint32_t p1, uint32_t p2) { return call(a->snd_loop_stop, a, *a->sound, p1, p2, 0); }
uint32_t mt_sound_stop_instance(const MtAudio* a, uint32_t p1, uint32_t p2) { return call(a->snd_stop, a, *a->sound, p1, p2, 0); }
uint32_t mt_sound_stop(const MtAudio* a, uint32_t p1) { return call(a->snd_stop, a, *a->sound, p1, 0xffffffffu, 0); }

uint32_t mt_sound_play(const MtAudio* a, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4) {
    uint32_t st[3] = {p4, 0xffffffffu, 0};
    return a->snd_method0(a->user, *a->sound, p1, p2, p3, st, 3);
}
uint32_t mt_sound_play_variant(const MtAudio* a, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t s) {
    uint32_t st[3] = {p4, s, 0};
    return a->snd_method0(a->user, *a->sound, p1, p2, p3, st, 3);
}
uint32_t mt_sound_flush(const MtAudio* a) { return call(a->snd_flush, a, *a->sound, 0, 0, 0); }
uint32_t mt_sound_method1(const MtAudio* a) { return call(a->snd_method1, a, *a->sound, 0, 0, 0); }
uint32_t mt_music_call0(const MtAudio* a, uint32_t p1) { return call(a->music_call, a, p1, 0, 0, 0); }
uint32_t mt_music_call(const MtAudio* a, uint32_t p1, uint32_t p2) { return call(a->music_call, a, p1, p2, 0, 0); }
uint32_t mt_music_set_20(const MtAudio* a, uint32_t p1) { return call(a->music_set_20, a, MUSIC_PLAYER, p1, 0, 0); }

static void volume_step(const MtAudio* a, int32_t v) {
    *a->volume = v < *a->volume_floor ? *a->volume_floor : v;
    int32_t w = (int32_t)((uint32_t)v * 0xffffu) / 100;
    if (w > 0xffff) w = 0xffff;
    call(a->music_volume_set, a, MUSIC_PLAYER, (uint32_t)w + ((uint32_t)w << 16), 0, 0);
}

void mt_music_alternate_track(const MtAudio* a) {
    *a->music_track = (*a->music_track == 10) + 10;
    call(a->track_set, a, *a->music_obj, (uint32_t)*a->music_track, 0, 0);
    volume_step(a, *a->music_volume);
}

void mt_music_fade_out(const MtAudio* a, int32_t v) {
    while (v > 0) {
        v--;
        volume_step(a, v);
        /* 800 sprintf calls into a stack buffer as a delay; dest (the original's stack) is passed as 0 */
        for (int k = 0; k < 800; k++) {
            uint32_t st[2] = {0, 0x004e21b0u};
            a->sprintf_s(a->user, 0, 0, 0, 0, st, 2);
        }
    }
}

uint32_t mt_get_c8500c(const MtAudio* a) { return *a->v_c8500c; }
uint32_t mt_get_c85058(const MtAudio* a) { return *a->v_c85058; }
int32_t mt_get_music_volume(const MtAudio* a) { return *a->music_volume; }
void mt_flag81_set(const MtAudio* a) { a->flags[1] = 1; }
void mt_flag81_clear(const MtAudio* a) { a->flags[1] = 0; }
uint8_t mt_flag84_get(const MtAudio* a) { return a->flags[4]; }
uint8_t mt_flag85_get(const MtAudio* a) { return a->flags[5]; }
void mt_flag82_set(const MtAudio* a, uint8_t v) { a->flags[2] = v; }
void mt_flag84_set(const MtAudio* a, uint8_t v) { a->flags[4] = v; }
void mt_flag86_set(const MtAudio* a, uint8_t v) { a->flags[6] = v; }
void mt_set_621168(const MtAudio* a, uint32_t v) { *a->v_621168 = v; }
void mt_flag80_set(const MtAudio* a, uint8_t v) { a->flags[0] = v; }
uint8_t mt_flag80_get(const MtAudio* a) { return a->flags[0]; }
void mt_countdown_start(const MtAudio* a) { *a->countdown = 6; }
int32_t mt_countdown_step(const MtAudio* a) {
    int32_t v = *a->countdown;
    v = v > 0 ? v - 1 : 0;
    *a->countdown = v;
    return v;
}
void mt_nop_46f780(void) {}
void mt_nop_46f828(void) {}
