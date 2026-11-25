/*
 * input_joy.c: the game controllers behind DirectInput's joystick devices, on SDL2 (port code,
 * host only; not from the original). input_dinput.c turns these into IDirectInputDevice2
 * objects (ranges, dead zones, the data format); this file only reads SDL.
 *
 * Devices: every SDL joystick present at the first call, in SDL's order (no hot plug). One that
 * SDL knows as a GameController is read through the GameController API with the layout below;
 * any other is read raw. TM2_NOJOY=1: no devices (WINMM's joystick functions read the same
 * variable). SDL's background joystick events are allowed (the game decides on its own focus).
 *
 * GameController layout (what a 1997 DirectInput gamepad driver would report):
 *
 *   axis  X   left stick x (right +)        axis  Y   left stick y (down +)
 *   axis  Z   right stick x (right +)       axis  Rz  right stick y (down +)
 *   axis  Rx  left trigger:  centre at rest, + when pulled (only the + half is used)
 *   axis  Ry  right trigger: the same
 *   POV 0     the d-pad, 0 up, 9000 right, 18000 down, 27000 left, diagonals 4500 ..., centred -1
 *   buttons   0 A (south)  1 B (east)   2 X (west)   3 Y (north)
 *             4 left shoulder   5 right shoulder   6 back / select   7 start
 *             8 left stick click   9 right stick click
 *            10 left trigger past half way   11 right trigger past half way
 *            12 d-pad up   13 d-pad down   14 d-pad left   15 d-pad right   16 guide
 *
 *   (The game compares the POV with exactly 0 / 9000 / 18000 / 27000, so a diagonal on the d-pad
 *   counts as no direction there; the d-pad buttons 12..15 have no such gap.)
 *
 * A raw joystick: SDL axes 0..5 -> X, Y, Z, Rx, Ry, Rz, axes 6..7 -> the two sliders, hat 0 ->
 * POV 0, buttons 0..31 as they are.
 *
 * GUIDs: guidInstance is a hash of SDL's device GUID and the copy number among devices with that
 * GUID (stable from run to run while the same controllers are plugged in the same order);
 * guidProduct is DirectInput's {PIDVID-0000-0000-0000-504944564944} with SDL's USB ids.
 *
 * TM2_VIRTUAL_PAD=1 attaches an SDL virtual GameController (SDL 2.0.14+) and puts it first, for
 * unattended runs and tests. TM2_PAD scripts it: "read:control[=value][:hold],..." where `read`
 * counts the device's state reads (GetDeviceState: once per game frame while the game reads the
 * pad), control is a b x y back guide start ls rs lb rb up down left right (buttons) or lx ly rx
 * ry (sticks, value -32768..32767, default 32767) or lt rt (triggers, 0..32767, default 32767),
 * hold = reads until release (default: never released). E.g. TM2_PAD="1:ly=-32767" holds the
 * left stick up from the first read. TM2_PAD_LOG=1 logs the script and every 500th read.
 */
#include "input_joy.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct {
    SDL_Joystick* js;
    SDL_GameController* gc;
    int is_virtual;
    JoyInfo info;
    unsigned reads;
} Joy;

static Joy joys[JOY_MAX];
static int njoy = -1;

/* ---- the virtual pad's script ---- */

typedef struct { unsigned at, hold; int kind, idx, value, state; } PadStep;   /* kind 0 button, 1 axis */
static PadStep steps[64];
static int nsteps;
static int pad_log;

/* the virtual controller's SDL mapping (SDL makes it for SDL_JOYSTICK_TYPE_GAMECONTROLLER):
 * a:b0 b:b1 x:b2 y:b3 back:b4 guide:b5 start:b6 leftstick:b7 rightstick:b8 leftshoulder:b9
 * rightshoulder:b10 dpup:b11 dpdown:b12 dpleft:b13 dpright:b14 leftx:a0 lefty:a1 rightx:a2
 * righty:a3 lefttrigger:a4 righttrigger:a5 */
static const struct { const char* name; int kind, idx; } PAD_CTL[] = {
    {"a", 0, 0}, {"b", 0, 1}, {"x", 0, 2}, {"y", 0, 3}, {"back", 0, 4}, {"guide", 0, 5}, {"start", 0, 6},
    {"ls", 0, 7}, {"rs", 0, 8}, {"lb", 0, 9}, {"rb", 0, 10}, {"up", 0, 11}, {"down", 0, 12}, {"left", 0, 13},
    {"right", 0, 14}, {"lx", 1, 0}, {"ly", 1, 1}, {"rx", 1, 2}, {"ry", 1, 3}, {"lt", 2, 4}, {"rt", 2, 5},
};
#define VP_TRIGGER_REST (-32768)   /* a virtual trigger axis at -32768 reads 0 through the mapping */

static void pad_parse(void)
{
    const char* s = getenv("TM2_PAD");
    pad_log = getenv("TM2_PAD_LOG") != NULL;
    while (s && *s && nsteps < 64) {
        char item[64];
        size_t l = strcspn(s, ",");
        snprintf(item, sizeof item, "%.*s", (int)(l < 63 ? l : 63), s);
        s += l + (s[l] == ',');
        char* ctl = strchr(item, ':');
        if (!ctl) continue;
        *ctl++ = 0;
        char* hold = strchr(ctl, ':');
        if (hold) *hold++ = 0;
        char* val = strchr(ctl, '=');
        if (val) *val++ = 0;
        int k = -1;
        for (size_t i = 0; i < sizeof PAD_CTL / sizeof PAD_CTL[0]; i++)
            if (!strcasecmp(ctl, PAD_CTL[i].name)) k = (int)i;
        if (k < 0) { fprintf(stderr, "tm2 host: TM2_PAD: unknown control %s\n", ctl); continue; }
        PadStep* p = &steps[nsteps++];
        p->at = (unsigned)atoi(item);
        p->hold = hold ? (unsigned)atoi(hold) : 0;
        p->kind = PAD_CTL[k].kind;
        p->idx = PAD_CTL[k].idx;
        int v = val ? atoi(val) : 32767;
        if (p->kind == 2) { v = v * 2 + VP_TRIGGER_REST; p->kind = 1; }   /* 0..32767 -> the raw range */
        p->value = v < -32768 ? -32768 : v > 32767 ? 32767 : v;
        p->state = 0;
        if (pad_log) fprintf(stderr, "tm2 host: pad step at read %u: %s = %d for %u\n", p->at, ctl, p->value, p->hold);
    }
}

static void pad_set(Joy* j, const PadStep* p, int on)
{
#if SDL_VERSION_ATLEAST(2, 0, 14)
    if (p->kind == 0) SDL_JoystickSetVirtualButton(j->js, p->idx, (Uint8)(on ? 1 : 0));
    else SDL_JoystickSetVirtualAxis(j->js, p->idx, (Sint16)(on ? p->value : (p->idx >= 4 ? VP_TRIGGER_REST : 0)));
#else
    (void)j; (void)p; (void)on;
#endif
}

static void pad_run(Joy* j)
{
    unsigned r = j->reads;
    for (int i = 0; i < nsteps; i++) {
        PadStep* p = &steps[i];
        if (p->state == 0 && r >= p->at) {
            pad_set(j, p, 1);
            p->state = 1;
            if (pad_log) fprintf(stderr, "tm2 host: pad read %u: control %d.%d on\n", r, p->kind, p->idx);
        } else if (p->state == 1 && p->hold && r >= p->at + p->hold) {
            pad_set(j, p, 0);
            p->state = 2;
            if (pad_log) fprintf(stderr, "tm2 host: pad read %u: control %d.%d off\n", r, p->kind, p->idx);
        }
    }
}

/* ---- devices ---- */

static uint32_t fnv(const uint8_t* b, size_t n, uint32_t h)
{
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 16777619u; }
    return h;
}

static void make_info(Joy* j, int copy)
{
    JoyInfo* f = &j->info;
    const char* nm = j->gc ? SDL_GameControllerName(j->gc) : SDL_JoystickName(j->js);
    snprintf(f->name, sizeof f->name, "%s", nm && *nm ? nm : "Joystick");
    if (copy) snprintf(f->instance, sizeof f->instance, "%.55s %d", f->name, copy + 1);
    else snprintf(f->instance, sizeof f->instance, "%s", f->name);

    SDL_JoystickGUID g = SDL_JoystickGetGUID(j->js);
    uint8_t key[20];
    memcpy(key, g.data, 16);
    key[16] = (uint8_t)copy; key[17] = 'T'; key[18] = 'M'; key[19] = '2';
    for (int k = 0; k < 4; k++) {
        uint32_t h = fnv(key, sizeof key, 2166136261u + 0x9e3779b9u * (uint32_t)k);
        memcpy(f->guid_instance + 4 * k, &h, 4);   /* little endian hosts: Data1 first */
    }
    f->guid_instance[7] = (uint8_t)((f->guid_instance[7] & 0x0f) | 0x40);   /* "version 4" style */
    f->guid_instance[8] = (uint8_t)((f->guid_instance[8] & 0x3f) | 0x80);
    uint32_t pidvid = (uint32_t)SDL_JoystickGetProduct(j->js) << 16 | SDL_JoystickGetVendor(j->js);
    static const uint8_t TAIL[12] = {0, 0, 0, 0, 0, 0, 'P', 'I', 'D', 'V', 'I', 'D'};
    memcpy(f->guid_product, &pidvid, 4);
    memcpy(f->guid_product + 4, TAIL, 12);

    memset(f->has_axis, 0, sizeof f->has_axis);
    if (j->gc) {
        for (int a = JOY_X; a <= JOY_RZ; a++) f->has_axis[a] = 1;
        f->nbuttons = 17;
        f->npovs = 1;
    } else {
        int na = SDL_JoystickNumAxes(j->js);
        for (int a = 0; a < na && a < JOY_NAXES; a++) f->has_axis[a] = 1;
        f->nbuttons = SDL_JoystickNumButtons(j->js);
        if (f->nbuttons > 32) f->nbuttons = 32;
        if (f->nbuttons < 0) f->nbuttons = 0;
        f->npovs = SDL_JoystickNumHats(j->js) > 0;
    }
    f->gamepad = j->gc != NULL;
}

static int open_index(int di, int is_virtual)
{
    if (njoy >= JOY_MAX) return -1;
    Joy* j = &joys[njoy];
    memset(j, 0, sizeof *j);
    if (SDL_IsGameController(di)) {
        j->gc = SDL_GameControllerOpen(di);
        if (j->gc) j->js = SDL_GameControllerGetJoystick(j->gc);
    }
    if (!j->js) j->js = SDL_JoystickOpen(di);
    if (!j->js) return -1;
    j->is_virtual = is_virtual;
    SDL_JoystickGUID g = SDL_JoystickGetGUID(j->js);
    int copy = 0;
    for (int k = 0; k < njoy; k++)
        if (!memcmp(SDL_JoystickGetGUID(joys[k].js).data, g.data, 16)) copy++;
    make_info(j, copy);
    return njoy++;
}

static void init(void)
{
    if (njoy >= 0) return;
    njoy = 0;
    const char* no = getenv("TM2_NOJOY");
    if (no && *no == '1') return;
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "tm2 host: no joysticks (SDL: %s)\n", SDL_GetError());
        return;
    }
    int vdi = -1;
    const char* vp = getenv("TM2_VIRTUAL_PAD");
    if (vp && *vp == '1') {
#if SDL_VERSION_ATLEAST(2, 0, 14)
        vdi = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 15, 1);
        if (vdi < 0) fprintf(stderr, "tm2 host: TM2_VIRTUAL_PAD: %s\n", SDL_GetError());
        else if (open_index(vdi, 1) == 0) {
            SDL_JoystickSetVirtualAxis(joys[0].js, 4, VP_TRIGGER_REST);
            SDL_JoystickSetVirtualAxis(joys[0].js, 5, VP_TRIGGER_REST);
            pad_parse();
        }
#else
        fprintf(stderr, "tm2 host: TM2_VIRTUAL_PAD needs SDL 2.0.14 or later\n");
#endif
    }
    int n = SDL_NumJoysticks();
    for (int di = 0; di < n; di++)
        if (di != vdi) open_index(di, 0);
    for (int i = 0; i < njoy; i++)
        fprintf(stderr, "tm2 host: joystick %d: %s (%s, %d buttons)\n", i, joys[i].info.instance,
                joys[i].gc ? "game controller" : "raw joystick", joys[i].info.nbuttons);
}

int joy_count(void)
{
    init();
    return njoy;
}

const JoyInfo* joy_info(int i)
{
    init();
    return i >= 0 && i < njoy ? &joys[i].info : NULL;
}

static uint32_t pov_of(int up, int down, int left, int right)
{
    int y = up ? -1 : down ? 1 : 0, x = left ? -1 : right ? 1 : 0;
    static const uint32_t P[3][3] = {{31500, 0, 4500}, {27000, 0xffffffffu, 9000}, {22500, 18000, 13500}};
    return P[y + 1][x + 1];
}

int joy_read(int i, JoyRaw* out)
{
    init();
    memset(out, 0, sizeof *out);
    out->pov = 0xffffffffu;
    if (i < 0 || i >= njoy) return -1;
    Joy* j = &joys[i];
    j->reads++;
    if (j->is_virtual) {
        pad_run(j);
        if (pad_log && (j->reads == 1 || j->reads % 500 == 0)) fprintf(stderr, "tm2 host: pad read %u\n", j->reads);
    }
    SDL_JoystickUpdate();   /* also the GameControllers (SDL 2: the same driver update) */
    if (!SDL_JoystickGetAttached(j->js)) return -1;
    if (j->gc) {
        SDL_GameController* c = j->gc;
        out->axis[JOY_X] = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX);
        out->axis[JOY_Y] = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
        out->axis[JOY_Z] = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTX);
        out->axis[JOY_RZ] = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTY);
        out->axis[JOY_RX] = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
        out->axis[JOY_RY] = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        static const SDL_GameControllerButton B[17] = {
            SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y,
            SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_BUTTON_BACK,
            SDL_CONTROLLER_BUTTON_START, SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK,
            SDL_CONTROLLER_BUTTON_INVALID, SDL_CONTROLLER_BUTTON_INVALID,   /* 10, 11: the triggers */
            SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT,
            SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_CONTROLLER_BUTTON_GUIDE,
        };
        for (int b = 0; b < 17; b++)
            if (B[b] != SDL_CONTROLLER_BUTTON_INVALID) out->button[b] = SDL_GameControllerGetButton(c, B[b]) ? 1 : 0;
        out->button[10] = out->axis[JOY_RX] > 16383;
        out->button[11] = out->axis[JOY_RY] > 16383;
        out->pov = pov_of(out->button[12], out->button[13], out->button[14], out->button[15]);
    } else {
        for (int a = 0; a < JOY_NAXES; a++)
            if (j->info.has_axis[a]) out->axis[a] = SDL_JoystickGetAxis(j->js, a);
        for (int b = 0; b < j->info.nbuttons; b++) out->button[b] = SDL_JoystickGetButton(j->js, b) ? 1 : 0;
        if (j->info.npovs) {
            Uint8 h = SDL_JoystickGetHat(j->js, 0);
            out->pov = pov_of(h & SDL_HAT_UP, h & SDL_HAT_DOWN, h & SDL_HAT_LEFT, h & SDL_HAT_RIGHT);
        }
    }
    return 0;
}
