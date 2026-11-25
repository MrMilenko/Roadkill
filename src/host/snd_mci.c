/*
 * snd_mci.c: MCI (WINMM.dll) for the game: CD audio and movies (port code, not from the
 * original; docs/PLATFORM.md Part C: cd_open 0x004b858c, cd_play 0x004b86dc, cd_command
 * 0x004b8b40, cd_monitor_thread 0x004b902c, the music object; movie 0x004b9bf0 .. 0x004ba39c).
 *
 * CD audio: mciSendCommandA drives one "cdaudio" device that plays the audio tracks of the
 * player's own CD image (cd_audio.c / cd_image.c: TM2_CDIMAGE, default the .cue in TM2_DATA's
 * parent folder, e.g. disc/TWISTEDMETAL2.cue; read at run time, never copied) through the
 * DirectSound mixer's SDL device. How the game uses it: cd_open opens "cdaudio" (by type id 516
 * with the element "D:", or by name) and sets the time format to TMSF; music_cd_play_track plays
 * MCI_FROM track N to track N + 1 (MCI_TO only when N + 1 <= the number of tracks, else to the
 * end of the disc); pause / resume are MCI_PAUSE and MCI_PLAY with no MCI_FROM; the monitor
 * thread asks MCI_STATUS mode every 3 s and starts the track again when it reads "stopped"
 * ([gameplay] loop_music: cd_audio.c starts it over at once instead).
 *
 *   MCI_OPEN 0x803: type "cdaudio" or id 516 (MCI_DEVTYPE_CD_AUDIO); any element (drive) is
 *     accepted. The device id is 1; opening again (shareable) gives the same id. Without an
 *     image or an audio device (TM2_NOSOUND=1) it fails with MCIERR_INVALID_DEVICE_NAME as the
 *     old stub did, so cd_open leaves the player closed and the game runs without music.
 *   MCI_CLOSE 0x804, MCI_PLAY 0x806 (MCI_FROM 4 / MCI_TO 8; a start inside the data track moves
 *     to the next audio track), MCI_SEEK 0x807 (MCI_TO, MCI_SEEK_TO_START 0x100 / END 0x200),
 *     MCI_STOP 0x808, MCI_PAUSE 0x809, MCI_RESUME 0x855, MCI_INFO 0x80a (product), MCI_GETDEVCAPS
 *     0x80b, MCI_SET 0x80d (time format MS 0 / MSF 2 / TMSF 10, door open / closed and audio
 *     on / off accepted and ignored), MCI_STATUS 0x814 (length, position, number of tracks,
 *     mode, media present, time format, ready, current track, MCI_CDA_STATUS_TYPE_TRACK; with
 *     MCI_TRACK 0x10 and MCI_STATUS_START 0x200). Positions as Windows gives them: MSF / MS
 *     count from the disc's start (LBA + 150), TMSF from the track's INDEX 01; lengths are MSF
 *     in the MSF and TMSF formats. Modes: stopped 0x20d, playing 0x20e, paused 0x211.
 *   MCI_NOTIFY (flag 1, the window in dwCallback): MM_MCINOTIFY 0x3b9 (wParam SUCCESSFUL 1,
 *     SUPERSEDED 2, ABORTED 4; lParam the device id) to the window's procedure. The game itself
 *     never asks for it. HOST DIFFERENCE: the notifications are kept and delivered at the next
 *     MCI call (the host has no way to post to a window from the audio thread), not by the
 *     window's message queue.
 *   Other commands: MCIERR_UNSUPPORTED_FUNCTION; other device ids MCIERR_INVALID_DEVICE_ID.
 *     (Not 0x123: cd_command 0x004b8b40 shows an error box and retries on that one.)
 *   mciSendStringA: the movies' "avivideo" device answers its strings (host/movies/mci_avi.c:
 *     "open avivideo", "open <file> alias mov ...", "play mov window notify", ...); every other
 *     string fails with MCIERR_INVALID_DEVICE_NAME. The return string, when asked for, is
 *     emptied first.
 *   mciGetErrorStringA: a short text for the codes above.
 * The CD volume (auxSetVolume on the aux device of type AUXCAPS_CDAUDIO) is win32_mm.c's.
 * TM2_CD_LOG=1 logs every MCI command and its answer (and cd_audio.c the mixing).
 */
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "cd_audio.h"
#include "dx_com.h"
#include "win32_host.h"
#include "host/movies/movies.h"

#define MCIERR_INVALID_DEVICE_ID    257u
#define MCIERR_UNRECOGNIZED_KEYWORD 259u
#define MCIERR_HARDWARE             262u
#define MCIERR_INVALID_DEVICE_NAME  263u
#define MCIERR_UNSUPPORTED_FUNCTION 274u
#define MCIERR_OUTOFRANGE           282u
#define MCIERR_BAD_TIME_FORMAT      293u

#define MCI_NOTIFY 1u
#define MCI_FROM 4u
#define MCI_TO 8u
#define MCI_TRACK 0x10u
#define MCI_ALL_DEVICE_ID 0xffffu

#define F_MS 0u
#define F_MSF 2u
#define F_TMSF 10u

#define DEV_ID 1u

static struct {
    int open;
    uint32_t fmt;
    CdImage* img;
    uint32_t notify_hwnd;     /* a play's pending MCI_NOTIFY, 0: none */
    uint32_t notify_done;     /* cd_audio_finished() when it was asked */
    struct { uint32_t hwnd, code; } q[16];
    int nq;
} M;

#if defined(__GNUC__)
__attribute__((format(printf, 1, 0)))
#endif
static void cdlog(const char* fmt, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    if (!cd_audio_log()) return;
    fprintf(stderr, "tm2 cd: ");
    fprintf(stderr, fmt, a, b, c, d);
    fputc('\n', stderr);
}

/* ---- notifications ---- */

static void notify(uint32_t hwnd, uint32_t code)
{
    if (!hwnd) return;
    if (M.nq == 16) { memmove(M.q, M.q + 1, sizeof M.q[0] * 15); M.nq--; }
    M.q[M.nq].hwnd = hwnd;
    M.q[M.nq].code = code;
    M.nq++;
}
/* the pending play notification ends with code (SUPERSEDED 2 / ABORTED 4) */
static void notify_end_play(uint32_t code) { notify(M.notify_hwnd, code); M.notify_hwnd = 0; }
static void notify_check(void)
{
    if (M.notify_hwnd && cd_audio_finished() != M.notify_done) notify_end_play(1);
}
static void notify_flush(void)
{
    notify_check();
    uint32_t slot = rt_import_slot("GetWindowLongA");
    while (M.nq) {
        uint32_t hwnd = M.q[0].hwnd, code = M.q[0].code;
        memmove(M.q, M.q + 1, sizeof M.q[0] * (size_t)--M.nq);
        uint32_t args[2] = {hwnd, (uint32_t)-4};   /* GWL_WNDPROC */
        uint32_t proc = slot ? rt_call(slot, args, 2) : 0;
        cdlog("MM_MCINOTIFY %u to window %08x", code, hwnd, 0, 0);
        if (proc) w32_call_wndproc(proc, hwnd, 0x3b9u, code, DEV_ID);
    }
}

/* ---- time formats ---- */

static uint32_t track_lba(int t) { return cd_image_track_lba(M.img, t); }

/* a position in the current format -> disc LBA; 0 = out of range */
static int to_lba(uint32_t v, uint32_t* lba)
{
    int64_t l;
    if (M.fmt == F_TMSF) {
        int t = (int)(v & 0xff);
        if (t < 1 || t > cd_image_tracks(M.img)) return 0;
        l = (int64_t)track_lba(t) + ((int64_t)((v >> 8) & 0xff) * 60 + ((v >> 16) & 0xff)) * 75 + (v >> 24);
    } else if (M.fmt == F_MSF) {
        l = ((int64_t)(v & 0xff) * 60 + ((v >> 8) & 0xff)) * 75 + ((v >> 16) & 0xff) - 150;
    } else {
        l = (int64_t)v * 75 / 1000 - 150;
    }
    if (l < 0) l = 0;
    if (l > (int64_t)cd_image_end_lba(M.img)) return 0;
    *lba = (uint32_t)l;
    return 1;
}

static uint32_t msf(uint32_t frames) { return frames / 4500 | (frames / 75 % 60) << 8 | (frames % 75) << 16; }

/* a disc LBA in the current format */
static uint32_t from_lba(uint32_t lba)
{
    if (M.fmt == F_TMSF) {
        int t = cd_image_track_at(M.img, lba);
        uint32_t s = track_lba(t), r = lba > s ? lba - s : 0;
        return (uint32_t)t | (r / 4500) << 8 | (r / 75 % 60) << 16 | (r % 75) << 24;
    }
    if (M.fmt == F_MSF) return msf(lba + 150);
    return (uint32_t)((uint64_t)(lba + 150) * 1000 / 75);
}
/* a length: MSF in the MSF and TMSF formats */
static uint32_t length_of(uint32_t frames)
{
    return M.fmt == F_MS ? (uint32_t)((uint64_t)frames * 1000 / 75) : msf(frames);
}

/* ---- the commands ---- */

static uint32_t cd_open_cmd(uint32_t flags, uint32_t p)
{
    int is_cd = 0;
    uint32_t type = p ? rt_r32(p + 8) : 0;
    if (flags & 0x1000u) is_cd = (type & 0xffffu) == 516u;            /* MCI_OPEN_TYPE_ID */
    else if ((flags & 0x2000u) && type) is_cd = strncasecmp(w32_str(type), "cdaudio", 7) == 0;
    if (!is_cd) {
        dx_log_once("mciSendCommandA MCI_OPEN: not cdaudio, no such device");
        return MCIERR_INVALID_DEVICE_NAME;
    }
    const char* why = NULL;
    if (!M.img) M.img = cd_audio_init(&why);
    if (!M.img) {
        dx_log_once("mciSendCommandA MCI_OPEN cdaudio: %s", why ? why : "no CD image");
        return MCIERR_INVALID_DEVICE_NAME;
    }
    if (cd_audio_log()) {
        uint32_t el = (flags & 0x200u) ? rt_r32(p + 0xc) : 0;
        fprintf(stderr, "tm2 cd: MCI_OPEN cdaudio%s%s (flags %x): device %u\n", el ? " element " : "",
                el ? w32_str(el) : "", flags, DEV_ID);
    }
    if (!M.open) M.fmt = F_MSF;
    M.open++;
    rt_w32(p + 4, DEV_ID);
    return 0;
}

static uint32_t status_cmd(uint32_t flags, uint32_t p)
{
    if (!(flags & 0x100u)) return MCIERR_UNRECOGNIZED_KEYWORD;
    uint32_t item = rt_r32(p + 8), track = rt_r32(p + 0xc), r;
    int nt = cd_image_tracks(M.img);
    if ((flags & MCI_TRACK) && ((int)track < 1 || (int)track > nt)) return MCIERR_OUTOFRANGE;
    switch (item) {
    case 1:   /* MCI_STATUS_LENGTH */
        r = (flags & MCI_TRACK) ? length_of(cd_image_track_len(M.img, (int)track))
                                : length_of(cd_image_end_lba(M.img) - track_lba(1));
        break;
    case 2:   /* MCI_STATUS_POSITION */
        if (flags & MCI_TRACK) r = from_lba(track_lba((int)track));
        else if (flags & 0x200u) r = from_lba(track_lba(1));   /* MCI_STATUS_START */
        else r = from_lba(cd_audio_pos());
        break;
    case 3: r = (uint32_t)nt; break;   /* MCI_STATUS_NUMBER_OF_TRACKS */
    case 4: {                          /* MCI_STATUS_MODE */
        int s = cd_audio_state();
        r = s == CD_PLAYING ? 0x20eu : s == CD_PAUSED ? 0x211u : 0x20du;
        break;
    }
    case 5: r = 1; break;              /* MCI_STATUS_MEDIA_PRESENT */
    case 6: r = M.fmt; break;          /* MCI_STATUS_TIME_FORMAT */
    case 7: r = 1; break;              /* MCI_STATUS_READY */
    case 8: r = (uint32_t)cd_image_track_at(M.img, cd_audio_pos()); break;   /* CURRENT_TRACK */
    case 0x4001u:                      /* MCI_CDA_STATUS_TYPE_TRACK: AUDIO 0x3ff / OTHER 0x400 */
        if (!(flags & MCI_TRACK)) return MCIERR_UNRECOGNIZED_KEYWORD;
        r = cd_image_is_audio(M.img, (int)track) ? 0x3ffu : 0x400u;
        break;
    default: return MCIERR_UNSUPPORTED_FUNCTION;
    }
    rt_w32(p + 4, r);
    return 0;
}

static uint32_t play_cmd(uint32_t flags, uint32_t p)
{
    uint32_t from = cd_audio_pos(), to = cd_image_end_lba(M.img);
    if ((flags & MCI_FROM) && !to_lba(rt_r32(p + 4), &from)) return MCIERR_OUTOFRANGE;
    if ((flags & MCI_TO) && !to_lba(rt_r32(p + 8), &to)) return MCIERR_OUTOFRANGE;
    int t = cd_image_track_at(M.img, from);
    if (!cd_image_is_audio(M.img, t)) {   /* a start in a data track: the next audio track */
        while (t <= cd_image_tracks(M.img) && !cd_image_is_audio(M.img, t)) t++;
        if (t > cd_image_tracks(M.img)) return MCIERR_HARDWARE;
        from = track_lba(t);
    }
    if (to < from) return MCIERR_OUTOFRANGE;
    notify_end_play(2);   /* SUPERSEDED */
    cd_audio_play(from, to);
    if (flags & MCI_NOTIFY) { M.notify_hwnd = rt_r32(p) & 0xffffu; M.notify_done = cd_audio_finished(); }
    if (cd_audio_log()) {
        int tf = cd_image_track_at(M.img, from), tt = cd_image_track_at(M.img, to ? to - 1 : 0);
        fprintf(stderr, "tm2 cd: MCI_PLAY%s%s: LBA %u (track %d) .. %u (track %d end), %u:%02u\n",
                flags & MCI_FROM ? " from" : "", flags & MCI_TO ? " to" : "", from, tf, to, tt,
                (to - from) / 4500, (to - from) / 75 % 60);
    }
    return 0;
}

static uint32_t imp_mci_send_command(const uint32_t* a, int n)
{
    uint32_t id = dx_arg(a, n, 0), msg = dx_arg(a, n, 1), flags = dx_arg(a, n, 2), p = dx_arg(a, n, 3);
    if (M.open) notify_flush();
    if (msg == 0x803u) return p ? cd_open_cmd(flags, p) : MCIERR_UNRECOGNIZED_KEYWORD;
    if (!M.open || (id != DEV_ID && !(id == MCI_ALL_DEVICE_ID && (msg == 0x804u || msg == 0x808u))))
        return MCIERR_INVALID_DEVICE_ID;
    uint32_t err = 0;
    int done_now = 1;   /* MCI_NOTIFY answered at once (not a play) */
    switch (msg) {
    case 0x804u:   /* MCI_CLOSE */
        notify_end_play(4);
        cd_audio_stop();
        if (--M.open < 0 || id == MCI_ALL_DEVICE_ID) M.open = 0;
        cdlog("MCI_CLOSE (%u left open)", (uint32_t)M.open, 0, 0, 0);
        break;
    case 0x806u:   /* MCI_PLAY */
        if (!p && (flags & (MCI_FROM | MCI_TO | MCI_NOTIFY))) return MCIERR_UNRECOGNIZED_KEYWORD;
        err = play_cmd(flags, p);
        done_now = 0;
        break;
    case 0x807u: { /* MCI_SEEK */
        uint32_t lba = 0;
        if (flags & 0x100u) lba = track_lba(1);
        else if (flags & 0x200u) lba = cd_image_end_lba(M.img);
        else if ((flags & MCI_TO) && p) { if (!to_lba(rt_r32(p + 4), &lba)) { err = MCIERR_OUTOFRANGE; break; } }
        else { err = MCIERR_UNRECOGNIZED_KEYWORD; break; }
        notify_end_play(4);
        cd_audio_seek(lba);
        cdlog("MCI_SEEK to LBA %u (track %d)", lba, (uint32_t)cd_image_track_at(M.img, lba), 0, 0);
        break;
    }
    case 0x808u:   /* MCI_STOP */
        notify_end_play(4);
        cd_audio_stop();
        cdlog("MCI_STOP at LBA %u", cd_audio_pos(), 0, 0, 0);
        break;
    case 0x809u:   /* MCI_PAUSE */
        notify_end_play(4);
        cd_audio_pause();
        cdlog("MCI_PAUSE at LBA %u", cd_audio_pos(), 0, 0, 0);
        break;
    case 0x855u:   /* MCI_RESUME */
        cd_audio_resume();
        cdlog("MCI_RESUME at LBA %u", cd_audio_pos(), 0, 0, 0);
        break;
    case 0x80au: { /* MCI_INFO: MCI_INFO_PRODUCT 0x100 into lpstrReturn (+4), size +8 */
        if (!p || !(flags & 0x100u)) { err = MCIERR_UNRECOGNIZED_KEYWORD; break; }
        if (rt_r32(p + 4) && rt_r32(p + 8)) w32_put_str(rt_r32(p + 4), "CD Audio (image)", rt_r32(p + 8));
        break;
    }
    case 0x80bu: { /* MCI_GETDEVCAPS item +8 -> +4 */
        if (!p || !(flags & 0x100u)) { err = MCIERR_UNRECOGNIZED_KEYWORD; break; }
        uint32_t item = rt_r32(p + 8), r;
        switch (item) {
        case 2: case 7: case 8: r = 1; break;   /* HAS_AUDIO, CAN_EJECT, CAN_PLAY */
        case 4: r = 516; break;                 /* DEVICE_TYPE: MCI_DEVTYPE_CD_AUDIO */
        case 1: case 3: case 5: case 6: case 9: r = 0; break;
        default: err = MCIERR_UNSUPPORTED_FUNCTION; r = 0; break;
        }
        if (!err) rt_w32(p + 4, r);
        break;
    }
    case 0x80du:   /* MCI_SET */
        if (flags & 0x400u) {   /* MCI_SET_TIME_FORMAT */
            uint32_t f = p ? rt_r32(p + 4) : 0xffu;
            if (f != F_MS && f != F_MSF && f != F_TMSF) { err = MCIERR_BAD_TIME_FORMAT; break; }
            M.fmt = f;
        }
        cdlog("MCI_SET flags %x, time format %u", flags, M.fmt, 0, 0);
        break;
    case 0x814u:   /* MCI_STATUS */
        if (!p) return MCIERR_UNRECOGNIZED_KEYWORD;
        err = status_cmd(flags, p);
        if (cd_audio_log())
            fprintf(stderr, "tm2 cd: MCI_STATUS item %u%s -> %s%x\n", rt_r32(p + 8), flags & MCI_TRACK ? " (track)" : "",
                    err ? "error " : "", err ? err : rt_r32(p + 4));
        break;
    default:
        dx_log_once("mciSendCommandA %x: unsupported", msg);
        err = MCIERR_UNSUPPORTED_FUNCTION;
        break;
    }
    if (!err && done_now && (flags & MCI_NOTIFY) && p) notify(rt_r32(p) & 0xffffu, 1);
    if (err) cdlog("mciSendCommandA %x flags %x: error %u", msg, flags, err, 0);
    return err;
}

static uint32_t imp_mci_send_string(const uint32_t* a, int n)
{
    uint32_t cmd = dx_arg(a, n, 0), ret = dx_arg(a, n, 1), len = dx_arg(a, n, 2), err;
    if (ret && len) rt_w8(ret, 0);
    /* the movies' "avivideo" device (host/movies/mci_avi.c) */
    if (cmd && mci_avi_string(w32_str(cmd), ret, len, dx_arg(a, n, 3), &err)) return err;
    char c[64] = "";
    if (cmd) {
        size_t i = 0;
        for (; i + 1 < sizeof c && rt_r8(cmd + (uint32_t)i); i++) c[i] = (char)rt_r8(cmd + (uint32_t)i);
        c[i] = 0;
    }
    dx_log_once("mciSendStringA \"%s\": no MCI device", c);
    return MCIERR_INVALID_DEVICE_NAME;
}

static uint32_t imp_mci_get_error_string(const uint32_t* a, int n)
{
    uint32_t err = dx_arg(a, n, 0), buf = dx_arg(a, n, 1), len = dx_arg(a, n, 2);
    const char* t = err == MCIERR_INVALID_DEVICE_NAME ? "The specified device is not open or is not recognized by MCI."
                  : err == MCIERR_INVALID_DEVICE_ID   ? "Invalid device ID. Use the ID given to the device when the device was opened."
                  : err == MCIERR_OUTOFRANGE          ? "The specified parameter is out of range for the specified command."
                  : err == MCIERR_UNSUPPORTED_FUNCTION ? "The MCI device driver the system is using does not support the specified command."
                  : err == MCIERR_UNRECOGNIZED_KEYWORD ? "The driver cannot recognize the specified command parameter."
                  : err == MCIERR_BAD_TIME_FORMAT     ? "The specified value for the time format is invalid."
                  : err == MCIERR_HARDWARE            ? "The specified device cannot play the requested data."
                  : err == 275u                       ? "Cannot find the specified file. Make sure the path and filename are correct."
                  : err == 261u                       ? "Unknown command."
                  : err == 289u                       ? "The specified alias is already being used in this application. Use a unique alias."
                  : err == 348u                       ? "Cannot read the specified file."
                                                      : "Unknown MCI error.";
    if (!buf || !len) return 0;
    uint32_t k = 0;
    for (; k + 1 < len && t[k]; k++) rt_w8(buf + k, (uint8_t)t[k]);
    rt_w8(buf + k, 0);
    return 1;
}

static void rt_bind_host_snd_mci(void)
{
    rt_bind_import("mciSendCommandA", imp_mci_send_command);
    rt_bind_import("mciSendStringA", imp_mci_send_string);
    rt_bind_import("mciGetErrorStringA", imp_mci_get_error_string);
}

RT_BIND_AREA(host_snd_mci)
