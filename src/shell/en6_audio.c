/*
 * en6_audio.c: the audio dialog's mixer and control helpers (see en6_audio.h).
 * Verified under emulation: tools/emu/check_en6_audio.sh.
 */
#include "en6_audio.h"

#define I_GetLineInfo     0x00c90628u
#define I_GetNumDevs      0x00c905f8u
#define I_GetLineControls 0x00c90624u
#define I_GetDetails      0x00c9061cu
#define I_SetDetails      0x00c90630u
#define F_STRSTR          0x004bc710u
#define F_LABEL_HILITE    0x004a4a94u
#define F_GET_HWND        0x004a6dccu
#define F_COMBO_GETSEL    0x004a5bf0u
#define F_TRACK_GETPOS    0x004a6710u
#define F_TRACK_SETPOS    0x004a6830u
#define F_OVERLAY         0x00484260u

static uint32_t card(En2Env* e, uint32_t cfg) { return en2_r32(e, cfg + 0x1128); }

/* MIXERLINE at ml: cbStruct 0xa8, dwComponentType */
static uint32_t line_info(En2Env* e, uint32_t cfg, uint32_t ml, uint32_t type)
{
    en2_w32(e, ml, 0xa8);
    en2_w32(e, ml + 0x18, type);
    return en2_call3(e, I_GetLineInfo, card(e, cfg), ml, 3 /* MIXER_GETLINEINFOF_COMPONENTTYPE */);
}
/* MIXERLINECONTROLS at mlc for the line at ml, one MIXERCONTROL at mc */
static uint32_t line_controls(En2Env* e, uint32_t cfg, uint32_t mlc, uint32_t ml, uint32_t ctype, uint32_t mc)
{
    en2_w32(e, mlc, 0x18);
    en2_w32(e, mlc + 4, en2_r32(e, ml + 0xc));
    en2_w32(e, mlc + 8, ctype);
    en2_w32(e, mlc + 0xc, en2_r32(e, ml + 0x24));
    en2_w32(e, mlc + 0x10, 0x94);
    en2_w32(e, mlc + 0x14, mc);
    return en2_call3(e, I_GetLineControls, card(e, cfg), mlc, 2 /* ONEBYTYPE */);
}
/* MIXERCONTROLDETAILS at mcd for the control at mc, the line's channels, 4 byte details at pa */
static void details(En2Env* e, uint32_t mcd, uint32_t mc, uint32_t ml, uint32_t pa)
{
    en2_w32(e, mcd, 0x18);
    en2_w32(e, mcd + 4, en2_r32(e, mc + 4));
    en2_w32(e, mcd + 8, en2_r32(e, ml + 0x1c));
    en2_w32(e, mcd + 0xc, 0);
    en2_w32(e, mcd + 0x10, 4);
    en2_w32(e, mcd + 0x14, pa);
}

/* the line of the type; for the wave type 0x1005 when the card has none: the mixers from the last
 * down to the first whose line name contains name */
static void find_line(En2Env* e, uint32_t cfg, uint32_t F, uint32_t type, uint32_t name)
{
    en2_w32(e, F + 0x18, type);
    en2_w32(e, F, 0xa8);
    if (en2_call3(e, I_GetLineInfo, card(e, cfg), F, 3) != 0 && type == 0x1005) {
        int32_t n = (int32_t)en2_call0(e, I_GetNumDevs);
        while (n > 0) {
            n--;
            en2_w32(e, F, 0xa8);
            en2_call3(e, I_GetLineInfo, (uint32_t)n, F, 0x50000000u);
            if (en2_call2(e, F_STRSTR, F + 0x38, name) != 0) break;
        }
    }
}

uint32_t en6_mixer_get(En2Env* e, uint32_t cfg, uint32_t type)
{
    const uint32_t F = e->scratch;   /* +0 MIXERLINE, +0xa8 MIXERCONTROL, +0x13c details, +0x154 controls, +0x16c value */
    find_line(e, cfg, F, type, 0x004e2d4eu);
    if (line_controls(e, cfg, F + 0x154, F, 0x50030001u, F + 0xa8) != 0) return 0;
    details(e, F + 0x13c, F + 0xa8, F, F + 0x16c);
    en2_call3(e, I_GetDetails, card(e, cfg), F + 0x13c, 0);
    return en2_r32(e, F + 0x16c);
}

uint32_t en6_mixer_set(En2Env* e, uint32_t cfg, uint32_t type, uint32_t value)
{
    const uint32_t F = e->scratch;   /* the same layout, the values at +0x16c / +0x170 */
    find_line(e, cfg, F, type, 0x004e2d51u);
    line_controls(e, cfg, F + 0x154, F, 0x50030001u, F + 0xa8);
    details(e, F + 0x13c, F + 0xa8, F, F + 0x16c);
    en2_w32(e, F + 0x16c, value);
    en2_w32(e, F + 0x170, value);
    return en2_call3(e, I_SetDetails, card(e, cfg), F + 0x13c, 0);
}

void en6_audio_labels(En2Env* e, uint32_t cfg)
{
    static const uint16_t label[6] = {0x48c, 0x658, 0x824, 0x9f0, 0xd88, 0xbbc};
    static const uint16_t ctrl[6] = {0x9c, 0x118, 0x1c0, 0x16c, 0x268, 0x214};
    static const uint16_t ctrl_label[6] = {0x48c, 0x658, 0x9f0, 0x824, 0xd88, 0xbbc};
    for (int k = 0; k < 6; k++) en2_call2(e, F_LABEL_HILITE, cfg + label[k], 0);
    for (int k = 0; k < 6; k++) {
        if (en2_call1(e, F_GET_HWND, cfg + ctrl[k]) == en2_r32(e, cfg + 0x488)) {
            en2_call2(e, F_LABEL_HILITE, cfg + ctrl_label[k], 1);
            return;
        }
    }
}

/* the speaker line's control ctype set to v on both channels; 0 on success */
static int speaker_set(En2Env* e, uint32_t cfg, uint32_t ml, uint32_t mlc, uint32_t mc, uint32_t mcd, uint32_t pa,
                       uint32_t ctype, uint32_t v)
{
    if (line_info(e, cfg, ml, 4 /* MIXERLINE_COMPONENTTYPE_DST_SPEAKERS */) != 0) return -1;
    if (line_controls(e, cfg, mlc, ml, ctype, mc) != 0) return -1;
    details(e, mcd, mc, ml, pa);
    en2_w32(e, pa, v);
    en2_w32(e, pa + 4, v);
    en2_call3(e, I_SetDetails, card(e, cfg), mcd, 0);
    return 0;
}

void en6_audio_apply_sliders(En2Env* e, uint32_t cfg, uint32_t combo)
{
    const uint32_t F = e->scratch;
    uint32_t old = card(e, cfg);
    uint32_t c = en2_call1(e, F_COMBO_GETSEL, combo);
    en2_w32(e, cfg + 0x1128, c);
    if (old != c) en2_call1(e, F_OVERLAY, 0x1c);
    en2_call3(e, 0x00489340u, cfg, 4, en2_call1(e, F_TRACK_GETPOS, cfg + 0x118) * 10);
    en2_call3(e, 0x00489340u, cfg, 0x1005, en2_call1(e, F_TRACK_GETPOS, cfg + 0x16c) * 10);
    en2_call3(e, 0x00489340u, cfg, 0x1008, en2_call1(e, F_TRACK_GETPOS, cfg + 0x1c0) * 10);
    if (en2_r32(e, cfg + 0x1120) == 0) return;
    uint32_t v = en2_call1(e, F_TRACK_GETPOS, cfg + 0x268) * 10;
    if (en2_r32(e, cfg + 0x1120) != 0 &&
        speaker_set(e, cfg, F + 0xa8, F + 0x278, F + 0x150, F + 0x290, F + 0x2d8, 0x50030002u, v) != 0)
        en2_w32(e, cfg + 0x1120, 0);
    v = en2_call1(e, F_TRACK_GETPOS, cfg + 0x214) * 10;
    if (en2_r32(e, cfg + 0x1120) != 0 &&
        speaker_set(e, cfg, F, F + 0x2a8, F + 0x1e4, F + 0x2c0, F + 0x2e0, 0x50030003u, v) != 0)
        en2_w32(e, cfg + 0x1120, 0);
}

/* the speaker line's control ctype read; 0 and +0x1120 cleared on a failure */
static uint32_t speaker_get(En2Env* e, uint32_t cfg, uint32_t ml, uint32_t mlc, uint32_t mc, uint32_t mcd, uint32_t pa,
                            uint32_t ctype)
{
    if (line_info(e, cfg, ml, 4) != 0 || line_controls(e, cfg, mlc, ml, ctype, mc) != 0) {
        en2_w32(e, cfg + 0x1120, 0);
        return 0;
    }
    details(e, mcd, mc, ml, pa);
    en2_call3(e, I_GetDetails, card(e, cfg), mcd, 0);
    return en2_r32(e, pa);
}

void en6_audio_read_sliders(En2Env* e, uint32_t cfg)
{
    const uint32_t F = e->scratch;
    static const struct { uint32_t type; uint16_t store, slider; } k[3] = {
        {4, 0x88, 0x118}, {0x1005, 0x90, 0x16c}, {0x1008, 0x8c, 0x1c0}};
    for (int i = 0; i < 3; i++) {
        uint32_t v = en2_call2(e, 0x004890c4u, cfg, k[i].type);
        en2_w32(e, cfg + k[i].store, v);
        en2_call3(e, F_TRACK_SETPOS, cfg + k[i].slider, 1, v / 10);
    }
    if (en2_r32(e, cfg + 0x1120) == 0) return;
    uint32_t v = speaker_get(e, cfg, F, F + 0x2a8, F + 0x150, F + 0x2c0, F + 0x2e0, 0x50030002u);
    en2_w32(e, cfg + 0x94, v);
    en2_call3(e, F_TRACK_SETPOS, cfg + 0x268, 1, v / 10);
    v = speaker_get(e, cfg, F + 0xa8, F + 0x290, F + 0x1e4, F + 0x278, F + 0x2d8, 0x50030003u);
    en2_w32(e, cfg + 0x98, v);
    en2_call3(e, F_TRACK_SETPOS, cfg + 0x214, 1, v / 10);
}

uint32_t en6_audio_dialog_destroy(En2Env* e, uint32_t obj, uint32_t flags)
{
    if (flags & 4) {
        en2_call1(e, 0x004aa018u, en2_call2(e, 0x004a9ff8u, obj, 0x004e57b8u));
        return obj;
    }
    uint32_t r = en2_call2(e, 0x004aa064u, obj + 0x1b8, 0);
    for (int i = 0; i < 9; i++) r = en2_call2(e, 0x004aa064u, r - 0x14, 0);
    r = en2_call2(e, 0x004aa064u, r - 0x14, 0);
    r = en2_call2(e, 0x004aa064u, r - 0x14, 0);
    r = en2_call2(e, 0x004aa064u, r - 0x34, 1);
    for (int i = 0; i < 3; i++) r = en2_call2(e, 0x004aa064u, r - 0x14, 1);
    r = en2_call2(e, 0x004a3ff4u, r - 0x6c, 1);
    if (flags & 2) en2_call1(e, 0x004aa39cu, r);
    return r;
}

/* ---- 0x004877e4: the dialog's run loop ---- */
#define F_INPUT_POLL  0x00493748u
#define F_INPUT_KEY   0x004a2068u
#define F_INPUT_CHECK 0x004936acu
#define F_APPLY_VOL   0x00486bb4u
#define F_REG_WRITE   0x004ac65cu   /* reg_write_data (key, name, data, size) */
#define F_DISPLAY_RECT 0x004a2fdcu
#define I_GetFocus    0x00c904ccu

/* OK: the current volumes saved into the card's row, applied (mode 1), written to the registry
 * ("GameVolume" 0x004e2d26, 0x3c bytes) */
static void dialog_ok(En2Env* e, uint32_t cfg)
{
    en2_w32(e, cfg + 0x1124, 1);
    en2_call1(e, F_OVERLAY, 0x1d);
    uint32_t row = cfg + 0x4c + en2_r32(e, cfg + 0x1128) * 0x14;
    for (uint32_t i = 0; i < 20; i += 4) en2_w32(e, row + i, en2_r32(e, cfg + 0x88 + i));   /* repne movsd */
    en2_call2(e, F_APPLY_VOL, cfg, 1);
    en2_call4(e, F_REG_WRITE, cfg + 0x112c, 0x004e2d26u, cfg + 0x4c, 0x3c);
    en2_w32(e, cfg + 0x1124, 1);
    en2_call1(e, F_DISPLAY_RECT, 0x009f6cd8u);
}
/* slider -> value * 10 into cfg field (sound 0x1c when it changed) */
static uint32_t slider_take(En2Env* e, uint32_t cfg, uint32_t slider, uint32_t field)
{
    uint32_t old = en2_r32(e, cfg + field);
    uint32_t v = en2_call1(e, F_TRACK_GETPOS, cfg + slider) * 10;
    en2_w32(e, cfg + field, v);
    if (old != v) en2_call1(e, F_OVERLAY, 0x1c);
    return v;
}

void en6_audio_dialog_run(En2Env* e, uint32_t cfg)
{
    const uint32_t F = e->scratch;   /* the mixer structures at the original's offsets (+0 .. +0x5c8) */
    int32_t count = 0;
    if (en2_r32(e, cfg + 0x1124) != 0) return;
    do {
        en2_call0(e, F_INPUT_POLL);
        if ((en2_call0(e, F_INPUT_KEY) & 0xff) == 0x1c) {
            uint32_t f = en2_call0(e, I_GetFocus);
            if (f != en2_call1(e, F_GET_HWND, cfg + 0x2bc)) dialog_ok(e, cfg);
        }
        if (en2_call2(e, F_INPUT_CHECK, 0x14, 0) & 0xff) {
            dialog_ok(e, cfg);
            en2_call1(e, F_OVERLAY, 0x1e);
        }
        if (en2_call2(e, F_INPUT_CHECK, 0x15, 0) & 0xff) {
            dialog_ok(e, cfg);
            en2_call1(e, F_OVERLAY, 0x1e);
        }
        if (en2_call0(e, I_GetFocus) != en2_r32(e, cfg + 0x488) && en2_r32(e, cfg + 0x1124) != 1) {
            en2_call1(e, F_OVERLAY, 0x1c);
            en2_w32(e, cfg + 0x488, en2_call0(e, I_GetFocus));
            en2_call1(e, 0x004880d8u, cfg);
        }
        if (count++ > 200) {   /* every 202nd pass: the sliders to the mixers and back */
            count = 0;
            static const struct { uint16_t slider, field; uint32_t type; } k[3] = {
                {0x118, 0x88, 4}, {0x16c, 0x90, 0x1005}, {0x1c0, 0x8c, 0x1008}};
            for (int i = 0; i < 3; i++) {
                slider_take(e, cfg, k[i].slider, k[i].field);
                en2_call3(e, 0x00489340u, cfg, k[i].type, en2_r32(e, cfg + k[i].field));
                en2_w32(e, cfg + k[i].field, en2_call2(e, 0x004890c4u, cfg, k[i].type));
            }
            slider_take(e, cfg, 0x268, 0x94);
            if (en2_r32(e, cfg + 0x1120) != 0 &&
                speaker_set(e, cfg, F + 0x1f8, F + 0x508, F + 0x2a0, F + 0x598, F + 0x5c0, 0x50030002u,
                            en2_r32(e, cfg + 0x94)) != 0)
                en2_w32(e, cfg + 0x1120, 0);
            en2_w32(e, cfg + 0x94, speaker_get(e, cfg, F + 0x150, F + 0x550, F + 0x3c8, F + 0x568, F + 0x5b8, 0x50030002u));
            slider_take(e, cfg, 0x214, 0x98);
            if (en2_r32(e, cfg + 0x1120) != 0 &&
                speaker_set(e, cfg, F, F + 0x4f0, F + 0x45c, F + 0x520, F + 0x5c8, 0x50030003u,
                            en2_r32(e, cfg + 0x98)) != 0)
                en2_w32(e, cfg + 0x1120, 0);
            en2_w32(e, cfg + 0x98, speaker_get(e, cfg, F + 0xa8, F + 0x538, F + 0x334, F + 0x580, F + 0x5b0, 0x50030003u));
        }
    } while (en2_r32(e, cfg + 0x1124) == 0);
}
