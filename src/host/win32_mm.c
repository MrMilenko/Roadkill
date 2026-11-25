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
 * STUBS: no mixers (mixerGetNumDevs 0, the rest MMSYSERR_BADDEVICEID): the audio dialog's
 * speaker / wave / CD sliders do nothing.
 */
#include "win32_host.h"
#include "cd_audio.h"
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

/* ---- aux: the CD audio line; mixers: none ---- */
static uint32_t m_zero(const uint32_t* a, int n) { (void)a; (void)n; return 0; }
static uint32_t m_baddev(const uint32_t* a, int n) { (void)a; (void)n; return 2; }   /* MMSYSERR_BADDEVICEID */
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
    rt_bind_import("WINMM.dll!mixerGetNumDevs", m_zero);
    rt_bind_import("WINMM.dll!mixerGetDevCapsA", m_baddev);
    rt_bind_import("WINMM.dll!mixerGetLineInfoA", m_baddev);
    rt_bind_import("WINMM.dll!mixerGetLineControlsA", m_baddev);
    rt_bind_import("WINMM.dll!mixerGetControlDetailsA", m_baddev);
    rt_bind_import("WINMM.dll!mixerSetControlDetails", m_baddev);
}

RT_BIND_AREA(win32_mm)
