/*
 * bind_match_audio.c: the sound / music / force feedback / display mode wrappers 0x0046f51c..
 * 0x0046feb0 (src/match/mt_audio.c), called the way tools/emu/diff_match_audio.py called them.
 */
#include "rt_env.h"
#include "match/mt_audio.h"

/* an MtCallFn callback: the original address with EAX, EDX, EBX, ECX and the stack words */
static uint32_t acall(uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, const uint32_t* st, int k)
{
    uint32_t a[4 + 8] = {eax, edx, ebx, ecx};
    if (k > 8) k = 8;
    for (int i = 0; i < k; i++) a[4 + i] = st[i];
    return rt_call(fn, a, 4 + k);
}
#define ACB(nm, fnexpr) \
    static uint32_t nm(void* u, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, const uint32_t* st, int k) \
    { (void)u; return acall((fnexpr), eax, edx, ebx, ecx, st, k); }

ACB(cb_display_save, 0x0046cd70u)
ACB(cb_display_apply, 0x0046ecb0u)
ACB(cb_text_set, 0x004a6f08u)
ACB(cb_snd_effect, 0x00472f8cu)
ACB(cb_snd_stop_effect, 0x0047304cu)
ACB(cb_snd_loop, 0x004730c8u)
ACB(cb_snd_loop_stop, 0x00473104u)
ACB(cb_snd_stop, 0x00472f74u)
ACB(cb_snd_flush, 0x0047314cu)
/* the sound driver's method table [snd +0x1c] and the music object's vtable [obj +8] */
ACB(cb_snd_method0, rt_r32(rt_r32(rt_r32(0x00626010u) + 0x1cu) + 0u))
ACB(cb_snd_method1, rt_r32(rt_r32(rt_r32(0x00626010u) + 0x1cu) + 4u))
ACB(cb_music_call, 0x004b8010u)
ACB(cb_music_set_20, 0x004b8990u)
ACB(cb_music_volume_set, 0x004b8f48u)
ACB(cb_track_set, rt_r32(rt_r32(rt_r32(0x00626014u) + 8u) + 4u))
/* sprintf 0x004b27e6 (cdecl: dest, format, ...; no register arguments, the stack words from a[0]).
 * The fade out's delay passes dest 0, the stand in for the original's 0x80 byte stack buffer under
 * the emulator: here a real frame buffer. */
static uint32_t cb_sprintf_s(void* u, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx, const uint32_t* st, int k)
{
    (void)u; (void)eax; (void)edx; (void)ebx; (void)ecx;
    if (k >= 1 && st[0] == 0) {
        uint32_t s2[8];
        if (k > 8) k = 8;
        for (int i = 0; i < k; i++) s2[i] = st[i];
        s2[0] = rt_frame_push(0x80u);
        uint32_t r = rt_call(0x004b27e6u, s2, k);
        rt_frame_pop(0x80u);
        return r;
    }
    return rt_call(0x004b27e6u, st, k);
}

static uint8_t* a_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }

static const MtAudio* env(void)
{
    static MtAudio e;
    static int done;
    if (!done) {
        e.sound = (uint32_t*)rt_ptr(0x00626010u);
        e.music_obj = (uint32_t*)rt_ptr(0x00626014u);
        e.music_track = (int32_t*)rt_ptr(0x004e7cc4u);
        e.music_volume = (int32_t*)rt_ptr(0x004e7cc0u);
        e.volume_floor = (int32_t*)rt_ptr(0x00c8507cu);
        e.volume = (int32_t*)rt_ptr(0x00c85098u);
        e.v_c8500c = (uint32_t*)rt_ptr(0x00c8500cu);
        e.v_c85058 = (uint32_t*)rt_ptr(0x00c85058u);
        e.disp_950 = (int32_t*)rt_ptr(0x0058a950u);
        e.disp_958 = (int32_t*)rt_ptr(0x0058a958u);
        e.disp_95c = (int32_t*)rt_ptr(0x0058a95cu);
        e.v_621168 = (uint32_t*)rt_ptr(0x00621168u);
        e.countdown = (int32_t*)rt_ptr(0x0062116cu);
        e.flags = rt_ptr(0x00621180u);
        e.resolve = a_resolve;
        e.display_save = cb_display_save;
        e.display_apply = cb_display_apply;
        e.text_set = cb_text_set;
        e.snd_effect = cb_snd_effect;
        e.snd_stop_effect = cb_snd_stop_effect;
        e.snd_loop = cb_snd_loop;
        e.snd_loop_stop = cb_snd_loop_stop;
        e.snd_stop = cb_snd_stop;
        e.snd_flush = cb_snd_flush;
        e.snd_method0 = cb_snd_method0;
        e.snd_method1 = cb_snd_method1;
        e.music_call = cb_music_call;
        e.music_set_20 = cb_music_set_20;
        e.music_volume_set = cb_music_volume_set;
        e.track_set = cb_track_set;
        e.sprintf_s = cb_sprintf_s;
        done = 1;
    }
    return &e;
}

#define R(i) rt_arg(a, n, i)
#define S(i) rt_arg(a, n, 4 + (i))

#define DISP(nm, mode) static uint32_t nm(const uint32_t* a, int n) { mt_display_mode(env(), mode, R(0)); return 0; }
DISP(b_display_mode1, 1)
DISP(b_display_mode2, 2)
DISP(b_display_mode3, 3)
DISP(b_display_mode4, 4)
DISP(b_display_mode5, 5)
DISP(b_display_mode6, 6)
static uint32_t b_display_text_off(const uint32_t* a, int n) { mt_display_text_off(env(), R(0)); return 0; }
static uint32_t b_display_text_on(const uint32_t* a, int n) { mt_display_text_on(env(), R(0)); return 0; }
static uint32_t b_text_escape_amp(const uint32_t* a, int n) { mt_text_escape_amp(env(), R(0), (uint8_t)R(1)); return 0; }
static uint32_t b_clamp_99(const uint32_t* a, int n) { return (uint32_t)mt_clamp_99((int32_t)R(0)); }
static uint32_t b_rumble_play(const uint32_t* a, int n) { return mt_rumble_play(env(), R(0), R(1), R(2), R(3), S(0)); }
static uint32_t b_sound_loop(const uint32_t* a, int n) { return mt_sound_loop(env(), R(0), R(1), R(2), R(3), S(0)); }
static uint32_t b_rumble_stop(const uint32_t* a, int n) { return mt_rumble_stop(env(), R(0), R(1)); }
static uint32_t b_sound_loop_stop(const uint32_t* a, int n) { return mt_sound_loop_stop(env(), R(0), R(1)); }
static uint32_t b_sound_stop_instance(const uint32_t* a, int n) { return mt_sound_stop_instance(env(), R(0), R(1)); }
static uint32_t b_sound_stop(const uint32_t* a, int n) { return mt_sound_stop(env(), R(0)); }
static uint32_t b_sound_play(const uint32_t* a, int n) { return mt_sound_play(env(), R(0), R(1), R(2), R(3)); }
static uint32_t b_sound_play_variant(const uint32_t* a, int n) { return mt_sound_play_variant(env(), R(0), R(1), R(2), R(3), S(0)); }
static uint32_t b_sound_flush(const uint32_t* a, int n) { (void)a; (void)n; return mt_sound_flush(env()); }
static uint32_t b_sound_method1(const uint32_t* a, int n) { (void)a; (void)n; return mt_sound_method1(env()); }
static uint32_t b_music_call0(const uint32_t* a, int n) { return mt_music_call0(env(), R(0)); }
static uint32_t b_music_call(const uint32_t* a, int n) { return mt_music_call(env(), R(0), R(1)); }
static uint32_t b_music_set_20(const uint32_t* a, int n) { return mt_music_set_20(env(), R(0)); }
static uint32_t b_music_alternate_track(const uint32_t* a, int n) { (void)a; (void)n; mt_music_alternate_track(env()); return 0; }
static uint32_t b_music_fade_out(const uint32_t* a, int n) { mt_music_fade_out(env(), (int32_t)R(0)); return 0; }
static uint32_t b_get_c8500c(const uint32_t* a, int n) { (void)a; (void)n; return mt_get_c8500c(env()); }
static uint32_t b_get_c85058(const uint32_t* a, int n) { (void)a; (void)n; return mt_get_c85058(env()); }
static uint32_t b_get_music_volume(const uint32_t* a, int n) { (void)a; (void)n; return (uint32_t)mt_get_music_volume(env()); }
static uint32_t b_flag81_set(const uint32_t* a, int n) { (void)a; (void)n; mt_flag81_set(env()); return 0; }
static uint32_t b_flag81_clear(const uint32_t* a, int n) { (void)a; (void)n; mt_flag81_clear(env()); return 0; }
static uint32_t b_flag84_get(const uint32_t* a, int n) { (void)a; (void)n; return mt_flag84_get(env()); }
static uint32_t b_flag85_get(const uint32_t* a, int n) { (void)a; (void)n; return mt_flag85_get(env()); }
static uint32_t b_flag82_set(const uint32_t* a, int n) { mt_flag82_set(env(), (uint8_t)R(0)); return 0; }
static uint32_t b_flag84_set(const uint32_t* a, int n) { mt_flag84_set(env(), (uint8_t)R(0)); return 0; }
static uint32_t b_flag86_set(const uint32_t* a, int n) { mt_flag86_set(env(), (uint8_t)R(0)); return 0; }
static uint32_t b_set_621168(const uint32_t* a, int n) { mt_set_621168(env(), R(0)); return 0; }
static uint32_t b_flag80_set(const uint32_t* a, int n) { mt_flag80_set(env(), (uint8_t)R(0)); return 0; }
static uint32_t b_flag80_get(const uint32_t* a, int n) { (void)a; (void)n; return mt_flag80_get(env()); }
static uint32_t b_countdown_start(const uint32_t* a, int n) { (void)a; (void)n; mt_countdown_start(env()); return 0; }
static uint32_t b_countdown_step(const uint32_t* a, int n) { (void)a; (void)n; return (uint32_t)mt_countdown_step(env()); }
static uint32_t b_nop_46f780(const uint32_t* a, int n) { (void)a; (void)n; mt_nop_46f780(); return 0; }
static uint32_t b_nop_46f828(const uint32_t* a, int n) { (void)a; (void)n; mt_nop_46f828(); return 0; }

void rt_bind_match_audio(void)
{
    rt_bind(0x0046f51cu, b_display_mode1, "display_mode1");
    rt_bind(0x0046f544u, b_display_mode2, "display_mode2");
    rt_bind(0x0046f570u, b_display_mode3, "display_mode3");
    rt_bind(0x0046f59cu, b_display_mode4, "display_mode4");
    rt_bind(0x0046f5c8u, b_display_mode5, "display_mode5");
    rt_bind(0x0046f5f4u, b_display_mode6, "display_mode6");
    rt_bind(0x0046f620u, b_display_text_off, "display_text_off");
    rt_bind(0x0046f644u, b_display_text_on, "display_text_on");
    rt_bind(0x0046f66cu, b_text_escape_amp, "text_escape_amp");
    rt_bind(0x0046f830u, b_clamp_99, "clamp_99");
    rt_bind(0x0046f840u, b_rumble_play, "rumble_play");
    rt_bind(0x0046f878u, b_sound_loop, "sound_loop");
    rt_bind(0x0046f860u, b_rumble_stop, "rumble_stop");
    rt_bind(0x0046f898u, b_sound_loop_stop, "sound_loop_stop");
    rt_bind(0x0046f934u, b_sound_stop_instance, "sound_stop_instance");
    rt_bind(0x0046f918u, b_sound_stop, "sound_stop");
    rt_bind(0x0046f8b0u, b_sound_play, "sound_play");
    rt_bind(0x0046f8ccu, b_sound_play_variant, "sound_play_variant");
    rt_bind(0x0046f94cu, b_sound_flush, "sound_flush");
    rt_bind(0x0046f958u, b_sound_method1, "sound_method1");
    rt_bind(0x0046f974u, b_music_call0, "music_call0");
    rt_bind(0x0046f980u, b_music_call, "music_call");
    rt_bind(0x0046fcb0u, b_music_set_20, "music_set_20");
    rt_bind(0x0046fcc0u, b_music_alternate_track, "audioAlternateMusicTrack");
    rt_bind(0x0046fdf8u, b_music_fade_out, "audioFadeOutMusic");
    rt_bind(0x0046f988u, b_get_c8500c, "get_c8500c");
    rt_bind(0x0046f990u, b_get_c85058, "get_c85058");
    rt_bind(0x0046f9dcu, b_get_music_volume, "get_music_volume");
    rt_bind(0x0046fc0cu, b_flag81_set, "flag81_set");
    rt_bind(0x0046fc14u, b_flag81_clear, "flag81_clear");
    rt_bind(0x0046fc30u, b_flag84_get, "flag84_get");
    rt_bind(0x0046fc38u, b_flag85_get, "flag85_get");
    rt_bind(0x0046fc58u, b_flag82_set, "flag82_set");
    rt_bind(0x0046fc68u, b_flag84_set, "flag84_set");
    rt_bind(0x0046fc78u, b_flag86_set, "flag86_set");
    rt_bind(0x0046fc90u, b_set_621168, "set_621168");
    rt_bind(0x0046fca0u, b_flag80_set, "flag80_set");
    rt_bind(0x0046fca8u, b_flag80_get, "flag80_get");
    rt_bind(0x0046fea4u, b_countdown_start, "countdown_start");
    rt_bind(0x0046feb0u, b_countdown_step, "countdown_step");
    rt_bind(0x0046f780u, b_nop_46f780, "nop_46f780");
    rt_bind(0x0046f828u, b_nop_46f828, "nop_46f828");
}

RT_BIND_AREA(match_audio)
