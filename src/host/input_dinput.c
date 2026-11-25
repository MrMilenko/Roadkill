/*
 * input_dinput.c: DirectInput 5 for the game on SDL2 (port code, not from the original).
 *
 *   DirectInputCreateA (DINPUT.dll import slot and the thunk 0x004d1914) -> IDirectInputA. The
 *   game reaches the objects only through their vtables in game memory (en5_dinput.c 0x0049e110,
 *   en5_keys.c 0x0049e638, en5_kbdev.c 0x004a3964, en7_b.c 0x004a7be0, en4_obj.c).
 *   CreateDevice(GUID_SysKeyboard) -> a keyboard: GetDeviceState gives the 256 DIK states from
 *   SDL_GetKeyboardState, GetDeviceData the presses and releases since the last call (found by
 *   comparing the SDL state with the last one seen: the Win32 host's message loop pumps SDL).
 *
 *   Joysticks: the game controllers of input_joy.c (its header has the button / axis mapping;
 *   TM2_NOJOY=1: none, TM2_VIRTUAL_PAD=1: a scripted virtual pad). The game's path (en7_b.c
 *   0x004a7be0, 0x004a8284, en7_i.c 0x004a7dbc, en5_joyobj.c 0x004a8330, en4_obj.c 0x004a8018):
 *     EnumDevices(DIDEVTYPE_JOYSTICK, cb, ref, DIEDFL_ATTACHEDONLY): cb(DIDEVICEINSTANCEA, ref)
 *       per controller (dwSize 0x244: guidInstance, guidProduct, dwDevType, names); types 0 (all)
 *       and 4 list the controllers, other types and DIEDFL_FORCEFEEDBACK list none.
 *     CreateDevice(&guidInstance) -> an IDirectInputDevice(2)A (QueryInterface answers Device2).
 *     SetDataFormat(DIDATAFORMAT): any format, matched as DirectInput does (each entry takes the
 *       first unassigned device object of its type, GUID and instance; non optional entries
 *       with no object fail); the game passes c_dfDIJoystick (0x004d1380, DIJOYSTATE 0x50).
 *     GetCapabilities: DIDEVCAPS, attached, DIDEVTYPE_JOYSTICK | DIDEVTYPE_HID, subtype gamepad
 *       (4) for a GameController (the game then gives X / Y the range -1..1), traditional (2)
 *       for a raw joystick; axes, buttons, POVs.
 *     EnumObjects(cb, ref, flags): cb(DIDEVICEOBJECTINSTANCEA 0x13c, ref) per axis (instances 0..,
 *       GUID_XAxis .. GUID_Slider, DIDFT_ABSAXIS), button (DIDFT_PSHBUTTON) and POV (DIDFT_POV);
 *       dwOfs is the object's offset in the format set.
 *     Set / GetProperty: DIPROP_RANGE, DIPROP_DEADZONE, DIPROP_SATURATION per object (DIPH_BYOFFSET,
 *       DIPH_BYID) or for all axes (DIPH_DEVICE), DIPROP_BUFFERSIZE; GetProperty also
 *       DIPROP_GRANULARITY (1) and DIPROP_AXISMODE (absolute). Defaults: 0..65535, no dead zone,
 *       saturation 10000, as DirectInput.
 *     Acquire / Unacquire, Poll (DI_OK), GetDeviceState (the format's layout: axes scaled into
 *       their range through dead zone and saturation as DirectInput does, buttons 0x80, POVs in
 *       hundredths of degrees or -1, format POV slots with no POV -1), GetDeviceData (buffered:
 *       changes since the last look, by format offset), GetDeviceInfo, GetObjectInfo.
 *     A controller unplugged after start reads centred (no DIERR_INPUTLOST: the game's answer to
 *     that is a re acquire, then an error box when it fails).
 *   Other devices (the system mouse) are refused with DIERR_DEVICENOTREG.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "dx_com.h"
#include "input_joy.h"

#define DIERR_NOTACQUIRED  0x8007000cu
#define DIERR_DEVICENOTREG 0x80040154u
#define DIERR_ALREADYINITIALIZED 0x800704dfu
#define DI_BUFFEROVERFLOW  1u
#define DI_NOEFFECT        1u

static DxIface I_DI = {"IDirectInputA", 0, {0x89521360u, 0x89521361u}};
/* IDirectInputDeviceA / W, IDirectInputDevice2A / W */
static DxIface I_DID = {"IDirectInputDeviceA", 0, {0x5944e680u, 0x5944e681u, 0x5944e682u, 0x5944e683u}};

#define DIERR_INPUTLOST    0x8007001eu
#define DIERR_ACQUIRED     0x800700aau
#define DIERR_OBJECTNOTFOUND 0x80070002u
#define DIERR_NOTINITIALIZED 0x80070015u
#define DIERR_INVALIDPARAM_NOFORMAT DX_E_INVALIDARG   /* Acquire before SetDataFormat */

/* a joystick's device object (axis, button or POV) */
enum { OB_AXIS, OB_BUTTON, OB_POV };
typedef struct JObj {
    int kind;                     /* OB_* */
    int inst;                     /* DIDFT_MAKEINSTANCE: axes 0.., buttons 0.., POVs 0.. */
    int src;                      /* JOY_X.. / button index / POV index */
    int32_t ofs;                  /* offset in the data format, -1 when the format has no slot */
    int32_t lo, hi;               /* DIPROP_RANGE */
    uint32_t dz, sat;             /* DIPROP_DEADZONE / DIPROP_SATURATION, 0..10000 */
    int32_t last;                 /* the value GetDeviceData last reported */
} JObj;

typedef struct Dev {
    int joy;                      /* -1 the keyboard, else the input_joy.c index */
    JObj obj[JOY_NAXES + 32 + 1];
    int nobj;
    uint32_t fmt_size;            /* the data format's dwDataSize, 0 before SetDataFormat */
    uint32_t pov_slots[4];        /* format POV offsets with no POV (read as -1) */
    int npov_slots;
    int acquired;
    uint32_t bufsize;             /* DIPROP_BUFFERSIZE */
    uint8_t last[256];            /* the states last reported by GetDeviceData */
    struct { uint32_t ofs, data, time, seq; } q[256];
    int nq;
    uint32_t seq;
    int overflow;
} Dev;

/* SDL scancode -> DIK code (PC set 1; 0x80 bit = the E0 extended keys) */
static const struct { uint16_t sdl; uint8_t dik; } KEYS[] = {
    {SDL_SCANCODE_ESCAPE, 0x01}, {SDL_SCANCODE_1, 0x02}, {SDL_SCANCODE_2, 0x03}, {SDL_SCANCODE_3, 0x04},
    {SDL_SCANCODE_4, 0x05}, {SDL_SCANCODE_5, 0x06}, {SDL_SCANCODE_6, 0x07}, {SDL_SCANCODE_7, 0x08},
    {SDL_SCANCODE_8, 0x09}, {SDL_SCANCODE_9, 0x0a}, {SDL_SCANCODE_0, 0x0b}, {SDL_SCANCODE_MINUS, 0x0c},
    {SDL_SCANCODE_EQUALS, 0x0d}, {SDL_SCANCODE_BACKSPACE, 0x0e}, {SDL_SCANCODE_TAB, 0x0f},
    {SDL_SCANCODE_Q, 0x10}, {SDL_SCANCODE_W, 0x11}, {SDL_SCANCODE_E, 0x12}, {SDL_SCANCODE_R, 0x13},
    {SDL_SCANCODE_T, 0x14}, {SDL_SCANCODE_Y, 0x15}, {SDL_SCANCODE_U, 0x16}, {SDL_SCANCODE_I, 0x17},
    {SDL_SCANCODE_O, 0x18}, {SDL_SCANCODE_P, 0x19}, {SDL_SCANCODE_LEFTBRACKET, 0x1a},
    {SDL_SCANCODE_RIGHTBRACKET, 0x1b}, {SDL_SCANCODE_RETURN, 0x1c}, {SDL_SCANCODE_LCTRL, 0x1d},
    {SDL_SCANCODE_A, 0x1e}, {SDL_SCANCODE_S, 0x1f}, {SDL_SCANCODE_D, 0x20}, {SDL_SCANCODE_F, 0x21},
    {SDL_SCANCODE_G, 0x22}, {SDL_SCANCODE_H, 0x23}, {SDL_SCANCODE_J, 0x24}, {SDL_SCANCODE_K, 0x25},
    {SDL_SCANCODE_L, 0x26}, {SDL_SCANCODE_SEMICOLON, 0x27}, {SDL_SCANCODE_APOSTROPHE, 0x28},
    {SDL_SCANCODE_GRAVE, 0x29}, {SDL_SCANCODE_LSHIFT, 0x2a}, {SDL_SCANCODE_BACKSLASH, 0x2b},
    {SDL_SCANCODE_Z, 0x2c}, {SDL_SCANCODE_X, 0x2d}, {SDL_SCANCODE_C, 0x2e}, {SDL_SCANCODE_V, 0x2f},
    {SDL_SCANCODE_B, 0x30}, {SDL_SCANCODE_N, 0x31}, {SDL_SCANCODE_M, 0x32}, {SDL_SCANCODE_COMMA, 0x33},
    {SDL_SCANCODE_PERIOD, 0x34}, {SDL_SCANCODE_SLASH, 0x35}, {SDL_SCANCODE_RSHIFT, 0x36},
    {SDL_SCANCODE_KP_MULTIPLY, 0x37}, {SDL_SCANCODE_LALT, 0x38}, {SDL_SCANCODE_SPACE, 0x39},
    {SDL_SCANCODE_CAPSLOCK, 0x3a}, {SDL_SCANCODE_F1, 0x3b}, {SDL_SCANCODE_F2, 0x3c}, {SDL_SCANCODE_F3, 0x3d},
    {SDL_SCANCODE_F4, 0x3e}, {SDL_SCANCODE_F5, 0x3f}, {SDL_SCANCODE_F6, 0x40}, {SDL_SCANCODE_F7, 0x41},
    {SDL_SCANCODE_F8, 0x42}, {SDL_SCANCODE_F9, 0x43}, {SDL_SCANCODE_F10, 0x44},
    {SDL_SCANCODE_NUMLOCKCLEAR, 0x45}, {SDL_SCANCODE_SCROLLLOCK, 0x46}, {SDL_SCANCODE_KP_7, 0x47},
    {SDL_SCANCODE_KP_8, 0x48}, {SDL_SCANCODE_KP_9, 0x49}, {SDL_SCANCODE_KP_MINUS, 0x4a},
    {SDL_SCANCODE_KP_4, 0x4b}, {SDL_SCANCODE_KP_5, 0x4c}, {SDL_SCANCODE_KP_6, 0x4d},
    {SDL_SCANCODE_KP_PLUS, 0x4e}, {SDL_SCANCODE_KP_1, 0x4f}, {SDL_SCANCODE_KP_2, 0x50},
    {SDL_SCANCODE_KP_3, 0x51}, {SDL_SCANCODE_KP_0, 0x52}, {SDL_SCANCODE_KP_PERIOD, 0x53},
    {SDL_SCANCODE_NONUSBACKSLASH, 0x56}, {SDL_SCANCODE_F11, 0x57}, {SDL_SCANCODE_F12, 0x58},
    {SDL_SCANCODE_KP_ENTER, 0x9c}, {SDL_SCANCODE_RCTRL, 0x9d}, {SDL_SCANCODE_KP_DIVIDE, 0xb5},
    {SDL_SCANCODE_PRINTSCREEN, 0xb7}, {SDL_SCANCODE_RALT, 0xb8}, {SDL_SCANCODE_PAUSE, 0xc5},
    {SDL_SCANCODE_HOME, 0xc7}, {SDL_SCANCODE_UP, 0xc8}, {SDL_SCANCODE_PAGEUP, 0xc9},
    {SDL_SCANCODE_LEFT, 0xcb}, {SDL_SCANCODE_RIGHT, 0xcd}, {SDL_SCANCODE_END, 0xcf},
    {SDL_SCANCODE_DOWN, 0xd0}, {SDL_SCANCODE_PAGEDOWN, 0xd1}, {SDL_SCANCODE_INSERT, 0xd2},
    {SDL_SCANCODE_DELETE, 0xd3}, {SDL_SCANCODE_LGUI, 0xdb}, {SDL_SCANCODE_RGUI, 0xdc},
    {SDL_SCANCODE_APPLICATION, 0xdd},
};

const uint8_t* host_keyboard_state(int* n);   /* win32_keys.c: SDL's state with scripted keys */

/* the current DIK states (0x80 = down) */
static void keyboard_state(uint8_t st[256])
{
    memset(st, 0, 256);
    int nk = 0;
    const uint8_t* k = host_keyboard_state(&nk);
    if (!k) return;
    for (size_t i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++)
        if (KEYS[i].sdl < nk && k[KEYS[i].sdl]) st[KEYS[i].dik] = 0x80;
}

/* the changes since the last look into the queue (DIPROP_BUFFERSIZE bounds it) */
static void collect(Dev* d)
{
    uint8_t st[256];
    keyboard_state(st);
    uint32_t now = SDL_GetTicks();
    uint32_t cap = d->bufsize < 256 ? d->bufsize : 256;
    for (int k = 0; k < 256; k++) {
        if (st[k] == d->last[k]) continue;
        d->last[k] = st[k];
        if ((uint32_t)d->nq >= cap) { d->overflow = 1; continue; }
        d->q[d->nq].ofs = (uint32_t)k;
        d->q[d->nq].data = st[k];
        d->q[d->nq].time = now;
        d->q[d->nq].seq = ++d->seq;
        d->nq++;
    }
}

static Dev* dev_of(uint32_t a) { DxObj* o = dx_obj_get(a, &I_DID); return o ? o->host : NULL; }
static void dev_destroy(DxObj* o) { free(o->host); }

/* ---- joysticks ---- */

/* the DirectInput object type GUIDs {A36D02xx-C9F3-11CF-BFC7-444553540000} by their first byte */
enum { G_X = 0xe0, G_Y = 0xe1, G_Z = 0xe2, G_RZ = 0xe3, G_SLIDER = 0xe4, G_BUTTON = 0xf0, G_POV = 0xf2,
       G_RX = 0xf4, G_RY = 0xf5 };
static void guid_obj(uint8_t g[16], int b)
{
    static const uint8_t T[16] = {0, 0x02, 0x6d, 0xa3, 0xf3, 0xc9, 0xcf, 0x11,
                                  0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00};
    memcpy(g, T, 16);
    g[0] = (uint8_t)b;
}
static const int AXIS_GUID[JOY_NAXES] = {G_X, G_Y, G_Z, G_RX, G_RY, G_RZ, G_SLIDER, G_SLIDER};
static const char* const AXIS_NAME[JOY_NAXES] = {"X Axis", "Y Axis", "Z Axis", "X Rotation", "Y Rotation",
                                                  "Z Rotation", "Slider", "Slider 2"};
/* c_dfDIJoystick's offsets, used before a format is set */
static const int32_t AXIS_OFS[JOY_NAXES] = {0, 4, 8, 0x0c, 0x10, 0x14, 0x18, 0x1c};

static int obj_guid(const JObj* o) { return o->kind == OB_AXIS ? AXIS_GUID[o->src] : o->kind == OB_BUTTON ? G_BUTTON : G_POV; }
static uint32_t obj_type(const JObj* o)   /* DIDFT_ABSAXIS / DIDFT_PSHBUTTON / DIDFT_POV | instance */
{
    return (o->kind == OB_AXIS ? 2u : o->kind == OB_BUTTON ? 4u : 0x10u) | (uint32_t)o->inst << 8;
}

static void joy_objects(Dev* d)
{
    const JoyInfo* f = joy_info(d->joy);
    int ai = 0;
    d->nobj = 0;
    for (int a = 0; a < JOY_NAXES; a++) {
        if (!f->has_axis[a]) continue;
        JObj* o = &d->obj[d->nobj++];
        *o = (JObj){OB_AXIS, ai++, a, AXIS_OFS[a], 0, 65535, 0, 10000, 0};
    }
    for (int b = 0; b < f->nbuttons; b++) d->obj[d->nobj++] = (JObj){OB_BUTTON, b, b, 0x30 + b, 0, 0, 0, 0, 0};
    for (int p = 0; p < f->npovs; p++) d->obj[d->nobj++] = (JObj){OB_POV, p, p, 0x20 + 4 * p, 0, 0, 0, 0, 0};
}

/* an axis value: raw -32768..32767 through dead zone and saturation into lo..hi */
static int32_t axis_scale(const JObj* o, int32_t raw)
{
    double v = raw / 32767.0;
    if (v > 1) v = 1;
    if (v < -1) v = -1;
    double m = (v < 0 ? -v : v) * 10000.0;
    if (m <= o->dz) v = 0;
    else if (m >= o->sat || o->sat <= o->dz) v = v < 0 ? -1 : 1;
    else v = (v < 0 ? -1 : 1) * (m - o->dz) / (double)(o->sat - o->dz);
    double x = o->lo + (v + 1) * ((double)o->hi - o->lo) / 2;
    return (int32_t)(x < 0 ? x - 0.5 : x + 0.5);
}

static int32_t obj_value(const JObj* o, const JoyRaw* r)
{
    if (o->kind == OB_AXIS) return axis_scale(o, r->axis[o->src]);
    if (o->kind == OB_BUTTON) return r->button[o->src] ? 0x80 : 0;
    return (int32_t)r->pov;
}

static void joy_raw(Dev* d, JoyRaw* r)
{
    if (joy_read(d->joy, r) != 0) {
        dx_log_once("DirectInput: joystick %d unplugged: reads centred", d->joy);
        memset(r, 0, sizeof *r);
        r->pov = 0xffffffffu;
    }
}

/* DIDEVICEINSTANCEA (dwSize 0x244, or the DirectX 3 0x22c) of controller i at p */
static void write_instance(uint32_t p, int i, uint32_t size)
{
    const JoyInfo* f = joy_info(i);
    memset(rt_ptr(p + 4), 0, size - 4);
    rt_w32(p, size);
    memcpy(rt_ptr(p + 4), f->guid_instance, 16);
    memcpy(rt_ptr(p + 0x14), f->guid_product, 16);
    rt_w32(p + 0x24, 4u | (f->gamepad ? 4u : 2u) << 8 | 0x10000u);   /* JOYSTICK, subtype, HID */
    snprintf((char*)rt_ptr(p + 0x28), 260, "%s", f->instance);
    snprintf((char*)rt_ptr(p + 0x12c), 260, "%s", f->name);
    if (size >= 0x244) {
        rt_w16(p + 0x240, 1);                    /* HID usage page generic desktop */
        rt_w16(p + 0x242, f->gamepad ? 5 : 4);   /* game pad / joystick */
    }
}

/* DIDEVICEOBJECTINSTANCEA (dwSize 0x13c, or 0x124) of object o at p */
static void write_object(uint32_t p, const JObj* o, uint32_t size)
{
    memset(rt_ptr(p + 4), 0, size - 4);
    rt_w32(p, size);
    guid_obj(rt_ptr(p + 4), obj_guid(o));
    rt_w32(p + 0x14, o->ofs >= 0 ? (uint32_t)o->ofs : 0);
    rt_w32(p + 0x18, obj_type(o));
    rt_w32(p + 0x1c, o->kind == OB_AXIS ? 0x100u : 0);   /* DIDOI_ASPECTPOSITION */
    char* nm = (char*)rt_ptr(p + 0x20);
    if (o->kind == OB_AXIS) snprintf(nm, 260, "%s", AXIS_NAME[o->src]);
    else if (o->kind == OB_BUTTON) snprintf(nm, 260, "Button %d", o->inst);
    else snprintf(nm, 260, "Hat Switch");
    if (size >= 0x13c) {
        static const uint16_t USAGE[JOY_NAXES] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x36};
        rt_w16(p + 0x130, o->kind == OB_BUTTON ? 9 : 1);
        rt_w16(p + 0x132, o->kind == OB_AXIS ? USAGE[o->src] : o->kind == OB_BUTTON ? (uint16_t)(o->inst + 1) : 0x39);
    }
}

/* the object a DIPROPHEADER (+8 dwObj, +0x0c dwHow) or GetObjectInfo names */
static JObj* find_obj(Dev* d, uint32_t obj, uint32_t how)
{
    for (int k = 0; k < d->nobj; k++) {
        JObj* o = &d->obj[k];
        if (how == 1 && o->ofs >= 0 && (uint32_t)o->ofs == obj) return o;            /* DIPH_BYOFFSET */
        if (how == 2 && obj_type(o) == (obj & 0xffffffu)) return o;                    /* DIPH_BYID */
    }
    return NULL;
}

/* the changes in r since the last look into the queue (DIPROP_BUFFERSIZE bounds it) */
static void joy_queue(Dev* d, const JoyRaw* r)
{
    uint32_t now = SDL_GetTicks();
    uint32_t cap = d->bufsize < 256 ? d->bufsize : 256;
    for (int k = 0; k < d->nobj; k++) {
        JObj* o = &d->obj[k];
        if (o->ofs < 0) continue;
        int32_t v = obj_value(o, r);
        if (v == o->last) continue;
        o->last = v;
        if ((uint32_t)d->nq >= cap) { d->overflow = 1; continue; }
        d->q[d->nq].ofs = (uint32_t)o->ofs;
        d->q[d->nq].data = (uint32_t)v;
        d->q[d->nq].time = now;
        d->q[d->nq].seq = ++d->seq;
        d->nq++;
    }
}

/* the joystick state in the format's layout; every read also feeds the buffer when there is one */
static void joy_state(Dev* d, uint8_t* buf, uint32_t size)
{
    JoyRaw r;
    joy_raw(d, &r);
    if (d->bufsize) joy_queue(d, &r);
    memset(buf, 0, size);
    for (int k = 0; k < d->npov_slots; k++)
        if (d->pov_slots[k] + 4 <= size) memset(buf + d->pov_slots[k], 0xff, 4);
    for (int k = 0; k < d->nobj; k++) {
        const JObj* o = &d->obj[k];
        if (o->ofs < 0) continue;
        int32_t v = obj_value(o, &r);
        if (o->kind == OB_BUTTON) { if ((uint32_t)o->ofs < size) buf[o->ofs] = (uint8_t)v; }
        else if ((uint32_t)o->ofs + 4 <= size) memcpy(buf + o->ofs, &v, 4);
    }
}

static void joy_collect(Dev* d)
{
    JoyRaw r;
    joy_raw(d, &r);
    joy_queue(d, &r);
}

static void joy_mark(Dev* d)   /* the current values as already reported */
{
    JoyRaw r;
    joy_raw(d, &r);
    for (int k = 0; k < d->nobj; k++) d->obj[k].last = obj_value(&d->obj[k], &r);
    d->nq = 0;
}

/* GetCapabilities: DIDEVCAPS (0x18 DirectX 3, 0x2c DirectX 5) */
static uint32_t j_getcaps(Dev* d, uint32_t c)
{
    uint32_t size = rt_r32(c);
    if (size < 0x18) return DX_E_INVALIDARG;
    const JoyInfo* f = joy_info(d->joy);
    int na = 0;
    for (int k = 0; k < JOY_NAXES; k++) na += f->has_axis[k];
    memset(rt_ptr(c + 4), 0, (size > 0x2c ? 0x2c : size) - 4);
    rt_w32(c + 4, 1);   /* DIDC_ATTACHED */
    rt_w32(c + 8, 4u | (f->gamepad ? 4u : 2u) << 8 | 0x10000u);
    rt_w32(c + 0x0c, (uint32_t)na);
    rt_w32(c + 0x10, (uint32_t)f->nbuttons);
    rt_w32(c + 0x14, (uint32_t)f->npovs);
    return DX_OK;
}

/* SetDataFormat: each entry {pguid, dwOfs, dwType, dwFlags} takes the first unassigned object of
 * its type (and GUID, and instance unless DIDFT_ANYINSTANCE) */
static uint32_t j_setformat(Dev* d, uint32_t f)
{
    uint32_t objsize = rt_r32(f + 4), datasize = rt_r32(f + 0x0c), nobjs = rt_r32(f + 0x10), odf = rt_r32(f + 0x14);
    if (rt_r32(f) != 0x18 || objsize < 0x10 || !datasize || datasize > 0x400 || (datasize & 3) || (nobjs && !odf))
        return DX_E_INVALIDARG;
    int taken[JOY_NAXES + 33] = {0};
    int32_t ofs[JOY_NAXES + 33];
    uint32_t pov_slots[4];
    int npov = 0;
    for (uint32_t i = 0; i < nobjs; i++) {
        uint32_t e = odf + i * objsize, pg = rt_r32(e), at = rt_r32(e + 4), ty = rt_r32(e + 8);
        uint32_t inst = ty >> 8 & 0xffff;
        int found = -1;
        for (int k = 0; k < d->nobj && found < 0; k++) {
            const JObj* o = &d->obj[k];
            if (taken[k] || !(obj_type(o) & ty & 0x1f)) continue;
            if (inst != 0xffff && (uint32_t)o->inst != inst) continue;
            if (pg) {
                uint8_t g[16];
                guid_obj(g, obj_guid(o));
                if (memcmp(g, rt_ptr(pg), 16) != 0) continue;
            }
            found = k;
        }
        uint32_t width = ty & 0x0c ? 1 : 4;   /* buttons are bytes, axes and POVs dwords */
        if (at + width > datasize) return DX_E_INVALIDARG;
        if (found < 0) {
            if (!(ty & 0x80000000u)) return DX_E_INVALIDARG;   /* not DIDFT_OPTIONAL */
            if ((ty & 0x10) && npov < 4) pov_slots[npov++] = at;
            continue;
        }
        taken[found] = 1;
        ofs[found] = (int32_t)at;
    }
    for (int k = 0; k < d->nobj; k++) d->obj[k].ofs = taken[k] ? ofs[k] : -1;
    memcpy(d->pov_slots, pov_slots, sizeof pov_slots);
    d->npov_slots = npov;
    d->fmt_size = datasize;
    return DX_OK;
}

/* DIPROPHEADER: +0 dwSize, +4 dwHeaderSize, +8 dwObj, +0x0c dwHow; data from +0x10 */
static uint32_t j_setproperty(Dev* d, uint32_t prop, uint32_t h)
{
    if (prop == 1) { if (h) d->bufsize = rt_r32(h + 0x10); return DX_OK; }   /* DIPROP_BUFFERSIZE */
    if (!h || rt_r32(h + 4) != 0x10) return DX_E_INVALIDARG;
    if (prop != 4 && prop != 5 && prop != 6) return prop == 2 ? DX_OK : DX_E_NOTIMPL;   /* AXISMODE: absolute only */
    uint32_t how = rt_r32(h + 0x0c);
    JObj* one = NULL;
    if (how != 0) {   /* DIPH_DEVICE: every axis */
        one = find_obj(d, rt_r32(h + 8), how);
        if (!one || one->kind != OB_AXIS) return DIERR_OBJECTNOTFOUND;
    }
    int32_t lo = (int32_t)rt_r32(h + 0x10), hi = (int32_t)rt_r32(h + 0x14);
    uint32_t v = rt_r32(h + 0x10);
    if (prop == 4 && lo >= hi) return DX_E_INVALIDARG;
    if (prop != 4 && v > 10000) return DX_E_INVALIDARG;
    for (int k = 0; k < d->nobj; k++) {
        JObj* o = &d->obj[k];
        if (o->kind != OB_AXIS || (one && o != one)) continue;
        if (prop == 4) { o->lo = lo; o->hi = hi; }
        else if (prop == 5) o->dz = v;
        else o->sat = v;
    }
    return DX_OK;
}

static uint32_t j_getproperty(Dev* d, uint32_t prop, uint32_t h)
{
    if (prop == 1) { rt_w32(h + 0x10, d->bufsize); return DX_OK; }
    uint32_t how = rt_r32(h + 0x0c);
    JObj* o = NULL;
    if (how != 0) o = find_obj(d, rt_r32(h + 8), how);
    else for (int k = 0; k < d->nobj && !o; k++) if (d->obj[k].kind == OB_AXIS) o = &d->obj[k];
    if (prop == 2) { rt_w32(h + 0x10, 0); return DX_OK; }   /* DIPROPAXISMODE_ABS */
    if (!o) return DIERR_OBJECTNOTFOUND;
    if (prop == 3) { rt_w32(h + 0x10, 1); return DX_OK; }   /* DIPROP_GRANULARITY */
    if (o->kind != OB_AXIS) return DIERR_OBJECTNOTFOUND;
    if (prop == 4) { rt_w32(h + 0x10, (uint32_t)o->lo); rt_w32(h + 0x14, (uint32_t)o->hi); return DX_OK; }
    if (prop == 5) { rt_w32(h + 0x10, o->dz); return DX_OK; }
    if (prop == 6) { rt_w32(h + 0x10, o->sat); return DX_OK; }
    return DX_E_NOTIMPL;
}

/* ---- IDirectInputA ---- */

/* 0x0c CreateDevice(GUID*, out, outer) */
static uint32_t di_create_device(const uint32_t* a, int n)
{
    uint32_t guid = dx_arg(a, n, 1), out = dx_arg(a, n, 2);
    if (!guid || !out) return DX_E_INVALIDARG;
    rt_w32(out, 0);
    /* GUID_SysKeyboard {6F1D2B61-D5A0-11CF-BFC7-444553540000} */
    static const uint8_t KBD[16] = {0x61, 0x2b, 0x1d, 0x6f, 0xa0, 0xd5, 0xcf, 0x11,
                                    0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00};
    int joy = -1;
    if (memcmp(rt_ptr(guid), KBD, 16) != 0) {
        for (int i = 0; i < joy_count(); i++)
            if (!memcmp(rt_ptr(guid), joy_info(i)->guid_instance, 16)) joy = i;
        if (joy < 0) {
            dx_log_once("DirectInput: device %08x refused (keyboard and joysticks only)", rt_r32(guid));
            return DIERR_DEVICENOTREG;
        }
    }
    Dev* d = calloc(1, sizeof *d);
    DxObj* o = d ? dx_obj_new(&I_DID, d, dev_destroy) : NULL;
    if (!o) { free(d); return 0x8007000eu; }
    d->joy = joy;
    if (joy >= 0) joy_objects(d);
    rt_w32(out, o->addr);
    return DX_OK;
}

/* 0x10 EnumDevices(type, callback(DIDEVICEINSTANCEA*, context), context, flags): the joysticks */
static uint32_t di_enum_devices(const uint32_t* a, int n)
{
    uint32_t type = dx_arg(a, n, 1) & 0xff, cb = dx_arg(a, n, 2), ctx = dx_arg(a, n, 3), flags = dx_arg(a, n, 4);
    if (!cb) return DX_E_INVALIDARG;
    if ((type != 0 && type != 4) || (flags & 0x100)) return DX_OK;   /* DIEDFL_FORCEFEEDBACK: none */
    int nj = joy_count();
    dx_log_once("DirectInput: EnumDevices type %u: %d joysticks", type, nj);
    if (!nj) return DX_OK;
    uint32_t ddi = rt_frame_push(0x244);
    for (int i = 0; i < nj; i++) {
        write_instance(ddi, i, 0x244);
        uint32_t w[2] = {ddi, ctx};
        if (rt_call(cb, w, 2) == 0) break;   /* DIENUM_STOP */
    }
    rt_frame_pop(0x244);
    return DX_OK;
}

static uint32_t di_get_device_status(const uint32_t* a, int n) { (void)a; (void)n; return DX_OK; }
static uint32_t di_ok(const uint32_t* a, int n) { (void)a; (void)n; return DX_OK; }
static uint32_t di_initialize(const uint32_t* a, int n) { (void)a; (void)n; return DX_OK; }

/* ---- IDirectInputDeviceA (the keyboard; joysticks go to the j_ functions) ---- */

static uint32_t d_getcaps(const uint32_t* a, int n)   /* 0x0c GetCapabilities(DIDEVCAPS*) */
{
    uint32_t c = dx_arg(a, n, 1);
    Dev* d = dev_of(dx_arg(a, n, 0));
    if (!d || !c) return DX_E_INVALIDARG;
    if (d->joy >= 0) return j_getcaps(d, c);
    uint32_t size = rt_r32(c);
    if (size >= 0x18) {
        memset(rt_ptr(c + 4), 0, size - 4);
        rt_w32(c + 4, 1);              /* DIDC_ATTACHED */
        rt_w32(c + 8, 3u | 4u << 8);   /* DIDEVTYPE_KEYBOARD, DIDEVTYPEKEYBOARD_PCENH */
        rt_w32(c + 0x10, 128);         /* dwButtons */
    }
    return DX_OK;
}

static uint32_t d_setproperty(const uint32_t* a, int n)   /* 0x18 SetProperty(guid, DIPROPHEADER*) */
{
    Dev* d = dev_of(dx_arg(a, n, 0));
    uint32_t prop = dx_arg(a, n, 1), h = dx_arg(a, n, 2);
    if (!d) return DX_E_INVALIDARG;
    if (d->joy >= 0) return j_setproperty(d, prop, h);
    if (prop == 1 && h) d->bufsize = rt_r32(h + 0x10);   /* DIPROP_BUFFERSIZE: DIPROPDWORD dwData */
    return DX_OK;
}

static uint32_t d_getproperty(const uint32_t* a, int n)   /* 0x14 GetProperty(guid, DIPROPHEADER*) */
{
    Dev* d = dev_of(dx_arg(a, n, 0));
    uint32_t prop = dx_arg(a, n, 1), h = dx_arg(a, n, 2);
    if (!d || !h) return DX_E_INVALIDARG;
    if (d->joy >= 0) return j_getproperty(d, prop, h);
    if (prop == 1) { rt_w32(h + 0x10, d->bufsize); return DX_OK; }
    return DX_E_NOTIMPL;
}

static uint32_t d_acquire(const uint32_t* a, int n)   /* 0x1c Acquire */
{
    Dev* d = dev_of(dx_arg(a, n, 0));
    if (!d) return DX_E_INVALIDARG;
    if (d->joy >= 0) {
        if (!d->fmt_size) return DIERR_INVALIDPARAM_NOFORMAT;
        if (!d->acquired) { d->acquired = 1; joy_mark(d); }
        return DX_OK;
    }
    if (!d->acquired) {
        d->acquired = 1;
        keyboard_state(d->last);   /* keys already down are not reported as presses */
        d->nq = 0;
    }
    return DX_OK;
}

static uint32_t d_unacquire(const uint32_t* a, int n)   /* 0x20 Unacquire */
{
    Dev* d = dev_of(dx_arg(a, n, 0));
    if (!d) return DX_E_INVALIDARG;
    if (!d->acquired) return DI_NOEFFECT;
    d->acquired = 0;
    return DX_OK;
}

static uint32_t d_getstate(const uint32_t* a, int n)   /* 0x24 GetDeviceState(size, buffer) */
{
    Dev* d = dev_of(dx_arg(a, n, 0));
    uint32_t size = dx_arg(a, n, 1), buf = dx_arg(a, n, 2);
    if (!d || !buf) return DX_E_INVALIDARG;
    if (!d->acquired) return DIERR_NOTACQUIRED;
    if (d->joy >= 0) {
        if (size != d->fmt_size) return DX_E_INVALIDARG;
        joy_state(d, rt_ptr(buf), size);
        return DX_OK;
    }
    uint8_t st[256];
    keyboard_state(st);
    memset(rt_ptr(buf), 0, size);
    memcpy(rt_ptr(buf), st, size < 256 ? size : 256);
    return DX_OK;
}

/* 0x28 GetDeviceData(cbObjectData, DIDEVICEOBJECTDATA* or NULL, &count in / out, flags (PEEK 1)) */
static uint32_t d_getdata(const uint32_t* a, int n)
{
    Dev* d = dev_of(dx_arg(a, n, 0));
    uint32_t cb = dx_arg(a, n, 1), out = dx_arg(a, n, 2), pc = dx_arg(a, n, 3), flags = dx_arg(a, n, 4);
    if (!d || !pc) return DX_E_INVALIDARG;
    if (!d->acquired) return DIERR_NOTACQUIRED;
    if (!d->bufsize) return 0x80070001u;   /* DIERR_NOTBUFFERED */
    if (d->joy >= 0) joy_collect(d);
    else collect(d);
    uint32_t want = rt_r32(pc), k = 0;
    if (cb < 0x10) cb = 0x10;
    for (; k < want && (int)k < d->nq; k++) {
        if (!out) continue;
        uint32_t e = out + k * cb;
        rt_w32(e, d->q[k].ofs);
        rt_w32(e + 4, d->q[k].data);
        rt_w32(e + 8, d->q[k].time);
        rt_w32(e + 0xc, d->q[k].seq);
    }
    rt_w32(pc, k);
    if (!(flags & 1)) {
        memmove(d->q, d->q + k, sizeof d->q[0] * (size_t)(d->nq - (int)k));
        d->nq -= (int)k;
    }
    if (d->overflow) { d->overflow = 0; return DI_BUFFEROVERFLOW; }
    return DX_OK;
}

static uint32_t d_ok(const uint32_t* a, int n) { return dev_of(dx_arg(a, n, 0)) ? DX_OK : DX_E_INVALIDARG; }
static uint32_t d_unsupported(const uint32_t* a, int n) { (void)a; (void)n; return DX_E_NOTIMPL; }

/* 0x2c SetDataFormat(DIDATAFORMAT*): the keyboard takes any (it always reports 256 DIK bytes) */
static uint32_t d_setformat(const uint32_t* a, int n)
{
    Dev* d = dev_of(dx_arg(a, n, 0));
    uint32_t f = dx_arg(a, n, 1);
    if (!d || !f) return DX_E_INVALIDARG;
    if (d->joy < 0) return DX_OK;
    if (d->acquired) return DIERR_ACQUIRED;
    return j_setformat(d, f);
}

/* 0x10 EnumObjects(callback(DIDEVICEOBJECTINSTANCEA*, context), context, flags) */
static uint32_t d_enumobjects(const uint32_t* a, int n)
{
    Dev* d = dev_of(dx_arg(a, n, 0));
    uint32_t cb = dx_arg(a, n, 1), ctx = dx_arg(a, n, 2), flags = dx_arg(a, n, 3);
    if (!d || !cb) return DX_E_INVALIDARG;
    if (d->joy < 0) return DX_OK;   /* the keyboard: nothing (the game never asks) */
    uint32_t types = flags & 0x1f;   /* DIDFT_ALL 0: every object */
    uint32_t p = rt_frame_push(0x13c);
    for (int k = 0; k < d->nobj; k++) {
        if (types && !(obj_type(&d->obj[k]) & types)) continue;
        write_object(p, &d->obj[k], 0x13c);
        uint32_t w[2] = {p, ctx};
        if (rt_call(cb, w, 2) == 0) break;   /* DIENUM_STOP */
    }
    rt_frame_pop(0x13c);
    return DX_OK;
}

/* 0x38 GetObjectInfo(DIDEVICEOBJECTINSTANCEA*, dwObj, dwHow) */
static uint32_t d_objectinfo(const uint32_t* a, int n)
{
    Dev* d = dev_of(dx_arg(a, n, 0));
    uint32_t p = dx_arg(a, n, 1);
    if (!d || !p) return DX_E_INVALIDARG;
    if (d->joy < 0) return DX_E_NOTIMPL;
    JObj* o = find_obj(d, dx_arg(a, n, 2), dx_arg(a, n, 3));
    if (!o) return DIERR_OBJECTNOTFOUND;
    uint32_t size = rt_r32(p);
    if (size != 0x13c && size != 0x124) return DX_E_INVALIDARG;
    write_object(p, o, size);
    return DX_OK;
}

/* 0x3c GetDeviceInfo(DIDEVICEINSTANCEA*) */
static uint32_t d_deviceinfo(const uint32_t* a, int n)
{
    Dev* d = dev_of(dx_arg(a, n, 0));
    uint32_t p = dx_arg(a, n, 1);
    if (!d || !p) return DX_E_INVALIDARG;
    if (d->joy < 0) return DX_E_NOTIMPL;
    uint32_t size = rt_r32(p);
    if (size != 0x244 && size != 0x22c) return DX_E_INVALIDARG;
    write_instance(p, d->joy, size);
    return DX_OK;
}

/* 0x64 Poll: the keyboard needs none; a joystick is read at GetDeviceState */
static uint32_t d_poll(const uint32_t* a, int n)
{
    Dev* d = dev_of(dx_arg(a, n, 0));
    if (!d) return DX_E_INVALIDARG;
    if (d->joy < 0) return DI_NOEFFECT;
    return d->acquired ? DX_OK : DIERR_NOTACQUIRED;
}

/* DirectInputCreateA(hinst, version, IDirectInputA** out, outer), DINPUT.dll */
static uint32_t imp_directinput_create(const uint32_t* a, int n)
{
    uint32_t out = dx_arg(a, n, 2);
    if (!out) return DX_E_INVALIDARG;
    DxObj* o = dx_obj_new(&I_DI, NULL, NULL);
    if (!o) { rt_w32(out, 0); return 0x8007000eu; }
    rt_w32(out, o->addr);
    return DX_OK;
}

static const DxMethod M_DI[] = {
    {0x00, dx_unk_qi, "IDirectInputA::QueryInterface"},
    {0x04, dx_unk_addref, "IDirectInputA::AddRef"},
    {0x08, dx_unk_release, "IDirectInputA::Release"},
    {0x0c, di_create_device, "IDirectInputA::CreateDevice"},
    {0x10, di_enum_devices, "IDirectInputA::EnumDevices"},
    {0x14, di_get_device_status, "IDirectInputA::GetDeviceStatus"},
    {0x18, di_ok, "IDirectInputA::RunControlPanel"},
    {0x1c, di_initialize, "IDirectInputA::Initialize"},
};

static const DxMethod M_DID[] = {
    {0x00, dx_unk_qi, "IDirectInputDevice::QueryInterface"},
    {0x04, dx_unk_addref, "IDirectInputDevice::AddRef"},
    {0x08, dx_unk_release, "IDirectInputDevice::Release"},
    {0x0c, d_getcaps, "IDirectInputDevice::GetCapabilities"},
    {0x10, d_enumobjects, "IDirectInputDevice::EnumObjects"},
    {0x14, d_getproperty, "IDirectInputDevice::GetProperty"},
    {0x18, d_setproperty, "IDirectInputDevice::SetProperty"},
    {0x1c, d_acquire, "IDirectInputDevice::Acquire"},
    {0x20, d_unacquire, "IDirectInputDevice::Unacquire"},
    {0x24, d_getstate, "IDirectInputDevice::GetDeviceState"},
    {0x28, d_getdata, "IDirectInputDevice::GetDeviceData"},
    {0x2c, d_setformat, "IDirectInputDevice::SetDataFormat"},
    {0x30, d_ok, "IDirectInputDevice::SetEventNotification"},
    {0x34, d_ok, "IDirectInputDevice::SetCooperativeLevel"},
    {0x38, d_objectinfo, "IDirectInputDevice::GetObjectInfo"},
    {0x3c, d_deviceinfo, "IDirectInputDevice::GetDeviceInfo"},
    {0x40, d_ok, "IDirectInputDevice::RunControlPanel"},
    {0x44, d_ok, "IDirectInputDevice::Initialize"},
    {0x48, d_unsupported, "IDirectInputDevice2::CreateEffect"},
    {0x4c, d_ok, "IDirectInputDevice2::EnumEffects"},
    {0x50, d_unsupported, "IDirectInputDevice2::GetEffectInfo"},
    {0x54, d_unsupported, "IDirectInputDevice2::GetForceFeedbackState"},
    {0x58, d_unsupported, "IDirectInputDevice2::SendForceFeedbackCommand"},
    {0x5c, d_ok, "IDirectInputDevice2::EnumCreatedEffectObjects"},
    {0x60, d_unsupported, "IDirectInputDevice2::Escape"},
    {0x64, d_poll, "IDirectInputDevice2::Poll"},
    {0x68, d_unsupported, "IDirectInputDevice2::SendDeviceData"},
};

#define NM(m) ((int)(sizeof m / sizeof m[0]))

static void rt_bind_host_input_dinput(void)
{
    dx_iface_build(&I_DI, M_DI, NM(M_DI), 0x20 / 4, 0);
    dx_iface_build(&I_DID, M_DID, NM(M_DID), 0x6c / 4, 0);
    rt_bind_import("DirectInputCreateA", imp_directinput_create);
}

RT_BIND_AREA(host_input_dinput)
