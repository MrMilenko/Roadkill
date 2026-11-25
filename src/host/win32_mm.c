/*
 * win32_mm.c: the WINMM imports other than MCI (timers, joysticks, aux / mixer volume), port
 * code. MCI (mciSendCommandA / mciSendStringA / mciGetErrorStringA: CD audio and movies) is the
 * audio host's.
 *
 * Joysticks: the SDL joysticks (TM2_NOJOY=1: none). joyGetNumDevs answers 16 as Windows does;
 * joyGetDevCapsA / joyGetPosEx answer JOYERR_UNPLUGGED (167) for an id with no SDL joystick.
 * Axes map -32768..32767 to 0..65535, the first hat to the POV (hundredths of a degree).
 * Aux: one device, 0, the CD audio line (AUXCAPS_CDAUDIO, volume left / right): auxSetVolume /
 * auxGetVolume set and read the CD music's gain (cd_audio.c; low word left, high word right,
 * 0..0xffff, linear; 0xffffffff at start). The game's cd_set_volume 0x004b8f48 goes there when
 * its "use aux volume" flag is on.
 * Mixer: one device, 0 ("Roadkill Mixer"), as a Windows 95 card shows it: the speakers
 * destination (line 0xffff0000) with two sources, wave out (0x00000000) and CD audio
 * (0x00010000), each a volume (MIXERCONTROL_CONTROLTYPE_VOLUME, 0..0xffff per channel) and a mute
 * control (MIXERCONTROL_CONTROLTYPE_MUTE); control id = line index * 2 (+ 1 for the mute). No
 * bass / treble: the audio screen then shows its three sliders. The game addresses it by id with
 * any MIXER_OBJECTF_* (the aux id included, for its CD line search); there is no mixerOpen in
 * its imports. The levels are host/audio/mixer.c's: they scale the effects and the CD music.
 */
#include "win32_host.h"
#include "cd_audio.h"
#include "host/audio/mixer.h"
#include "host/settings/settings.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t t_timeGetTime(const uint32_t* a, int n) { (void)a; (void)n; return w32_ms(); }
static uint32_t t_timePeriod(const uint32_t* a, int n) { (void)a; (void)n; return 0; }
/* timeGetDevCaps(&TIMECAPS {min, max}, size) */
static uint32_t t_timeGetDevCaps(const uint32_t* a, int n)
{
    rt_w32(W(0), 1);
    rt_w32(W(0) + 4, 1000000);
    return 0;
}

/* ---- joysticks ---- */

static int joy_ok = -1;
static SDL_Joystick* joys[16];
static SDL_Joystick* joy(uint32_t id)
{
    if (joy_ok < 0) {
        joy_ok = settings_bool("input.controller", 1) && SDL_InitSubSystem(SDL_INIT_JOYSTICK) == 0;
    }
    if (!joy_ok || id >= 16) return NULL;
    SDL_JoystickUpdate();
    if (!joys[id] && (int)id < SDL_NumJoysticks()) joys[id] = SDL_JoystickOpen((int)id);
    if (joys[id] && !SDL_JoystickGetAttached(joys[id])) { SDL_JoystickClose(joys[id]); joys[id] = NULL; }
    return joys[id];
}
static uint32_t j_joyGetNumDevs(const uint32_t* a, int n) { (void)a; (void)n; return 16; }

/* joyGetDevCapsA(id, &JOYCAPSA, size) */
static uint32_t j_joyGetDevCapsA(const uint32_t* a, int n)
{
    SDL_Joystick* j = joy(W(0));
    if (!j) return 167;
    uint32_t c = W(1), size = W(2) < 0x194 ? W(2) : 0x194;
    memset(rt_ptr(c), 0, size);
    int axes = SDL_JoystickNumAxes(j), nb = SDL_JoystickNumButtons(j);
    if (axes > 6) axes = 6;
    if (nb > 32) nb = 32;
    rt_w16(c, 0x045e);   /* wMid, wPid */
    rt_w16(c + 2, 1);
    w32_put_str(c + 4, SDL_JoystickName(j) ? SDL_JoystickName(j) : "Joystick", 32);
    /* wXmin..wZmax at 0x24, wNumButtons 0x3c, wPeriodMin/Max, then Rmin.. Vmax, wCaps 0x60,
     * wMaxAxes 0x64, wNumAxes 0x68, wMaxButtons 0x6c, szRegKey 0x70, szOEMVxD 0xa0 */
    for (int k = 0; k < 6; k++) rt_w32(c + 0x24 + 4 * (uint32_t)k, k & 1 ? 65535u : 0u);
    rt_w32(c + 0x3c, (uint32_t)nb);
    rt_w32(c + 0x40, 10);
    rt_w32(c + 0x44, 1000);
    for (int k = 0; k < 6; k++) rt_w32(c + 0x48 + 4 * (uint32_t)k, k & 1 ? 65535u : 0u);
    uint32_t caps = 0;
    if (axes > 2) caps |= 1;    /* JOYCAPS_HASZ */
    if (axes > 3) caps |= 2;    /* HASR */
    if (axes > 4) caps |= 4;    /* HASU */
    if (axes > 5) caps |= 8;    /* HASV */
    if (SDL_JoystickNumHats(j) > 0) caps |= 0x10 | 0x40;   /* HASPOV, POVCTS */
    rt_w32(c + 0x60, caps);
    rt_w32(c + 0x64, 6);
    rt_w32(c + 0x68, (uint32_t)axes);
    rt_w32(c + 0x6c, 32);
    return 0;
}

/* joyGetPosEx(id, &JOYINFOEX {size, flags, x, y, z, r, u, v, buttons, number, pov, ...}) */
static uint32_t j_joyGetPosEx(const uint32_t* a, int n)
{
    SDL_Joystick* j = joy(W(0));
    if (!j) return 167;
    uint32_t p = W(1);
    int axes = SDL_JoystickNumAxes(j);
    for (int k = 0; k < 6; k++) {
        int v = k < axes ? SDL_JoystickGetAxis(j, k) + 32768 : 32768;
        rt_w32(p + 8 + 4 * (uint32_t)k, (uint32_t)v);
    }
    uint32_t b = 0, cnt = 0;
    int nb = SDL_JoystickNumButtons(j);
    for (int k = 0; k < nb && k < 32; k++) if (SDL_JoystickGetButton(j, k)) { b |= 1u << k; cnt++; }
    rt_w32(p + 0x20, b);
    rt_w32(p + 0x24, cnt);
    uint32_t pov = 0xffff;
    if (SDL_JoystickNumHats(j) > 0) {
        switch (SDL_JoystickGetHat(j, 0)) {
        case SDL_HAT_UP: pov = 0; break;
        case SDL_HAT_RIGHTUP: pov = 4500; break;
        case SDL_HAT_RIGHT: pov = 9000; break;
        case SDL_HAT_RIGHTDOWN: pov = 13500; break;
        case SDL_HAT_DOWN: pov = 18000; break;
        case SDL_HAT_LEFTDOWN: pov = 22500; break;
        case SDL_HAT_LEFT: pov = 27000; break;
        case SDL_HAT_LEFTUP: pov = 31500; break;
        }
    }
    rt_w32(p + 0x28, pov);
    return 0;
}

/* ---- aux: the CD audio line ---- */
static uint32_t x_auxGetNumDevs(const uint32_t* a, int n) { (void)a; (void)n; return 1; }
/* auxGetDevCapsA(id, &AUXCAPSA, size): wMid, wPid, version, szPname[32] at 8, wTechnology 0x28,
 * dwSupport 0x2c */
static uint32_t x_auxGetDevCapsA(const uint32_t* a, int n)
{
    if (W(0) != 0) return 2;
    uint32_t c = W(1), size = W(2) < 0x30 ? W(2) : 0x30;
    if (!c) return 11;   /* MMSYSERR_INVALPARAM */
    uint8_t caps[0x30];
    memset(caps, 0, sizeof caps);
    caps[0] = 1; caps[2] = 1; caps[4] = 1;
    memcpy(caps + 8, "CD Audio", 8);
    caps[0x28] = 1;      /* AUXCAPS_CDAUDIO */
    caps[0x2c] = 1 | 2;  /* AUXCAPS_VOLUME | AUXCAPS_LRVOLUME */
    memcpy(rt_ptr(c), caps, size);
    return 0;
}
static uint32_t x_auxGetVolume(const uint32_t* a, int n)
{
    if (W(0) != 0) return 2;
    if (!W(1)) return 11;
    rt_w32(W(1), cd_audio_get_volume());
    return 0;
}
static uint32_t x_auxSetVolume(const uint32_t* a, int n)
{
    if (W(0) != 0) return 2;
    cd_audio_set_volume(W(1));
    return 0;
}

/* ---- the mixer ---- */

#define MIXERR_INVALLINE 1024u
#define MIXERR_INVALCONTROL 1025u
#define CT_VOLUME 0x50030001u
#define CT_MUTE 0x20010002u
static const struct { uint32_t id, type, flags, target; const char *shrt, *name; } mlines[MIXER_LINES] = {
    {0xffff0000u, 4, 1, 1, "Volume", "Volume Control"},                 /* DST_SPEAKERS, active, WAVEOUT */
    {0x00000000u, 0x1008, 0x80000001u, 1, "Wave", "Wave"},              /* SRC_WAVEOUT, source */
    {0x00010000u, 0x1005, 0x80000001u, 5, "CD", "CD Audio"},            /* SRC_COMPACTDISC, AUX */
};
static int mx_log(void) { return settings_bool("debug.mixer_log", 0); }

static uint32_t mx_GetNumDevs(const uint32_t* a, int n) { (void)a; (void)n; return 1; }
/* mixerGetDevCapsA(id, &MIXERCAPSA, size): wMid, wPid, version, szPname[32] at 8, fdwSupport
 * 0x28, cDestinations 0x2c */
static uint32_t mx_GetDevCapsA(const uint32_t* a, int n)
{
    if (W(0) != 0) return 2;
    if (!W(1)) return 11;
    uint8_t caps[0x30];
    memset(caps, 0, sizeof caps);
    caps[0] = 1; caps[2] = 1; caps[4] = 1;
    memcpy(caps + 8, "Roadkill Mixer", 14);
    caps[0x2c] = 1;
    memcpy(rt_ptr(W(1)), caps, W(2) < 0x30 ? W(2) : 0x30);
    return 0;
}
static void put_line(uint32_t ml, int k)
{
    rt_w32(ml + 4, 0);                                       /* dwDestination */
    rt_w32(ml + 8, k ? (uint32_t)(k - 1) : 0);               /* dwSource */
    rt_w32(ml + 0xc, mlines[k].id);
    rt_w32(ml + 0x10, mlines[k].flags);
    rt_w32(ml + 0x14, 0);
    rt_w32(ml + 0x18, mlines[k].type);
    rt_w32(ml + 0x1c, 2);                                    /* cChannels */
    rt_w32(ml + 0x20, k ? 0 : 2);                            /* cConnections */
    rt_w32(ml + 0x24, 2);                                    /* cControls */
    memset(rt_ptr(ml + 0x28), 0, 0x80);
    w32_put_str(ml + 0x28, mlines[k].shrt, 16);
    w32_put_str(ml + 0x38, mlines[k].name, 64);
    rt_w32(ml + 0x78, mlines[k].target);                     /* Target.dwType */
    rt_w32(ml + 0x7c, 0);
    rt_w16(ml + 0x80, 1);
    rt_w16(ml + 0x82, 1);
    rt_w32(ml + 0x84, 0x100);
    w32_put_str(ml + 0x88, "Roadkill Mixer", 32);
}
/* mixerGetLineInfoA(id, &MIXERLINEA, flags) */
static uint32_t mx_GetLineInfoA(const uint32_t* a, int n)
{
    uint32_t ml = W(1), how = W(2) & 0xf;
    if (W(0) != 0) return 2;
    if (!ml || rt_r32(ml) < 0xa8) return 11;
    int k = -1;
    switch (how) {
    case 0: if (rt_r32(ml + 4) == 0) k = 0; break;                        /* DESTINATION */
    case 1: if (rt_r32(ml + 4) == 0 && rt_r32(ml + 8) < 2) k = 1 + (int)rt_r32(ml + 8); break;   /* SOURCE */
    case 2: for (int i = 0; i < MIXER_LINES; i++) if (mlines[i].id == rt_r32(ml + 0xc)) k = i; break;   /* LINEID */
    case 3: for (int i = 0; i < MIXER_LINES; i++) if (mlines[i].type == rt_r32(ml + 0x18)) k = i; break;   /* COMPONENTTYPE */
    default: break;
    }
    if (k < 0) return MIXERR_INVALLINE;
    put_line(ml, k);
    return 0;
}
static void put_control(uint32_t mc, int line, int mute)
{
    memset(rt_ptr(mc + 4), 0, 0x90);
    rt_w32(mc, 0x94);
    rt_w32(mc + 4, (uint32_t)(line * 2 + mute));
    rt_w32(mc + 8, mute ? CT_MUTE : CT_VOLUME);
    rt_w32(mc + 0xc, 0);
    w32_put_str(mc + 0x14, mute ? "Mute" : "Volume", 16);
    w32_put_str(mc + 0x24, mute ? "Mute" : "Volume", 64);
    rt_w32(mc + 0x64, 0);
    rt_w32(mc + 0x68, mute ? 1 : 0xffff);
    rt_w32(mc + 0x7c, mute ? 2 : 0xffff);                    /* Metrics.cSteps */
}
/* mixerGetLineControlsA(id, &MIXERLINECONTROLSA {cb, line id, control id / type, count, cbmxctrl,
 * pamxctrl}, flags: ALL 0, ONEBYID 1, ONEBYTYPE 2) */
static uint32_t mx_GetLineControlsA(const uint32_t* a, int n)
{
    uint32_t c = W(1), how = W(2) & 0xf;
    if (W(0) != 0) return 2;
    if (!c) return 11;
    uint32_t arr = rt_r32(c + 0x14);
    if (how == 1) {
        uint32_t id = rt_r32(c + 8);
        if (id >= 2 * MIXER_LINES) return MIXERR_INVALCONTROL;
        put_control(arr, (int)id / 2, (int)id % 2);
        rt_w32(c + 4, mlines[id / 2].id);
        return 0;
    }
    int k = -1;
    for (int i = 0; i < MIXER_LINES; i++) if (mlines[i].id == rt_r32(c + 4)) k = i;
    if (k < 0) return MIXERR_INVALLINE;
    if (how == 0) {
        if (rt_r32(c + 0xc) < 2) return 11;
        put_control(arr, k, 0);
        put_control(arr + rt_r32(c + 0x10), k, 1);
        return 0;
    }
    if (how == 2) {
        uint32_t t = rt_r32(c + 8);
        if (t != CT_VOLUME && t != CT_MUTE) {
            static uint32_t told[4];
            int seen = 0;
            for (int i = 0; i < 4; i++) if (told[i] == t) seen = 1;
            for (int i = 0; i < 4 && !seen; i++) if (!told[i]) { told[i] = t; break; }
            if (mx_log() && !seen) fprintf(stderr, "tm2 mixer: no control of type %08x (bass / treble: not on this mixer; logged once)\n", t);
            return MIXERR_INVALCONTROL;
        }
        put_control(arr, k, t == CT_MUTE);
        return 0;
    }
    return 11;
}
/* MIXERCONTROLDETAILS {cb, control id, channels, items, cbDetails, paDetails}: the control, or -1 */
static int details_control(uint32_t d, uint32_t* ch, uint32_t* pa, uint32_t* cb)
{
    uint32_t id = rt_r32(d + 4);
    if (id >= 2 * MIXER_LINES) return -1;
    *ch = rt_r32(d + 8);
    if (*ch < 1) *ch = 1;
    if (*ch > 2) *ch = 2;
    *pa = rt_r32(d + 0x14);
    *cb = rt_r32(d + 0x10) ? rt_r32(d + 0x10) : 4;
    return (int)id;
}
/* mixerGetControlDetailsA(id, &MIXERCONTROLDETAILS, flags: VALUE 0) */
static uint32_t mx_GetControlDetailsA(const uint32_t* a, int n)
{
    uint32_t ch, pa, cb;
    if (W(0) != 0) return 2;
    if (!W(1) || (W(2) & 0xf) != 0) return 11;
    int id = details_control(W(1), &ch, &pa, &cb);
    if (id < 0) return MIXERR_INVALCONTROL;
    for (uint32_t c = 0; c < ch; c++)
        rt_w32(pa + c * cb, id % 2 ? (uint32_t)mixer_muted(id / 2) : mixer_volume(id / 2, (int)c));
    return 0;
}
/* mixerSetControlDetails(id, &MIXERCONTROLDETAILS, flags: VALUE 0); one channel sets both */
static uint32_t mx_SetControlDetails(const uint32_t* a, int n)
{
    uint32_t ch, pa, cb;
    if (W(0) != 0) return 2;
    if (!W(1) || (W(2) & 0xf) != 0) return 11;
    int id = details_control(W(1), &ch, &pa, &cb);
    if (id < 0) return MIXERR_INVALCONTROL;
    if (id % 2) { mixer_set_mute(id / 2, rt_r32(pa) != 0); return 0; }
    uint32_t l = rt_r32(pa), r = ch > 1 ? rt_r32(pa + cb) : l;
    mixer_set_volume(id / 2, l, r);
    return 0;
}

void rt_bind_win32_mm(void)
{
    rt_bind_import("WINMM.dll!timeGetTime", t_timeGetTime);
    rt_bind_import("WINMM.dll!timeBeginPeriod", t_timePeriod);
    rt_bind_import("WINMM.dll!timeEndPeriod", t_timePeriod);
    rt_bind_import("WINMM.dll!timeGetDevCaps", t_timeGetDevCaps);
    rt_bind_import("WINMM.dll!joyGetNumDevs", j_joyGetNumDevs);
    rt_bind_import("WINMM.dll!joyGetDevCapsA", j_joyGetDevCapsA);
    rt_bind_import("WINMM.dll!joyGetPosEx", j_joyGetPosEx);
    rt_bind_import("WINMM.dll!auxGetNumDevs", x_auxGetNumDevs);
    rt_bind_import("WINMM.dll!auxGetDevCapsA", x_auxGetDevCapsA);
    rt_bind_import("WINMM.dll!auxGetVolume", x_auxGetVolume);
    rt_bind_import("WINMM.dll!auxSetVolume", x_auxSetVolume);
    rt_bind_import("WINMM.dll!mixerGetNumDevs", mx_GetNumDevs);
    rt_bind_import("WINMM.dll!mixerGetDevCapsA", mx_GetDevCapsA);
    rt_bind_import("WINMM.dll!mixerGetLineInfoA", mx_GetLineInfoA);
    rt_bind_import("WINMM.dll!mixerGetLineControlsA", mx_GetLineControlsA);
    rt_bind_import("WINMM.dll!mixerGetControlDetailsA", mx_GetControlDetailsA);
    rt_bind_import("WINMM.dll!mixerSetControlDetails", mx_SetControlDetails);
}

RT_BIND_AREA(win32_mm)
