/*
 * mci_avi.c: the MCI "avivideo" device (movies.h) the game's movie code talks to through
 * mciSendStringA (port code, not from the original; docs/PLATFORM.md Part C: movie_init
 * 0x004b9bf0, movie_set_window 0x004b9c84, movie_open 0x004b9de0, movie_play 0x004ba15c,
 * movie_notify_done 0x004ba39c). Stands in for Windows 95's MCIAVI: the AVI files are read by
 * avi.c, the pictures decoded by FFmpeg's Indeo 3 / Indeo 4 decoders (third_party/indeo), drawn
 * over the game's picture by the display (dx_sdl_overlay) and the sound mixed into the
 * DirectSound mixer's SDL device (snd_mix_add).
 *
 * What the game sends (strings in TM2.EXE, the order of movie_play):
 *   "open avivideo" (movie_init: success sets 0x00c8511c, the movies usable flag; "close
 *     avivideo" at shutdown)
 *   "open <file> alias mov style child parent <hwnd>", "where mov source" (-> "0 0 320 240"; the
 *     third and fourth numbers are the movie's size), "put mov window client at x y w h" (w, h the
 *     size above, doubled by movie_set_double_size; x = y = -1 for the intro: centred),
 *     "window mov state show", "status mov window handle" (a number, kept in 0x00c8510c),
 *     "setaudio mov volume to <0..1000>", "play mov window notify" (the callback window in
 *     mciSendStringA's 4th argument), then movie_play waits (WaitMessage) until the window's
 *     MM_MCINOTIFY handler (0x004ba39c, wParam MCI_NOTIFY_SUCCESSFUL 1) clears the playing flag
 *     0x00c85110, or a key skips; then "pause mov" + "seek mov to end" when it was not skipped,
 *     and "close mov".
 * Also understood (MCIAVI's own words, for completeness): "type avivideo" in open, "put mov
 *   destination [at x y w h]", "window mov handle <n>" / "state hide", "play [from n] [to n]
 *   [wait]", "resume", "stop", "seek to start / end / n", "step [reverse] [by n]", "status mode /
 *   position / length / ready / time format / window handle", "set time format frames / ms",
 *   "close all".
 *
 * Playing: the frames are timed by the clock from the play's start (dwMicroSecPerFrame); the due
 *   frame is decoded (every frame in between too: the Indeo frames depend on the previous ones)
 *   and shown when the Win32 host calls mci_avi_idle() (its WaitMessage, on the game's thread),
 *   and the end of the play is the later of the last frame's time and the sound's end. The sound
 *   (PCM 8 / 16 bit, mono / stereo, 22050 Hz in the game's files) is read whole at open and
 *   resampled into the mixer from the play's start. The window rectangle is in the game's
 *   display coordinates; a movie wider than the display is scaled to fit.
 *   HOST DIFFERENCE: MM_MCINOTIFY is delivered by calling the window's procedure from
 *   mci_avi_idle() or the next MCI string (the game's wait loop dispatches no messages; on
 *   Windows the procedure gets it from MCIAVI's thread).
 *
 * [display] skip_movies = 1 (TM2_SKIP_MOVIES=1; docs/QOL.md): "open avivideo" fails as with no
 *   driver, so the game plays no movie (the behavior before this device). Default 0: the movies
 *   play, as the original.
 * ROADKILL_MOVIE_LOG=1 logs every string and its answer, the frames decoded, the sound mixed and
 *   the notifications. ROADKILL_MOVIE_SHOT_DIR=<dir> saves every 30th frame shown as
 *   <dir>/<MOVIE>_<frame>.bmp (the window as presented).
 */
#include "movies.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <SDL.h>

#include "avi.h"
#include "host/cd_audio.h"
#include "host/dx_com.h"
#include "host/dx_sdl.h"
#include "host/settings/settings.h"
#include "host/win32_host.h"
#include "indeo.h"

#define MCIERR_INVALID_DEVICE_NAME 263u
#define MCIERR_UNRECOGNIZED_COMMAND 261u
#define MCIERR_UNRECOGNIZED_KEYWORD 259u
#define MCIERR_MISSING_PARAMETER 273u
#define MCIERR_UNSUPPORTED_FUNCTION 274u
#define MCIERR_FILE_NOT_FOUND 275u
#define MCIERR_OUTOFRANGE 282u
#define MCIERR_DUPLICATE_ALIAS 289u
#define MCIERR_BAD_TIME_FORMAT 293u
#define MCIERR_FILE_READ 348u

#define MM_MCINOTIFY 0x3b9u
#define NOTIFY_SUCCESSFUL 1u
#define NOTIFY_SUPERSEDED 2u
#define NOTIFY_ABORTED 4u

#define DEV_ID 2u   /* the movie's device id in MM_MCINOTIFY's lParam (the CD player's is 1) */

enum { M_STOPPED, M_PLAYING, M_PAUSED };

static struct {
    int log;              /* ROADKILL_MOVIE_LOG */
    int driver;           /* "open avivideo" done */
    /* the open movie */
    int open;
    char alias[64];
    char path[1024];
    AviFile* avi;
    AviInfo info;
    IndeoDecoder* dec;
    int dec_next;         /* the next frame the decoder takes */
    int have_pic;
    uint32_t* rgb;        /* the picture shown, XRGB */
    int shown;            /* the frame in rgb, -1: none */
    uint8_t* audio;       /* the whole sound stream */
    uint32_t a_frames;    /* in sample frames */
    uint32_t parent, window;   /* parent window, "window handle" */
    int visible;
    int win_x, win_y, win_w, win_h;   /* the movie window, display coordinates */
    int dst_set, dst_x, dst_y, dst_w, dst_h;   /* "put destination", inside the window */
    int volume;           /* 0..1000 */
    int audio_on;
    int ms_format;        /* time format milliseconds (else frames) */
    /* playing */
    int mode;
    int pos;              /* the current frame */
    int from, to;         /* the play's range [from, to) */
    uint64_t t0;          /* the performance counter at frame `from` */
    double apos;          /* the sound's position in sample frames (the mixer's) */
    double aend;          /* where the play's sound stops */
    int a_playing;
    uint32_t notify_hwnd; /* the play's pending notify */
    /* notifications to send */
    struct { uint32_t hwnd, code; } q[8];
    int nq;
    /* log counters */
    uint32_t n_decoded, n_shown;
    int last_shot;
    uint64_t n_mixed;
    int mixer_added;
} V = {.shown = -1, .volume = 1000, .audio_on = 1, .last_shot = -1};

#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
static void mlog(const char* fmt, ...)
{
    if (!V.log) return;
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "tm2 movie: ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

static uint64_t now_us(void)
{
    static uint64_t freq;
    if (!freq) freq = SDL_GetPerformanceFrequency();
    return (uint64_t)((double)SDL_GetPerformanceCounter() * 1e6 / (double)freq);
}

/* ---- notifications ---- */

static void notify(uint32_t hwnd, uint32_t code)
{
    if (!hwnd) return;
    if (V.nq == 8) { memmove(V.q, V.q + 1, sizeof V.q[0] * 7); V.nq--; }
    V.q[V.nq].hwnd = hwnd;
    V.q[V.nq].code = code;
    V.nq++;
}
static void notify_end_play(uint32_t code) { notify(V.notify_hwnd, code); V.notify_hwnd = 0; }

/* sends the queued MM_MCINOTIFY to the windows' procedures (on the game's thread) */
static void notify_flush(void)
{
    if (!V.nq) return;
    uint32_t slot = rt_import_slot("GetWindowLongA");
    while (V.nq) {
        uint32_t hwnd = V.q[0].hwnd, code = V.q[0].code;
        memmove(V.q, V.q + 1, sizeof V.q[0] * (size_t)--V.nq);
        uint32_t args[2] = {hwnd, (uint32_t)-4};   /* GWL_WNDPROC */
        uint32_t proc = slot ? rt_call(slot, args, 2) : 0;
        mlog("MM_MCINOTIFY %s to window %08x", code == 1 ? "SUCCESSFUL" : code == 2 ? "SUPERSEDED" : "ABORTED", hwnd);
        if (proc) w32_call_wndproc(proc, hwnd, MM_MCINOTIFY, code, DEV_ID);
    }
}

/* ---- sound ---- */

static int16_t a_sample(uint32_t f, int ch)
{
    if (f >= V.a_frames) return 0;
    if (ch >= V.info.a_channels) ch = V.info.a_channels - 1;
    if (V.info.a_bits == 8) return (int16_t)((V.audio[f * (uint32_t)V.info.a_channels + (uint32_t)ch] - 128) << 8);
    const uint8_t* p = V.audio + (f * (uint32_t)V.info.a_channels + (uint32_t)ch) * 2;
    return (int16_t)(p[0] | p[1] << 8);
}

/* the mixer's stream (audio thread, device locked) */
static void mix(int32_t* acc, int frames)
{
    if (!V.a_playing || !V.audio) return;
    double step = (double)V.info.a_rate / 44100.0, g = V.audio_on ? V.volume / 1000.0 : 0.0;
    int i = 0;
    for (; i < frames && V.apos < V.aend; i++) {
        uint32_t f = (uint32_t)V.apos;
        double t = V.apos - f;
        double l = a_sample(f, 0) * (1 - t) + a_sample(f + 1, 0) * t;
        double r = a_sample(f, 1) * (1 - t) + a_sample(f + 1, 1) * t;
        acc[2 * i] += (int32_t)(l * g);
        acc[2 * i + 1] += (int32_t)(r * g);
        V.apos += step;
    }
    V.n_mixed += (uint64_t)i;
    if (V.apos >= V.aend) V.a_playing = 0;
}

static void audio_start(int frame)
{
    if (!V.audio || !snd_audio_open()) return;
    if (!V.mixer_added) V.mixer_added = snd_mix_add(mix) == 0;
    snd_audio_lock();
    V.apos = (double)frame * V.info.us_per_frame * V.info.a_rate / 1e6;
    V.aend = V.to >= V.info.frames ? (double)V.a_frames : (double)V.to * V.info.us_per_frame * V.info.a_rate / 1e6;
    V.a_playing = V.apos < V.aend;
    snd_audio_unlock();
}

static void audio_stop(void)
{
    snd_audio_lock();
    V.a_playing = 0;
    snd_audio_unlock();
}

/* ---- pictures ---- */

/* decodes up to frame k (from the start again when k is behind the decoder) into V.rgb */
static void decode_to(int k)
{
    if (!V.dec || k < 0 || k >= V.info.frames || k == V.shown) return;
    if (k < V.dec_next - 1 || V.dec_next == 0) {
        if (V.dec_next) {   /* back: from the start again */
            indeo_close(V.dec);
            V.dec = indeo_open(V.info.fourcc, V.info.width, V.info.height);
            V.dec_next = 0;
            V.have_pic = 0;
            if (!V.dec) return;
        }
    }
    IndeoPicture pic;
    int got = 0;
    while (V.dec_next <= k) {
        const uint8_t* data;
        int n = avi_read_frame(V.avi, V.dec_next, &data);
        V.dec_next++;
        if (n <= 0) continue;   /* a 0 byte chunk: the picture stays */
        int r = indeo_decode(V.dec, data, n, &pic);
        if (r > 0) { got = 1; V.n_decoded++; }
        else if (r < 0) mlog("frame %d: decode error %d", V.dec_next - 1, r);
    }
    if (got) {
        indeo_to_xrgb(&pic, V.rgb, V.info.width * 4);
        V.have_pic = 1;
    }
    V.shown = k;
}

/* the movie's rectangle in the display */
static void dest_rect(int* x, int* y, int* w, int* h)
{
    int cw, ch;
    dx_sdl_client_size(&cw, &ch);
    int ww = V.win_w, wh = V.win_h, wx = V.win_x, wy = V.win_y;
    if (ww <= 0 || wh <= 0) {
        /* an empty window: the port's movie_play hands movie_open no size (its binding reads the
         * caller's ebx / ecx as the stack words: "put mov window client at -1 0 0 0"), where the
         * original sends -1 -1 and the source size doubled (movie_set_double_size): 640 x 480,
         * the whole display. Shown that way: scaled to fit below. */
        ww = cw * 4;
        wh = ww * V.info.height / (V.info.width > 0 ? V.info.width : 1);
        wx = wy = -1;
    }
    if (wx < 0 || wy < 0) { wx = (cw - ww) / 2; wy = (ch - wh) / 2; }   /* -1: centred (the intro) */
    if (V.dst_set) { wx += V.dst_x; wy += V.dst_y; ww = V.dst_w; wh = V.dst_h; }
    if (ww > cw || wh > ch) {   /* larger than the display: fit, keeping the shape */
        double s = fmin((double)cw / ww, (double)ch / wh);
        ww = (int)(ww * s);
        wh = (int)(wh * s);
        wx = (cw - ww) / 2;
        wy = (ch - wh) / 2;
    }
    *x = wx; *y = wy; *w = ww; *h = wh;
}

static void show(void)
{
    if (!V.visible || !V.have_pic) return;
    int x, y, w, h;
    dest_rect(&x, &y, &w, &h);
    dx_sdl_overlay(V.rgb, V.info.width, V.info.height, V.info.width * 4, x, y, w, h);
    if (V.n_shown < 2 || V.n_shown % 150 == 0) mlog("frame %d shown at %d %d %d %d", V.shown, x, y, w, h);
    V.n_shown++;
    /* ROADKILL_MOVIE_SHOT_DIR: every 30th frame as <dir>/<movie>_<frame>.bmp (the whole window) */
    const char* dir = getenv("ROADKILL_MOVIE_SHOT_DIR");
    if (dir && *dir && V.shown % 30 == 0 && V.shown != V.last_shot) {
        const char* b = strrchr(V.path, '/');
        char path[1200];
        snprintf(path, sizeof path, "%s/%.*s_%04d.bmp", dir, (int)strcspn(b ? b + 1 : V.path, "."), b ? b + 1 : V.path,
                 V.shown);
        V.last_shot = V.shown;
        if (dx_sdl_screenshot(path) == 0) mlog("frame %d -> %s", V.shown, path);
    }
}

/* ---- the device ---- */

static void stop_play(uint32_t code)
{
    if (V.mode == M_PLAYING) audio_stop();
    V.mode = M_STOPPED;
    notify_end_play(code);
}

static void close_movie(void)
{
    if (!V.open) return;
    stop_play(NOTIFY_ABORTED);
    mlog("close %s: %u pictures decoded, %u shown, %llu sound frames mixed", V.alias, V.n_decoded, V.n_shown,
         (unsigned long long)V.n_mixed);
    snd_audio_lock();
    V.a_playing = 0;
    free(V.audio);
    V.audio = NULL;
    snd_audio_unlock();
    indeo_close(V.dec);
    avi_close(V.avi);
    free(V.rgb);
    V.dec = NULL;
    V.avi = NULL;
    V.rgb = NULL;
    V.open = 0;
    V.visible = 0;
    V.alias[0] = 0;
    dx_sdl_overlay_off();
}

static void put_ret(uint32_t ret, uint32_t len, const char* s)
{
    if (ret && len) w32_put_str(ret, s, len);
    mlog("  -> \"%s\"", s);
}

static int to_frames(long v) { return V.ms_format ? (int)((double)v * 1000.0 / V.info.us_per_frame) : (int)v; }
static long from_frames(int f) { return V.ms_format ? (long)((double)f * V.info.us_per_frame / 1000.0) : (long)f; }

/* the tokens of a command (quotes kept together) */
#define MAXTOK 24
static int tokenize(char* s, char** tok)
{
    int n = 0;
    while (*s && n < MAXTOK) {
        while (*s == ' ' || *s == '\t') s++;
        if (!*s) break;
        if (*s == '"') {
            tok[n++] = ++s;
            while (*s && *s != '"') s++;
        } else {
            tok[n++] = s;
            while (*s && *s != ' ' && *s != '\t') s++;
        }
        if (*s) *s++ = 0;
    }
    return n;
}

static int is(const char* a, const char* b) { return a && !strcasecmp(a, b); }
static int find(char** t, int n, const char* w)
{
    for (int i = 2; i < n; i++) if (is(t[i], w)) return i;
    return 0;
}

static uint32_t cmd_open(char** t, int n)
{
    if (V.open) return MCIERR_DUPLICATE_ALIAS;
    const char* game = t[1];
    const char* alias = NULL;
    for (int i = 2; i + 1 < n; i++) {
        if (is(t[i], "alias")) alias = t[i + 1];
        else if (is(t[i], "parent")) V.parent = (uint32_t)strtoul(t[i + 1], NULL, 0);
    }
    char host[1024];
    w32_host_path(game, host, sizeof host);
    const char* why = NULL;
    AviFile* f = avi_open(host, &why);
    if (!f) {
        mlog("open %s (%s): %s", game, host, why);
        return MCIERR_FILE_NOT_FOUND;
    }
    const AviInfo* in = avi_info(f);
    IndeoDecoder* d = indeo_open(in->fourcc, in->width, in->height);
    if (!d) {
        dx_log_once("MCI avivideo: %s: video compression %.4s not supported", game, (const char*)&in->fourcc);
        avi_close(f);
        return MCIERR_FILE_READ;
    }
    V.avi = f;
    V.dec = d;
    V.info = *in;
    V.dec_next = 0;
    V.have_pic = 0;
    V.shown = -1;
    V.rgb = calloc((size_t)in->width * (size_t)in->height, 4);
    V.audio = NULL;
    V.a_frames = 0;
    if (in->has_audio && in->a_bytes) {
        uint8_t* a = malloc(in->a_bytes);
        if (a) {
            uint32_t got = avi_read_audio(f, a, in->a_bytes);
            V.a_frames = got / (uint32_t)(in->a_channels * in->a_bits / 8);
            snd_audio_lock();
            V.audio = a;
            snd_audio_unlock();
        }
    }
    snprintf(V.alias, sizeof V.alias, "%s", alias ? alias : game);
    snprintf(V.path, sizeof V.path, "%s", host);
    V.open = 1;
    V.mode = M_STOPPED;
    V.pos = 0;
    V.visible = 0;
    V.win_x = V.win_y = 0;
    V.win_w = in->width;
    V.win_h = in->height;
    V.dst_set = 0;
    V.window = V.parent;
    V.ms_format = 0;
    V.n_decoded = V.n_shown = 0;
    V.n_mixed = 0;
    V.last_shot = -1;
    mlog("open %s: %dx%d %.4s, %d frames at %u us, sound %d Hz %d ch %d bit (%u frames)", host, in->width,
         in->height, (const char*)&in->fourcc, in->frames, in->us_per_frame, in->a_rate, in->a_channels, in->a_bits,
         V.a_frames);
    return 0;
}

static uint32_t cmd_play(char** t, int n, uint32_t cb)
{
    int from = V.pos, to = V.info.frames, k;
    if ((k = find(t, n, "from")) && k + 1 < n) from = to_frames(strtol(t[k + 1], NULL, 0));
    if ((k = find(t, n, "to")) && k + 1 < n) to = to_frames(strtol(t[k + 1], NULL, 0));
    if (from < 0 || from > V.info.frames || to < from || to > V.info.frames) return MCIERR_OUTOFRANGE;
    if (from >= V.info.frames) from = 0;   /* at the end: MCIAVI starts over */
    stop_play(NOTIFY_SUPERSEDED);
    V.from = from;
    V.to = to;
    V.pos = from;
    V.mode = M_PLAYING;
    V.visible = 1;
    if (find(t, n, "notify")) V.notify_hwnd = cb;
    V.t0 = now_us();
    audio_start(from);
    decode_to(from);
    show();
    mlog("play frames %d..%d%s (thread %u)", from, to, V.notify_hwnd ? " with notify" : "", w32_thread_id());
    if (find(t, n, "wait")) {   /* not used by the game: plays here until the end */
        while (V.mode == M_PLAYING) { mci_avi_idle(); SDL_Delay(5); }
    }
    return 0;
}

/* advances a playing movie: the due frame, the end */
static void advance(void)
{
    if (V.mode != M_PLAYING) return;
    uint64_t el = now_us() - V.t0;
    int due = V.from + (int)(el / V.info.us_per_frame);
    int last = V.to > V.from ? V.to - 1 : V.from;
    if (due > last) due = last;
    if (due != V.shown) {
        decode_to(due);
        show();
    }
    V.pos = due;
    double vend = (double)(V.to - V.from) * V.info.us_per_frame;
    if ((double)el >= vend && !V.a_playing) {
        V.pos = V.to;
        V.mode = M_STOPPED;
        mlog("play ends at frame %d after %.2f s: %u pictures decoded, %u shown, %llu sound frames mixed", V.pos,
             (double)el / 1e6, V.n_decoded, V.n_shown, (unsigned long long)V.n_mixed);
        notify_end_play(NOTIFY_SUCCESSFUL);
    }
}

void mci_avi_idle(void)
{
    if (!V.open && !V.nq) return;
    advance();
    notify_flush();
}

static uint32_t cmd_status(char** t, int n, uint32_t ret, uint32_t len)
{
    char b[64];
    const char* w = n > 2 ? t[2] : "";
    if (is(w, "mode")) snprintf(b, sizeof b, "%s", V.mode == M_PLAYING ? "playing" : V.mode == M_PAUSED ? "paused" : "stopped");
    else if (is(w, "position")) snprintf(b, sizeof b, "%ld", from_frames(V.pos));
    else if (is(w, "length")) snprintf(b, sizeof b, "%ld", from_frames(V.info.frames));
    else if (is(w, "ready")) snprintf(b, sizeof b, "true");
    else if (is(w, "time")) snprintf(b, sizeof b, "%s", V.ms_format ? "milliseconds" : "frames");
    else if (is(w, "window") && n > 3 && is(t[3], "handle")) snprintf(b, sizeof b, "%u", V.window);
    else if (is(w, "window") && n > 3 && is(t[3], "visible")) snprintf(b, sizeof b, "%s", V.visible ? "true" : "false");
    else if (is(w, "media") || is(w, "audio")) snprintf(b, sizeof b, "%s", "true");
    else if (is(w, "volume")) snprintf(b, sizeof b, "%d", V.volume);
    else if (is(w, "frame") && n > 3 && is(t[3], "rate")) snprintf(b, sizeof b, "%u", (unsigned)(1e9 / V.info.us_per_frame));
    else if (is(w, "number") || is(w, "current")) snprintf(b, sizeof b, "1");
    else return MCIERR_UNSUPPORTED_FUNCTION;
    put_ret(ret, len, b);
    return 0;
}

/* "x y w h" from t[i..] */
static int four(char** t, int n, int i, int* x, int* y, int* w, int* h)
{
    if (i + 3 >= n) return 0;
    *x = (int)strtol(t[i], NULL, 0);
    *y = (int)strtol(t[i + 1], NULL, 0);
    *w = (int)strtol(t[i + 2], NULL, 0);
    *h = (int)strtol(t[i + 3], NULL, 0);
    return 1;
}

static uint32_t command(char** t, int n, uint32_t ret, uint32_t len, uint32_t cb)
{
    const char* verb = t[0];
    if (is(verb, "open")) return cmd_open(t, n);
    if (is(verb, "close") && is(t[1], "all")) { close_movie(); V.driver = 0; return 0; }
    if (!V.open || strcasecmp(t[1], V.alias)) return MCIERR_INVALID_DEVICE_NAME;
    int k, x, y, w, h;
    if (is(verb, "close")) { close_movie(); return 0; }
    if (is(verb, "play")) return cmd_play(t, n, cb);
    if (is(verb, "status")) return cmd_status(t, n, ret, len);
    if (is(verb, "where")) {
        char b[64];
        if (n > 2 && is(t[2], "source")) snprintf(b, sizeof b, "0 0 %d %d", V.info.width, V.info.height);
        else if (n > 2 && is(t[2], "destination")) {
            if (V.dst_set) snprintf(b, sizeof b, "%d %d %d %d", V.dst_x, V.dst_y, V.dst_w, V.dst_h);
            else snprintf(b, sizeof b, "0 0 %d %d", V.win_w, V.win_h);
        } else if (n > 2 && is(t[2], "window")) snprintf(b, sizeof b, "%d %d %d %d", V.win_x, V.win_y, V.win_w, V.win_h);
        else return MCIERR_UNRECOGNIZED_KEYWORD;
        put_ret(ret, len, b);
        return 0;
    }
    if (is(verb, "put")) {
        if (n > 2 && is(t[2], "window")) {
            if (!(k = find(t, n, "at")) || !four(t, n, k + 1, &x, &y, &w, &h)) return MCIERR_MISSING_PARAMETER;
            V.win_x = x; V.win_y = y; V.win_w = w; V.win_h = h;
        } else if (n > 2 && is(t[2], "destination")) {
            if ((k = find(t, n, "at")) && four(t, n, k + 1, &x, &y, &w, &h)) {
                V.dst_set = 1; V.dst_x = x; V.dst_y = y; V.dst_w = w; V.dst_h = h;
            } else V.dst_set = 0;
        } else if (n > 2 && (is(t[2], "source") || is(t[2], "frame") || is(t[2], "video"))) {
            /* a source rectangle: the whole picture is shown */
        } else return MCIERR_UNRECOGNIZED_KEYWORD;
        if (V.visible && V.mode != M_PLAYING) show();
        return 0;
    }
    if (is(verb, "window")) {
        if ((k = find(t, n, "handle")) && k + 1 < n) {
            V.window = is(t[k + 1], "default") ? V.parent : (uint32_t)strtoul(t[k + 1], NULL, 0);
            V.win_x = V.win_y = 0;
            dx_sdl_client_size(&V.win_w, &V.win_h);
        }
        if ((k = find(t, n, "state")) && k + 1 < n) {
            const char* s = t[k + 1];
            if (is(s, "hide") || is(s, "minimize") || is(s, "minimized")) { V.visible = 0; dx_sdl_overlay_off(); }
            else V.visible = 1;
            if (V.visible) { decode_to(V.pos < V.info.frames ? V.pos : V.info.frames - 1); show(); }
        }
        return 0;
    }
    if (is(verb, "setaudio")) {
        if ((k = find(t, n, "volume")) && k + 2 < n && is(t[k + 1], "to")) {
            int v = (int)strtol(t[k + 2], NULL, 0);
            V.volume = v < 0 ? 0 : v > 1000 ? 1000 : v;
        } else if (find(t, n, "off")) V.audio_on = 0;
        else if (find(t, n, "on")) V.audio_on = 1;
        return 0;
    }
    if (is(verb, "pause") || is(verb, "stop")) {
        if (V.mode == M_PLAYING) { advance(); stop_play(NOTIFY_ABORTED); }
        if (is(verb, "pause") && V.open) V.mode = M_PAUSED;
        if (is(verb, "stop")) V.mode = M_STOPPED;
        return 0;
    }
    if (is(verb, "resume")) {
        if (V.mode == M_PAUSED) {
            char* p[3] = {"play", V.alias, NULL};
            return cmd_play(p, 2, 0);
        }
        return 0;
    }
    if (is(verb, "seek")) {
        int to;
        if (find(t, n, "start")) to = 0;
        else if (find(t, n, "end")) to = V.info.frames;
        else if ((k = find(t, n, "to")) && k + 1 < n) to = to_frames(strtol(t[k + 1], NULL, 0));
        else return MCIERR_MISSING_PARAMETER;
        if (to < 0 || to > V.info.frames) return MCIERR_OUTOFRANGE;
        stop_play(NOTIFY_ABORTED);
        V.pos = to;
        if (V.visible && to < V.info.frames) { decode_to(to); show(); }
        return 0;
    }
    if (is(verb, "step")) {
        int by = 1;
        if ((k = find(t, n, "by")) && k + 1 < n) by = (int)strtol(t[k + 1], NULL, 0);
        if (find(t, n, "reverse")) by = -by;
        stop_play(NOTIFY_ABORTED);
        V.pos += by;
        if (V.pos < 0) V.pos = 0;
        if (V.pos >= V.info.frames) V.pos = V.info.frames - 1;
        if (V.visible) { decode_to(V.pos); show(); }
        return 0;
    }
    if (is(verb, "set")) {
        if ((k = find(t, n, "format")) && k + 1 < n) {
            if (is(t[k + 1], "frames")) V.ms_format = 0;
            else if (is(t[k + 1], "ms") || is(t[k + 1], "milliseconds")) V.ms_format = 1;
            else return MCIERR_BAD_TIME_FORMAT;
        }
        return 0;
    }
    if (is(verb, "realize") || is(verb, "update") || is(verb, "cue") || is(verb, "signal")) return 0;
    if (is(verb, "info")) { put_ret(ret, len, "Video for Windows (Roadkill)"); return 0; }
    return MCIERR_UNRECOGNIZED_COMMAND;
}

int mci_avi_string(const char* cmd, uint32_t ret, uint32_t retlen, uint32_t cb, uint32_t* err)
{
    static int init;
    if (!init) {
        init = 1;
        const char* e = getenv("ROADKILL_MOVIE_LOG");
        V.log = e && *e && *e != '0';
    }
    char buf[1100];
    snprintf(buf, sizeof buf, "%s", cmd ? cmd : "");
    char* t[MAXTOK];
    int n = tokenize(buf, t);
    if (n < 2) return 0;
    /* the driver itself: "open avivideo" / "close avivideo" */
    if (is(t[1], "avivideo") && (is(t[0], "open") || is(t[0], "close")) && n == 2) {
        if (is(t[0], "close")) { close_movie(); V.driver = 0; *err = 0; mlog("\"%s\" -> 0", cmd); return 1; }
        if (settings_bool("display.skip_movies", 0)) {
            dx_log_once("MCI avivideo: [display] skip_movies: no movies");
            *err = MCIERR_INVALID_DEVICE_NAME;
            return 1;
        }
        V.driver = 1;
        *err = 0;
        mlog("\"%s\" -> 0 (thread %u)", cmd, w32_thread_id());
        return 1;
    }
    /* a movie's own strings: "open <file>" with avivideo open or "type avivideo", and anything
     * naming the open movie's alias */
    int mine = 0;
    if (is(t[0], "open")) {
        const char* dot = strrchr(t[1], '.');
        mine = (V.driver && dot && !strcasecmp(dot, ".avi")) || find(t, n, "avivideo");
    } else {
        mine = (V.open && !strcasecmp(t[1], V.alias)) || (is(t[0], "close") && is(t[1], "all"));
    }
    if (!mine) return 0;
    notify_flush();
    *err = command(t, n, ret, retlen, cb);
    if (!*err && find(t, n, "notify") && !is(t[0], "play")) notify(cb, NOTIFY_SUCCESSFUL);
    mlog("\"%s\"%s -> %u", cmd, cb ? " (callback)" : "", *err);
    return 1;
}
