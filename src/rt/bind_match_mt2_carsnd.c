/*
 * bind_match_mt2_carsnd.c: car_pair_vec 0x0045c1c4, carSoundUpdate 0x0045b5f0 and
 * ai_cars_respawn 0x0045c56c (src/match/mt2_carsnd.c), called the way
 * tools/emu/diff_mt2_carsnd.py called them:
 *   car_pair_vec(car EAX, other EDX) -> 0x0050d0d8 + mt2_car_pair_offset (32 bit wrap, as the original)
 *   carSoundUpdate(EAX ignored, car EDX, second BL): its sound wrappers are the mt_audio ports on
 *     a full MtAudio (the one bind_match_audio.c builds, duplicated: the files share no header)
 *   ai_cars_respawn(): its start set table is read from the player's TM2.EXE. The harness passed
 *     the whole file; the runtime keeps only the mapped image, so the table loader gets a PE image
 *     rebuilt from the mapped headers and sections (raw offsets rewritten to the RVAs), once, on
 *     the first call. The game never writes its code bytes, so they are the file's.
 */
#include <stdio.h>
#include <stdlib.h>

#include "rt_env.h"
#include "match/mt2_carsnd.h"

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

static const MtAudio* audio_env(void)
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

/* ---- carSoundUpdate ---- */

static uint32_t s_music_call(void* u, uint8_t* obj, uint32_t edx)
{
    (void)u;
    uint32_t a[4] = {rt_addr(obj), edx, 0, 0};
    return rt_call(0x004b8010u, a, 4);
}

static uint32_t b_car_sound_update(const uint32_t* a, int n)
{
    static Mt2CarSnd s;
    if (!s.audio) {
        s.audio = audio_env();
        s.flag_520e90 = (const uint32_t*)rt_ptr(0x00520e90u);
        s.player_mode = (const int32_t*)rt_ptr(0x00502fa4u);
        s.music_call = s_music_call;
    }
    mt2_car_sound_update(&s, rt_ptr(rt_arg(a, n, 1)), (uint8_t)rt_arg(a, n, 2));
    return 0;
}

/* ---- car_pair_vec ---- */

static uint32_t b_car_pair_vec(const uint32_t* a, int n)
{
    return 0x0050d0d8u + mt2_car_pair_offset((int32_t)rt_arg(a, n, 0), (int32_t)rt_arg(a, n, 1));
}

/* ---- ai_cars_respawn ---- */

static uint32_t r3(uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx)
{
    uint32_t a[4] = {eax, edx, ebx, 0};
    return rt_call(fn, a, 4);
}
#define RCB(nm, addr) static uint32_t nm(void* u, uint32_t eax, uint32_t edx, uint32_t ebx) { (void)u; return r3((addr), eax, edx, ebx); }
RCB(cb_rand, 0x004899c2u)
RCB(cb_schedule_event, 0x0046ca68u)
RCB(cb_freeze_reset_all, 0x00470900u)
RCB(cb_debris_clear, 0x00489a70u)
RCB(cb_start_nodes_clear, 0x0043bd60u)
RCB(cb_ai_car_start, 0x0043af58u)
RCB(cb_event_type, 0x00474508u)
RCB(cb_ai_place, 0x00432940u)

static uint32_t r_car_addr(void* u, uint8_t* car) { (void)u; return rt_addr(car); }

static uint32_t rd32le(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void wr32le(uint8_t* p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

/* a PE file view of the mapped image: the headers as mapped, each section's raw offset = its RVA */
static int load_table(Mt2RespawnTable* t)
{
    const uint8_t* img = rt_ptr(0x00400000u);
    uint32_t pe = rd32le(img + 0x3c);
    if (pe + 0xf8 > 0x1000u) return -1;
    uint32_t nsec = (uint32_t)(img[pe + 6] | img[pe + 7] << 8), opt = (uint32_t)(img[pe + 20] | img[pe + 21] << 8);
    if (pe + 24 + opt + 40 * nsec > 0x1000u) return -1;
    uint32_t end = 0x1000u;
    for (uint32_t i = 0; i < nsec; i++) {
        const uint8_t* s = img + pe + 24 + opt + 40 * i;
        uint32_t e = rd32le(s + 12) + rd32le(s + 16);
        if (rd32le(s + 20) && e > end) end = e;
    }
    if (0x00400000u + end > RT_HEAP_BASE) return -1;
    uint8_t* d = malloc(end);
    if (!d) return -1;
    memcpy(d, img, end);
    for (uint32_t i = 0; i < nsec; i++) {
        uint8_t* s = d + pe + 24 + opt + 40 * i;
        if (rd32le(s + 20)) wr32le(s + 20, rd32le(s + 12));
    }
    char err[200];
    int r = mt2_respawn_table_load(t, d, end, err, sizeof err);
    if (r) fprintf(stderr, "tm2: ai_cars_respawn start sets: %s\n", err);
    free(d);
    return r;
}

static uint32_t b_ai_cars_respawn(const uint32_t* a, int n)
{
    (void)a; (void)n;
    static Mt2RespawnTable table;
    static Mt2Respawn r;
    if (!r.table) {
        if (load_table(&table) != 0) rt_stop("ai_cars_respawn: no start set table", 0x0045c56cu);
        r.table = &table;
        r.tries = (int32_t*)rt_ptr(0x00502fa8u);
        r.flag_502f9c = rt_ptr(0x00502f9cu);
        r.used = rt_ptr(0x00502fa0u);
        r.v502fb4 = (int32_t*)rt_ptr(0x00502fb4u);
        r.v502fb8 = (int32_t*)rt_ptr(0x00502fb8u);
        r.v502fbc = (int32_t*)rt_ptr(0x00502fbcu);
        r.v502fc0 = (int32_t*)rt_ptr(0x00502fc0u);
        r.start_flag = (int32_t*)rt_ptr(0x00502fd8u);
        r.start = (int32_t*)rt_ptr(0x005030bcu);
        r.car_objects = (const uint32_t*)rt_ptr(0x005075e8u);
        r.cars = rt_ptr(0x0050760cu);
        r.car_count = (const int32_t*)rt_ptr(0x0050c2b0u);
        r.resolve = a_resolve;
        r.car_addr = r_car_addr;
        r.rand = cb_rand;
        r.schedule_event = cb_schedule_event;
        r.freeze_reset_all = cb_freeze_reset_all;
        r.debris_clear = cb_debris_clear;
        r.start_nodes_clear = cb_start_nodes_clear;
        r.ai_car_start = cb_ai_car_start;
        r.event_type = cb_event_type;
        r.ai_place = cb_ai_place;
    }
    mt2_ai_cars_respawn(&r);
    return 0;
}

void rt_bind_match_mt2_carsnd(void)
{
    rt_bind(0x0045b5f0u, b_car_sound_update, "carSoundUpdate");
    rt_bind(0x0045c1c4u, b_car_pair_vec, "car_pair_vec");
    rt_bind(0x0045c56cu, b_ai_cars_respawn, "ai_cars_respawn");
}

RT_BIND_AREA(match_mt2_carsnd)
