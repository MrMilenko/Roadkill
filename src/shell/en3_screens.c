/*
 * en3_screens.c: front end screens of TM2.EXE 0x00480000..0x00490000 (see en3_screens.h).
 * Verified under emulation: tools/emu/check_en3_screens.sh.
 */
#include "en3_screens.h"

/* callees (TM2.EXE addresses) */
#define F_SOUND_PLAY      0x0046f8b0u
#define F_SOUND_FLUSH     0x0046f94cu
#define F_SOUND_METHOD1   0x0046f958u
#define F_SCENE_INIT      0x00469f08u   /* (scene, draw, update, back; stack: 1 word) */
#define F_FILE_PATH       0x00499408u   /* (out, name, root slot) */
#define F_IMAGE_INIT      0x00469c40u
#define F_IMAGE_LOAD      0x00469c78u
#define F_CHOICE_SET      0x00469cc4u
#define F_CHOICE_GET      0x00469cdcu
#define F_ADD_DECOR       0x0046a138u
#define F_ADD_ITEM        0x0046a15cu
#define F_ADD_ACTION      0x0046a18cu   /* (scene, event, source, type; stack: command) */
#define F_LOAD_RES        0x0046a324u
#define F_SCENE_RUN       0x0046a5fcu

#define VT_ITEM   0x004e5798u
#define VT_DECOR  0x004e57a0u
#define VT_MOVE   0x004e577cu
#define VT_CHOICE 0x004e5790u
#define VT_WAIT   0x004e575cu
#define VT_TEXT   0x004e5794u
#define VT_U8     0x004e5760u
#define VT_SET    0x004e5770u
#define VT_NEXT   0x004e576cu
#define VT_PREV   0x004e5768u

static void w32(En2Env* e, uint32_t a, int32_t v) { en2_w32(e, a, (uint32_t)v); }

/* the common part of a scene object (item or decor) */
static void obj_init(En2Env* e, uint32_t o, uint16_t id, uint32_t vt, uint16_t f0c, uint16_t f0e, uint16_t f10,
                     int32_t x, int32_t z)
{
    en2_w16(e, o, id);
    w32(e, o + 0x04, 0);
    en2_w32(e, o + 0x08, vt);
    en2_w16(e, o + 0x0c, f0c);
    en2_w16(e, o + 0x0e, f0e);
    en2_w16(e, o + 0x10, f10);
    w32(e, o + 0x14, 1);
    w32(e, o + 0x18, 0);
    w32(e, o + 0x1c, x);
    w32(e, o + 0x20, 0);
    w32(e, o + 0x24, z);
    w32(e, o + 0x28, 0);
    w32(e, o + 0x2c, 0);
    w32(e, o + 0x30, 0);
    w32(e, o + 0x4c, 0);
}
static void obj_hotspot(En2Env* e, uint32_t o, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    w32(e, o + 0x34, x0);
    w32(e, o + 0x38, y0);
    w32(e, o + 0x3c, x1);
    w32(e, o + 0x40, y1);
    w32(e, o + 0x44, 1);
}
/* a menu row: model id, position, hot spot rows y0..y1 (x -296..-76), the screen it leads to */
static void menu_item(En2Env* e, uint32_t o, uint16_t id, int32_t x, int32_t z, int32_t y0, int32_t y1, uint32_t fn)
{
    obj_init(e, o, id, VT_ITEM, 1, 0, 0, x, z);
    obj_hotspot(e, o, -296, y0, -76, y1);
    en2_w32(e, o + 0x50, fn);
}
/* an invisible clickable area (no model) */
static void hot_area(En2Env* e, uint32_t o, int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    obj_init(e, o, 0, VT_DECOR, 0, 1, 0xffff, 0, 0);
    obj_hotspot(e, o, x0, y0, x1, y1);
}
static void cmd_head(En2Env* e, uint32_t c, int32_t dur, uint32_t vt, uint32_t target)
{
    w32(e, c, dur);
    w32(e, c + 4, dur);
    en2_w32(e, c + 8, vt);
    en2_w32(e, c + 0xc, target);
}
static void cmd_move(En2Env* e, uint32_t c, uint32_t target, int32_t dx)
{
    cmd_head(e, c, 7, VT_MOVE, target);
    w32(e, c + 0x10, dx);
    w32(e, c + 0x14, 0);
    w32(e, c + 0x18, 0);
}
static void cmd_choice_step(En2Env* e, uint32_t c, uint32_t target, int32_t step)
{
    cmd_head(e, c, -1, VT_CHOICE, target);
    en2_w8(e, c + 0x10, 0);   /* mode 0: step */
    w32(e, c + 0x14, step);
    w32(e, c + 0x18, 0);      /* no validator */
}
static void cmd_wait(En2Env* e, uint32_t c, int32_t player)
{
    cmd_head(e, c, 30, VT_WAIT, EN3_SCENE);
    w32(e, c + 0x10, player);
    w32(e, c + 0x18, 0);
}
static void cmd_text(En2Env* e, uint32_t c, int32_t id, uint32_t font)
{
    en2_w32(e, c + 8, VT_TEXT);
    w32(e, c + 0x10, id);
    w32(e, c + 0x14, 0);
    w32(e, c + 0x18, 0);
    en2_w32(e, c + 0x1c, font);
}
static void cmd_scene(En2Env* e, uint32_t c, uint32_t vt)
{
    en2_w32(e, c + 8, vt);
    en2_w32(e, c + 0xc, EN3_SCENE);
}
static void cmd_scene_u8(En2Env* e, uint32_t c, uint8_t v)
{
    cmd_scene(e, c, VT_U8);
    en2_w8(e, c + 0x10, v);
}

/* screenOptions' frame (offsets from the frame base) */
enum {
    O_IMAGE = 0x000, O_PATH = 0x134,
    O_HISTORY = 0x260, O_TOGGLE_A = 0x2b4, O_TOGGLE_B = 0x308, O_INPUT = 0x35c, O_AUDIO = 0x3b0,
    O_AREA_C = 0x404, O_CHOICE_A = 0x454, O_CHOICE_B = 0x4a4, O_AREA_A = 0x4f4, O_AREA_B = 0x544,
    O_WAIT0 = 0x594, O_WAIT2 = 0x5b8, O_TEXT_A = 0x5dc, O_TEXT_B = 0x5fc,
    O_U8_2 = 0x830, O_U8_0 = 0x844, O_SET0 = 0x858, O_U8_1 = 0x86c, O_NEXT = 0x880, O_PREV = 0x890,
};

/* action rows {event, source, type, command}: sources and commands are frame offsets
 * (source 0 = any). Event numbers are the scene engine's: 7 / 8 up / down, 9 / 0xa left / right,
 * 0xe / 0x11 select / click, 0xf / 0x10 / 3 / 2 as the scene engine posts them. */
static const struct { uint8_t ev; uint16_t src; uint8_t type; uint16_t cmd; } k_options_actions[] = {
    {7, 0, 2, 0x5dc}, {7, 0, 2, 0x890}, {7, 0, 2, 0x858}, {8, 0, 2, 0x5dc}, {8, 0, 2, 0x880}, {8, 0, 2, 0x858},
    {0x11, 0x4f4, 2, 0x5fc}, {0x11, 0x4f4, 1, 0x594}, {0x11, 0x4f4, 0, 0x844},
    {0xf, 0, 2, 0x5fc}, {0xf, 0, 1, 0x594}, {0xf, 0, 1, 0x5b8}, {0xf, 0, 0, 0x86c},
    {0x11, 0x544, 2, 0x5fc}, {0x11, 0x544, 1, 0x594}, {0x11, 0x544, 0, 0x86c},
    {0x10, 0, 2, 0x5fc}, {0x10, 0, 1, 0x594}, {0x10, 0, 1, 0x5b8}, {0x10, 0, 0, 0x830},
    {0x11, 0x404, 2, 0x5fc}, {0x11, 0x404, 1, 0x594}, {0x11, 0x404, 0, 0x830},
    {0, 0, 0, 0x858}, {0, 0, 0, 0x76c}, {0, 0, 0, 0x6e0}, {0, 0, 0, 0x6a8}, {0, 0, 0, 0x788}, {0, 0, 0, 0x638},
    {2, 0x76c, 2, 0x5dc}, {2, 0x76c, 2, 0x7c0}, {2, 0x76c, 1, 0x858}, {2, 0x76c, 0, 0x594},
    {7, 0x2b4, 2, 0x7dc}, {7, 0x2b4, 2, 0x6c4}, {8, 0x2b4, 2, 0x7dc}, {8, 0x2b4, 2, 0x7f8},
    {9, 0x2b4, 2, 0x5dc}, {9, 0x2b4, 2, 0x7a4}, {9, 0x2b4, 2, 0x858},
    {0xa, 0x2b4, 2, 0x5dc}, {0xa, 0x2b4, 2, 0x718}, {0xa, 0x2b4, 2, 0x858},
    {0xe, 0x2b4, 2, 0x5dc}, {0xe, 0x2b4, 2, 0x718}, {0xe, 0x2b4, 2, 0x858},
    {0x11, 0x2b4, 2, 0x5dc}, {0x11, 0x2b4, 2, 0x718}, {0x11, 0x2b4, 2, 0x858},
    {7, 0x308, 2, 0x750}, {7, 0x308, 2, 0x7c0}, {8, 0x308, 2, 0x750}, {8, 0x308, 2, 0x61c},
    {9, 0x308, 2, 0x5dc}, {9, 0x308, 2, 0x6fc}, {9, 0x308, 2, 0x858},
    {0xa, 0x308, 2, 0x5dc}, {0xa, 0x308, 2, 0x670}, {0xa, 0x308, 2, 0x858},
    {0xe, 0x308, 2, 0x5dc}, {0xe, 0x308, 2, 0x670}, {0xe, 0x308, 2, 0x858},
    {0x11, 0x308, 2, 0x5dc}, {0x11, 0x308, 2, 0x670}, {0x11, 0x308, 2, 0x858},
    {7, 0x35c, 2, 0x734}, {7, 0x35c, 2, 0x7f8}, {8, 0x35c, 2, 0x734}, {8, 0x35c, 2, 0x814},
    {8, 0x35c, 1, 0x594}, {8, 0x35c, 0, 0x5b8},
    {3, 0x35c, 2, 0x5fc}, {3, 0x35c, 2, 0x858}, {3, 0x35c, 1, 0x594}, {3, 0x35c, 0, 0x844},
    {0xe, 0x35c, 2, 0x5fc}, {0xe, 0x35c, 2, 0x858}, {0xe, 0x35c, 1, 0x594}, {0xe, 0x35c, 0, 0x844},
    {7, 0x3b0, 2, 0x68c}, {7, 0x3b0, 2, 0x61c}, {7, 0x3b0, 1, 0x594}, {7, 0x3b0, 0, 0x5b8},
    {8, 0x3b0, 2, 0x68c}, {8, 0x3b0, 2, 0x6c4},
    {3, 0x3b0, 2, 0x5fc}, {3, 0x3b0, 2, 0x858}, {3, 0x3b0, 1, 0x594}, {3, 0x3b0, 0, 0x844},
    {0xe, 0x3b0, 2, 0x5fc}, {0xe, 0x3b0, 2, 0x858}, {0xe, 0x3b0, 1, 0x594}, {0xe, 0x3b0, 0, 0x844},
    {7, 0x260, 2, 0x654}, {7, 0x260, 2, 0x814}, {8, 0x260, 2, 0x654}, {8, 0x260, 2, 0x7c0},
    {3, 0x260, 2, 0x5fc}, {3, 0x260, 2, 0x858}, {3, 0x260, 1, 0x594}, {3, 0x260, 0, 0x844},
    {0xe, 0x260, 2, 0x5fc}, {0xe, 0x260, 2, 0x858}, {0xe, 0x260, 1, 0x594}, {0xe, 0x260, 0, 0x844},
    {2, 0x594, 2, 0x86c}, {2, 0x5b8, 2, 0x86c},
};

uint32_t en3_screen_options(En2Env* e)
{
    const uint32_t F = e->scratch;
    en2_call4(e, F_SOUND_PLAY, 0x1b, 0, 0, 0x63);
    en2_call0(e, F_SOUND_FLUSH);
    en2_call0(e, F_SOUND_METHOD1);
    en2_call5(e, F_SCENE_INIT, EN3_SCENE, 0x004780fcu, 0x004780fcu, 0x00477b90u /* back handler */, 0);
    /* scene +0x10: the view (camera at z -1200, no turn) */
    w32(e, EN3_SCENE + 0x10, 0);
    w32(e, EN3_SCENE + 0x14, -0x4b0);
    w32(e, EN3_SCENE + 0x18, 0);
    en2_w16(e, EN3_SCENE + 0x1c, 0);
    en2_w16(e, EN3_SCENE + 0x1e, 0);
    en2_w16(e, EN3_SCENE + 0x20, 0);
    en2_call3(e, F_FILE_PATH, F + O_PATH, 0x004e2a46u /* background name */, 1);
    en2_call3(e, F_IMAGE_INIT, F + O_IMAGE, F + O_PATH, 0x2e630);

    /* the five menu rows (models 0x2329..0x232d) and their screens */
    menu_item(e, F + O_TOGGLE_A, 0x2329, -3488, 0x2d0, 0x4b, 0x69, 0x004780fcu);
    menu_item(e, F + O_TOGGLE_B, 0x232a, -1248, 0x160, 0x1d, 0x3b, 0x004780fcu);
    menu_item(e, F + O_INPUT, 0x232b, -3488, -16, -17, 0xd, 0x00484230u);
    menu_item(e, F + O_AUDIO, 0x232c, -3488, -384, -63, -33, 0x00484250u);
    menu_item(e, F + O_HISTORY, 0x232d, -1248, -752, -109, -79, 0x0048429cu);
    /* the two choice models and three click areas */
    obj_init(e, F + O_CHOICE_A, 0x232e, VT_DECOR, 1, 0, 0, -640, 0x2d0);
    w32(e, F + O_CHOICE_A + 0x44, 0);
    obj_init(e, F + O_CHOICE_B, 0x232f, VT_DECOR, 1, 0, 0, -640, 0x160);
    w32(e, F + O_CHOICE_B + 0x44, 0);
    hot_area(e, F + O_AREA_A, -300, -210, -170, -190);
    hot_area(e, F + O_AREA_B, -100, -210, 100, -190);
    hot_area(e, F + O_AREA_C, 130, -210, 300, -190);

    cmd_wait(e, F + O_WAIT0, 0);
    cmd_wait(e, F + O_WAIT2, 2);
    uint32_t font = en2_r32(e, 0x00626010u);
    cmd_text(e, F + O_TEXT_A, 0x1c, font);
    cmd_text(e, F + O_TEXT_B, 0x1d, font);
    /* rows step left / right through the 0x1c byte command array at F + 0x61c */
    cmd_choice_step(e, F + 0x61c, F + O_INPUT, 1);
    cmd_move(e, F + 0x638, F + O_HISTORY, -160);
    cmd_choice_step(e, F + 0x654, F + O_HISTORY, -1);
    cmd_choice_step(e, F + 0x670, F + O_CHOICE_B, 1);
    cmd_choice_step(e, F + 0x68c, F + O_AUDIO, -1);
    cmd_move(e, F + 0x6a8, F + O_INPUT, 160);
    cmd_choice_step(e, F + 0x6c4, F + O_HISTORY, 1);
    cmd_move(e, F + 0x6e0, F + O_TOGGLE_B, -160);
    cmd_choice_step(e, F + 0x6fc, F + O_CHOICE_B, -1);
    cmd_choice_step(e, F + 0x718, F + O_CHOICE_A, 1);
    cmd_choice_step(e, F + 0x734, F + O_INPUT, -1);
    cmd_choice_step(e, F + 0x750, F + O_TOGGLE_B, -1);
    cmd_move(e, F + 0x76c, F + O_TOGGLE_A, 160);
    cmd_move(e, F + 0x788, F + O_AUDIO, 160);
    cmd_choice_step(e, F + 0x7a4, F + O_CHOICE_A, -1);
    cmd_choice_step(e, F + 0x7c0, F + O_TOGGLE_A, 1);
    cmd_choice_step(e, F + 0x7dc, F + O_TOGGLE_A, -1);
    cmd_choice_step(e, F + 0x7f8, F + O_TOGGLE_B, 1);
    cmd_choice_step(e, F + 0x814, F + O_AUDIO, 1);
    cmd_scene_u8(e, F + O_U8_2, 2);
    cmd_scene_u8(e, F + O_U8_0, 0);
    cmd_scene(e, F + O_SET0, VT_SET);
    w32(e, F + O_SET0 + 0x10, 0);
    cmd_scene_u8(e, F + O_U8_1, 1);
    cmd_scene(e, F + O_NEXT, VT_NEXT);
    cmd_scene(e, F + O_PREV, VT_PREV);

    en2_w32(e, EN3_SCENE + 0x4c, F + O_IMAGE);   /* the background image record */
    en2_call2(e, F_ADD_ITEM, EN3_SCENE, F + O_TOGGLE_A);
    en2_call2(e, F_ADD_DECOR, EN3_SCENE, F + O_CHOICE_A);
    en2_call2(e, F_ADD_ITEM, EN3_SCENE, F + O_TOGGLE_B);
    en2_call2(e, F_ADD_DECOR, EN3_SCENE, F + O_CHOICE_B);
    en2_call2(e, F_ADD_ITEM, EN3_SCENE, F + O_INPUT);
    en2_call2(e, F_ADD_ITEM, EN3_SCENE, F + O_AUDIO);
    en2_call2(e, F_ADD_ITEM, EN3_SCENE, F + O_HISTORY);
    en2_call2(e, F_ADD_DECOR, EN3_SCENE, F + O_AREA_A);
    en2_call2(e, F_ADD_DECOR, EN3_SCENE, F + O_AREA_B);
    en2_call2(e, F_ADD_DECOR, EN3_SCENE, F + O_AREA_C);
    for (unsigned i = 0; i < sizeof k_options_actions / sizeof k_options_actions[0]; i++) {
        uint32_t src = k_options_actions[i].src ? F + k_options_actions[i].src : 0;
        en2_call5(e, F_ADD_ACTION, EN3_SCENE, k_options_actions[i].ev, src, k_options_actions[i].type,
                  F + k_options_actions[i].cmd);
    }
    en2_call2(e, F_LOAD_RES, EN3_SCENE, 0x004e2a58u /* scene model file name */);
    en2_call1(e, F_IMAGE_LOAD, F + O_IMAGE);
    en2_call2(e, F_CHOICE_SET, F + O_CHOICE_A, en2_r8(e, 0x00624ed0u));
    en2_call2(e, F_CHOICE_SET, F + O_CHOICE_B, en2_r8(e, 0x00624ed1u));
    uint32_t next = en2_call1(e, F_SCENE_RUN, EN3_SCENE);
    uint32_t cur = en2_r32(e, EN3_SCENE + 0x48);
    en2_w8(e, 0x009f6cd4u, en2_r32(e, EN3_SCENE + 0xbc + cur * 4) != F + O_INPUT);
    en2_w8(e, 0x00624ed0u, (uint8_t)en2_call1(e, F_CHOICE_GET, F + O_CHOICE_A));
    en2_w8(e, 0x00624ed1u, (uint8_t)en2_call1(e, F_CHOICE_GET, F + O_CHOICE_B));
    return next;
}

/* ---- 0x004813e8: the opponent line up ---- */
#define F_SPRINTF   0x004b27e6u   /* Watcom sprintf (cdecl, caller pops) */
#define F_FILE_LOAD 0x00499564u   /* file_load_secondary (name, root slot) */
#define F_RAND      0x004899c2u
#define VT_MOVE_TURN 0x004e5774u
#define VT_WAIT_TIMEOUT 0x004e5758u

/* the nine opponent models and their move commands, in opponent order (frame offsets) */
static const uint16_t k_lineup_obj[9] = {0x440, 0x3a0, 0x300, 0x4e0, 0x490, 0x260, 0x3f0, 0x350, 0x2b0};
static const uint16_t k_lineup_move[9] = {0x648, 0x530, 0x580, 0x5a8, 0x5f8, 0x558, 0x5d0, 0x620, 0x670};
enum { L_TABLE = 0x698, L_TEXT = 0x6bc, L_WAIT = 0x6dc, L_U8 = 0x6fc, L_SET = 0x710 };

/* the step of each move: by opponent count 2..4 -> 10, 5..6 -> 20, 7..9 -> 25, else 0 (times 8) */
static int32_t lineup_step(uint8_t n)
{
    if (n < 5) return n < 2 ? 0 : 10;
    if (n <= 6) return 20;
    if (n <= 9) return 25;
    return 0;
}

uint32_t en3_screen_lineup(En2Env* e)
{
    const uint32_t F = e->scratch;
    uint8_t mode = en2_r8(e, 0x00624ea8u);
    if (mode != 0 && mode != 1 && mode != 2) return 0x00485100u;
    /* the background name: sprintf(buf, 0x004e2a29, 0x004e2a1f, 0x004e2a17, (int8)arena) */
    {
        uint32_t v[5] = {F + O_PATH, 0x004e2a29u, 0x004e2a1fu, 0x004e2a17u,
                         (uint32_t)(int32_t)(int8_t)en2_r8(e, 0x00624ec7u)};
        e->call(e->user, F_SPRINTF, v, 5);
    }
    en2_call2(e, F_FILE_LOAD, F + O_PATH, 1);
    en2_call5(e, F_SCENE_INIT, EN3_SCENE, 0x004780fcu, 0x004780fcu, 0x00477b90u, 0);
    en2_call3(e, F_IMAGE_INIT, F + O_IMAGE, F + O_PATH, 0x2e630);

    uint32_t n = en2_r8(e, 0x00624eaau);   /* opponent count */
    for (uint32_t k = 0; k < 9; k++) {
        uint32_t o = F + k_lineup_obj[k];
        obj_init(e, o, 0x2329, VT_DECOR, n >= k + 1, 1, 0xffff, 0, 0);
        w32(e, o + 0x44, 0);
        en2_w32(e, F + L_TABLE + 4 * k, o);
    }
    /* ORIGINAL: more than 9 opponents would read past the 9 entry table (never happens) */
    for (uint32_t i = 0; i < en2_r8(e, 0x00624eaau); i++) {
        uint32_t cnt = en2_r8(e, 0x00624eaau);
        uint32_t row = (cnt - 1) * 0x24 + 2 * i;   /* places by count: x[9] at 0x004e808c, z[9] after */
        int32_t x = (int16_t)en2_r16(e, 0x004e808cu + row);
        int32_t z = (int16_t)en2_r16(e, 0x004e809eu + row);
        uint32_t o = en2_r32(e, F + L_TABLE + 4 * i);
        w32(e, o + 0x20, 0);
        en2_w32(e, o + 0x24, (uint32_t)z << 3);
        en2_w32(e, o + 0x1c, (uint32_t)x << 3);
        int32_t r = (int32_t)en2_call0(e, F_RAND);
        int32_t turn = (int32_t)((uint32_t)(r % 10 - 5) << 12) / 360;   /* -5..4 degrees */
        o = en2_r32(e, F + L_TABLE + 4 * i);
        w32(e, o + 0x28, 0);
        w32(e, o + 0x2c, 0);
        w32(e, o + 0x30, turn);
        en2_w16(e, en2_r32(e, F + L_TABLE + 4 * i), (uint16_t)(0x2329 + (int8_t)en2_r8(e, 0x00624eb5u + i)));
    }
    cmd_head(e, F + L_WAIT, 30, VT_WAIT_TIMEOUT, EN3_SCENE);
    w32(e, F + L_WAIT + 0x10, 0);
    w32(e, F + L_WAIT + 0x14, 0);
    cmd_scene_u8(e, F + L_U8, 0);
    cmd_scene(e, F + L_SET, VT_SET);
    w32(e, F + L_SET + 0x10, 0);
    for (uint32_t k = 0; k < 9; k++) {
        uint32_t c = F + k_lineup_move[k];
        cmd_head(e, c, 15, VT_MOVE_TURN, F + k_lineup_obj[k]);
        w32(e, c + 0x10, 0);
        w32(e, c + 0x14, lineup_step(en2_r8(e, 0x00624eaau)) * 8);
        for (uint32_t f = 0x18; f <= 0x24; f += 4) w32(e, c + f, 0);
    }
    cmd_text(e, F + L_TEXT, 3, en2_r32(e, 0x00626010u));

    for (uint32_t i = 0; i < en2_r8(e, 0x00624eaau); i++)
        en2_call2(e, F_ADD_DECOR, EN3_SCENE, en2_r32(e, F + L_TABLE + 4 * i));
    en2_w32(e, EN3_SCENE + 0x4c, F + O_IMAGE);
    en2_call5(e, F_ADD_ACTION, EN3_SCENE, 2, 0, 2, F + L_TEXT);
    en2_call5(e, F_ADD_ACTION, EN3_SCENE, 0, 0, 0, F + L_SET);
    en2_call5(e, F_ADD_ACTION, EN3_SCENE, 0, 0, 0, F + L_WAIT);
    en2_call5(e, F_ADD_ACTION, EN3_SCENE, 0, 0, 0, F + k_lineup_move[0]);
    /* each finished move starts the next one; after the last opponent's: stop waiting, run L_U8 */
    for (uint32_t k = 1; k < 9; k++) {
        uint32_t prev = F + k_lineup_move[k - 1];
        if (en2_r8(e, 0x00624eaau) > k) {
            en2_call5(e, F_ADD_ACTION, EN3_SCENE, 2, prev, 0, F + k_lineup_move[k]);
        } else {
            en2_call5(e, F_ADD_ACTION, EN3_SCENE, 2, prev, 1, F + L_WAIT);
            en2_call5(e, F_ADD_ACTION, EN3_SCENE, 2, prev, 0, F + L_U8);
        }
    }
    if (en2_r8(e, 0x00624eaau) == 9) {
        en2_call5(e, F_ADD_ACTION, EN3_SCENE, 2, F + k_lineup_move[8], 1, F + L_WAIT);
        en2_call5(e, F_ADD_ACTION, EN3_SCENE, 2, F + k_lineup_move[8], 0, F + L_U8);
    }
    en2_call5(e, F_ADD_ACTION, EN3_SCENE, 3, 0, 1, F + L_WAIT);
    en2_call5(e, F_ADD_ACTION, EN3_SCENE, 3, 0, 0, F + L_U8);
    en2_call5(e, F_ADD_ACTION, EN3_SCENE, 2, F + L_WAIT, 0, F + L_U8);
    en2_call2(e, F_LOAD_RES, EN3_SCENE, 0x004e2a34u);
    en2_call1(e, F_IMAGE_LOAD, F + O_IMAGE);
    en2_call1(e, F_SCENE_RUN, EN3_SCENE);
    return 0x00485100u;   /* screenMatchEnd */
}

/* ---- 0x0048429c: screenHistory ---- */
static const struct { uint8_t ev; uint16_t src; uint8_t type; uint16_t cmd; } k_history_actions[] = {
    {7, 0, 2, 0x41c}, {7, 0, 2, 0x564}, {7, 0, 2, 0x518}, {8, 0, 2, 0x41c}, {8, 0, 2, 0x554}, {8, 0, 2, 0x518},
    {3, 0, 2, 0x43c}, {3, 0, 2, 0x518}, {3, 0, 1, 0x3f8}, {3, 0, 0, 0x504},
    {0xe, 0, 2, 0x43c}, {0xe, 0, 2, 0x518}, {0xe, 0, 1, 0x3f8}, {0xe, 0, 0, 0x504},
    {0x11, 0x308, 2, 0x43c}, {0x11, 0x308, 1, 0x3f8}, {0x11, 0x308, 0, 0x504},
    {0xf, 0, 2, 0x43c}, {0xf, 0, 1, 0x3f8}, {0xf, 0, 0, 0x52c},
    {0x11, 0x358, 2, 0x43c}, {0x11, 0x358, 1, 0x3f8}, {0x11, 0x358, 0, 0x52c},
    {0x10, 0, 2, 0x43c}, {0x10, 0, 1, 0x3f8}, {0x10, 0, 0, 0x540},
    {0x11, 0x3a8, 2, 0x43c}, {0x11, 0x3a8, 1, 0x3f8}, {0x11, 0x3a8, 0, 0x540},
    {0, 0, 0, 0x518}, {0, 0, 0, 0x45c}, {0, 0, 0, 0x478},
    {2, 0x45c, 2, 0x41c}, {2, 0x45c, 2, 0x4cc}, {2, 0x45c, 0, 0x3f8}, {2, 0x45c, 1, 0x518},
    {7, 0x2b4, 2, 0x494}, {7, 0x2b4, 2, 0x4b0}, {8, 0x2b4, 2, 0x494}, {8, 0x2b4, 2, 0x4b0},
    {7, 0x260, 2, 0x4e8}, {7, 0x260, 2, 0x4cc}, {8, 0x260, 2, 0x4e8}, {8, 0x260, 2, 0x4cc},
    {2, 0x3f8, 2, 0x540},
};

uint32_t en3_screen_history(En2Env* e)
{
    const uint32_t F = e->scratch;
    enum { H_MOVIE = 0x2b4, H_TEXT = 0x260, H_AREA_A = 0x308, H_AREA_B = 0x358, H_AREA_C = 0x3a8 };
    en2_call5(e, F_SCENE_INIT, EN3_SCENE, 0x004780fcu, 0x004824c8u /* back: options */, 0x00477b90u, 0);
    w32(e, EN3_SCENE + 0x10, 0);
    w32(e, EN3_SCENE + 0x14, -0x4b0);
    w32(e, EN3_SCENE + 0x18, 0);
    en2_w16(e, EN3_SCENE + 0x1c, 0);
    en2_w16(e, EN3_SCENE + 0x1e, 0);
    en2_w16(e, EN3_SCENE + 0x20, 0);
    en2_call3(e, F_FILE_PATH, F + O_PATH, 0x004e2a69u, 1);
    en2_call3(e, F_IMAGE_INIT, F + O_IMAGE, F + O_PATH, 0x2e630);

    /* two rows: the history movie (0x00484fbc) and the history text (0x00485040) */
    menu_item(e, F + H_MOVIE, 0x2329, -3488, 0x2d0, 0x4b, 0x69, 0x00484fbcu);
    menu_item(e, F + H_TEXT, 0x232a, -1248, 0x160, 0x1d, 0x3b, 0x00485040u);
    hot_area(e, F + H_AREA_A, -300, -210, -170, -190);
    hot_area(e, F + H_AREA_B, -100, -210, 100, -190);
    hot_area(e, F + H_AREA_C, 170, -210, 300, -190);
    cmd_wait(e, F + 0x3f8, 0);
    uint32_t font = en2_r32(e, 0x00626010u);
    cmd_text(e, F + 0x41c, 0x1c, font);
    cmd_text(e, F + 0x43c, 0x1d, font);
    cmd_move(e, F + 0x45c, F + H_MOVIE, 160);
    cmd_move(e, F + 0x478, F + H_TEXT, -160);
    cmd_choice_step(e, F + 0x494, F + H_MOVIE, -1);
    cmd_choice_step(e, F + 0x4b0, F + H_TEXT, 1);
    cmd_choice_step(e, F + 0x4cc, F + H_MOVIE, 1);
    cmd_choice_step(e, F + 0x4e8, F + H_TEXT, -1);
    cmd_scene_u8(e, F + 0x504, 0);
    cmd_scene(e, F + 0x518, VT_SET);
    w32(e, F + 0x518 + 0x10, 0);
    cmd_scene_u8(e, F + 0x52c, 1);
    cmd_scene_u8(e, F + 0x540, 2);
    cmd_scene(e, F + 0x554, VT_NEXT);
    cmd_scene(e, F + 0x564, VT_PREV);

    en2_w32(e, EN3_SCENE + 0x4c, F + O_IMAGE);
    en2_call2(e, F_ADD_ITEM, EN3_SCENE, F + H_MOVIE);
    en2_call2(e, F_ADD_ITEM, EN3_SCENE, F + H_TEXT);
    en2_call2(e, F_ADD_DECOR, EN3_SCENE, F + H_AREA_A);
    en2_call2(e, F_ADD_DECOR, EN3_SCENE, F + H_AREA_B);
    en2_call2(e, F_ADD_DECOR, EN3_SCENE, F + H_AREA_C);
    for (unsigned i = 0; i < sizeof k_history_actions / sizeof k_history_actions[0]; i++) {
        uint32_t src = k_history_actions[i].src ? F + k_history_actions[i].src : 0;
        en2_call5(e, F_ADD_ACTION, EN3_SCENE, k_history_actions[i].ev, src, k_history_actions[i].type,
                  F + k_history_actions[i].cmd);
    }
    en2_call2(e, F_LOAD_RES, EN3_SCENE, 0x004e2a7eu);
    en2_call1(e, F_IMAGE_LOAD, F + O_IMAGE);
    return en2_call1(e, F_SCENE_RUN, EN3_SCENE);
}

/* ---- 0x00480c8c: screenWorldGlobe ---- */
#define VT_TEXT_OBJ 0x004e5740u
#define VT_AUX      0x004e57a8u
#define VT_TURN     0x004e5778u   /* scene_cmd_turn 0x0046a878 */
#define F_LEVEL_NAME 0x00474b8cu  /* levelDisplayName (game state, level) */
#define F_PALETTE    0x00452d28u  /* game_set_palette_mode (empty) */
#define F_ADD_AUX    0x0046a114u
#define F_SOUND_SYNC 0x0047314cu
#define F_FONT_COLOR 0x00452d48u  /* fontColorByteSet */

static double rd64(En2Env* e, uint32_t a) { double d; memcpy(&d, e->mem(e->user, a), 8); return d; }

/* the frame: the original addresses it from ebp = frame + 0x43e */
enum {
    G_TEXT = 0x260, G_GLOBE = 0x2c0, G_MARK = 0x314, G_SKY = 0x364, G_WAIT = 0x3b4, G_TITLE = 0x3d4,
    G_ROUTE = 0x3f4, G_TURN = 0x414, G_U8 = 0x430, G_SET = 0x444,
};
/* the eight level markers (ids 0x232b..0x2332) in level order, and where each sits in the frame */
static const uint16_t k_globe_aux[8] = {0x488, 0x470, 0x47c, 0x458, 0x4a0, 0x464, 0x4ac, 0x494};
static const uint16_t k_globe_aux_id[8] = {0x232b, 0x232e, 0x232d, 0x232c, 0x2332, 0x232f, 0x2331, 0x2330};

uint32_t en3_screen_world_globe(En2Env* e)
{
    const uint32_t F = e->scratch;
    int32_t level = (int8_t)en2_r8(e, 0x00624ec7u);
    if (en2_r8(e, 0x00624ed1u) == 0 || level > 8) return 0x004813e8u;   /* straight to the line up */
    en2_call5(e, F_SCENE_INIT, EN3_SCENE, 0x004780fcu, 0x004780fcu, 0x00477b90u, 0);
    en2_call3(e, F_FILE_PATH, F + O_PATH, 0x004e29e3u, 1);
    en2_call3(e, F_IMAGE_INIT, F + O_IMAGE, F + O_PATH, 0x2e630);

    /* the per level globe angles: i16 at 0x004e8068 + 4 * level (level - 1 for this level) */
    int32_t lon_prev = (int16_t)en2_r16(e, 0x004e8068u + 4u * (uint32_t)(level - 1));
    /* the globe, turned to the previous level */
    obj_init(e, F + G_GLOBE, 0x2328, VT_ITEM, 1, 1, 0xffff, 0, 0);
    w32(e, F + G_GLOBE + 0x20, 0x7d00);
    w32(e, F + G_GLOBE + 0x28, (int32_t)((uint32_t)lon_prev << 12) / 360);
    w32(e, F + G_GLOBE + 0x44, 0);
    en2_w32(e, F + G_GLOBE + 0x50, 0x004813e8u);
    obj_init(e, F + G_SKY, 0x2329, VT_DECOR, 1, 1, 0xffff, 0, 0);
    w32(e, F + G_SKY + 0x20, 0x7d00);
    w32(e, F + G_SKY + 0x44, 0);
    /* the marker's distance: trunc(i16 at 0x004e806a + 4 * level * 0x004e2a07 (2/7) * 0x004e2a0f (8))
     * (x87 at 53 bits, the original's float_round 0x004979a2 chops) */
    double d = (double)(int16_t)en2_r16(e, 0x004e806au + 4u * (uint32_t)level);
    d = d * rd64(e, 0x004e2a07u);
    d = d * rd64(e, 0x004e2a0fu);
    obj_init(e, F + G_MARK, 0x232a, VT_DECOR, 1, 0, 0, 0, (int32_t)d);
    w32(e, F + G_MARK + 0x44, 0);
    uint32_t name = en2_call2(e, F_LEVEL_NAME, 0x00624ea8u, (uint32_t)(int32_t)(int8_t)en2_r8(e, 0x00624ec7u));

    /* the level's name, centered at 320, 450 in font color 6 */
    obj_init(e, F + G_TEXT, 0, VT_TEXT_OBJ, 1, 1, 0xffff, 0, 0);
    w32(e, F + G_TEXT + 0x44, 0);
    en2_w8(e, F + G_TEXT + 0x50, 6);
    en2_w32(e, F + G_TEXT + 0x54, name);
    en2_w8(e, F + G_TEXT + 0x58, 1);
    en2_w16(e, F + G_TEXT + 0x5a, 0x140);
    en2_w16(e, F + G_TEXT + 0x5c, 0x1c2);
    en2_call4(e, F_PALETTE, 6, 0xff, 0xa1, 0);

    for (int k = 0; k < 8; k++) {
        uint32_t a = F + k_globe_aux[k];
        en2_w16(e, a, k_globe_aux_id[k]);
        w32(e, a + 4, 0);
        en2_w32(e, a + 8, VT_AUX);
    }
    for (int k = 0; k < 8; k++) en2_w32(e, F + G_ROUTE + 4u * (uint32_t)k, F + k_globe_aux[k]);
    uint8_t mode = en2_r8(e, 0x00624ea8u);
    cmd_head(e, F + G_WAIT, -1, VT_WAIT_TIMEOUT, EN3_SCENE);
    w32(e, F + G_WAIT + 0x10, (mode == 4 || mode == 2) ? 2 : 0);
    w32(e, F + G_WAIT + 0x14, 0);
    cmd_scene_u8(e, F + G_U8, 0);
    cmd_scene(e, F + G_SET, VT_SET);
    w32(e, F + G_SET + 0x10, 0);
    /* the globe turns from the previous level to this one, one step of 11 (= 1 degree) a frame */
    int32_t diff = (int16_t)en2_r16(e, 0x004e8068u + 4u * (uint32_t)level) - lon_prev;
    int32_t frames = diff < 0 ? -diff : diff;
    cmd_head(e, F + G_TURN, frames, VT_TURN, F + G_GLOBE);
    w32(e, F + G_TURN + 0x10, (int32_t)((uint32_t)(diff > 0 ? 1 : -1) << 12) / 360);
    w32(e, F + G_TURN + 0x14, 0);
    w32(e, F + G_TURN + 0x18, 0);
    cmd_text(e, F + G_TITLE, 0x1d, en2_r32(e, 0x00626010u));

    en2_w32(e, EN3_SCENE + 0x4c, F + O_IMAGE);
    en2_call2(e, F_ADD_ITEM, EN3_SCENE, F + G_GLOBE);
    en2_call2(e, F_ADD_DECOR, EN3_SCENE, F + G_SKY);
    en2_call2(e, F_ADD_DECOR, EN3_SCENE, F + G_MARK);
    en2_call2(e, F_ADD_DECOR, EN3_SCENE, F + G_TEXT);
    for (int k = 0; k < 8; k++) en2_call2(e, F_ADD_AUX, EN3_SCENE, F + k_globe_aux[k]);
    en2_call5(e, F_ADD_ACTION, EN3_SCENE, 0, 0, 0, F + G_SET);
    en2_call5(e, F_ADD_ACTION, EN3_SCENE, 0, 0, 0, F + G_WAIT);
    en2_call5(e, F_ADD_ACTION, EN3_SCENE, 0, 0, 0, F + G_TURN);
    en2_call5(e, F_ADD_ACTION, EN3_SCENE, 3, 0, 2, F + G_TITLE);
    en2_call5(e, F_ADD_ACTION, EN3_SCENE, 3, 0, 1, F + G_WAIT);
    en2_call5(e, F_ADD_ACTION, EN3_SCENE, 3, 0, 0, F + G_U8);
    en2_call5(e, F_ADD_ACTION, EN3_SCENE, 2, F + G_TURN, 0, F + G_U8);
    en2_call2(e, F_LOAD_RES, EN3_SCENE, 0x004e29f6u);
    en2_call1(e, F_IMAGE_LOAD, F + O_IMAGE);
    /* tournament / co-op: the levels already won are lit */
    mode = en2_r8(e, 0x00624ea8u);
    if (mode == 1 || mode == 2) {
        for (int32_t i = 0; i < (int8_t)en2_r8(e, 0x00624ec7u) - 1; i++)
            en2_call2(e, F_CHOICE_SET, en2_r32(e, F + G_ROUTE + 4u * (uint32_t)i), 1);
    }
    /* the sound driver's play (sound_play 0x0046f8b0 inlined), flush and method 1 */
    uint32_t snd = en2_r32(e, 0x00626010u);
    {
        uint32_t v[7] = {snd, 0x1b, 0, 0, 0x63, 0xffffffffu, 0};
        e->call(e->user, en2_r32(e, en2_r32(e, snd + 0x1c)), v, 7);
    }
    en2_call1(e, F_SOUND_SYNC, en2_r32(e, 0x00626010u));
    snd = en2_r32(e, 0x00626010u);
    en2_call1(e, en2_r32(e, en2_r32(e, snd + 0x1c) + 4), snd);
    uint32_t next = en2_call1(e, F_SCENE_RUN, EN3_SCENE);
    en2_w32(e, F + G_TEXT + 8, VT_TEXT_OBJ);
    en2_call1(e, F_FONT_COLOR, en2_r8(e, F + G_TEXT + 0x50));
    return next;
}

/* ---- 0x00486f18: the audio configuration dialog ---- */
#define I_waveOutGetNumDevs  0x00c9062cu
#define I_waveOutGetDevCapsA 0x00c90620u
#define F_GAME_LOOP     0x004a8e6cu
#define F_SOUND_STOP    0x00472f74u
#define F_RENDER_BG     0x004a2ea0u
#define F_RENDER_CFG    0x004a2ffcu
#define F_DISPLAY_SYNC  0x00495b14u
#define F_LABEL_CREATE  0x004a4a24u   /* (w, hwnd, id, text; 5 stack) */
#define F_SET_KEYS      0x004a4c00u
#define F_SET_KEYS2     0x004a4d68u
#define F_SET_NAV_KEYS  0x004a4bdcu
#define F_SET_HANDLER   0x004a4458u   /* obj_set_54_5c (w, owner; fn, 0, 0) */
#define F_SHOW          0x004a6ee4u
#define F_COMBO_CREATE  0x004a5b34u
#define F_COMBO_HANDLER 0x004a593cu   /* obj_set_64_58 */
#define F_COMBO_ADD     0x004a5b6cu
#define F_COMBO_SEL     0x004a5d8cu
#define F_TRACK_CREATE  0x004a6648u
#define F_TRACK_RANGE   0x004a6844u
#define F_TRACK_415     0x004a690cu
#define F_TRACK_POS     0x004a6818u
#define F_NAV_LINKS     0x004a70acu   /* (w, up, down, left; right, ...) 4 regs + 3 stack */
#define F_APPLY_VOLUME  0x00486bb4u
#define F_SLIDERS_SET   0x0048859cu
#define F_FOCUS         0x004a6f80u
#define F_AUDIO_RUN     0x004877e4u
#define F_BRUSH_FREE    0x004a4104u
#define F_OVERLAY       0x00484260u

static void label(En2Env* e, uint32_t cfg, uint32_t off, uint32_t id, uint32_t text, uint32_t kind, uint32_t x, uint32_t y,
                  uint32_t key_a, uint32_t key_b, uint32_t fn)
{
    uint32_t w = cfg + off;
    uint32_t v[9] = {w, en2_r32(e, cfg), id, text, kind, 0x1b, x, y, 0x21};
    e->call(e->user, F_LABEL_CREATE, v, 9);
    if (kind == 3) {
        en2_call4(e, F_SET_KEYS, w, key_a, key_b, key_b);
        en2_call4(e, F_SET_KEYS2, w, key_a, key_b, key_b);
    } else {
        en2_call5(e, F_SET_NAV_KEYS, w, key_a, key_b, key_b, 0);
    }
    en2_call5(e, F_SET_HANDLER, w, cfg, fn, 0, 0);
    en2_call2(e, F_SHOW, w, 5);
}
static void trackbar(En2Env* e, uint32_t cfg, uint32_t off, uint32_t id, uint32_t y)
{
    uint32_t v[8] = {cfg + off, en2_r32(e, cfg), id, 0x10010005u, 0x16a, y, 0xcc, 0x21};
    e->call(e->user, F_TRACK_CREATE, v, 8);
}
static void nav(En2Env* e, uint32_t w, uint32_t a, uint32_t b, uint32_t c, uint32_t s0, uint32_t s1, uint32_t s2)
{
    uint32_t v[7] = {w, a, b, c, s0, s1, s2};
    e->call(e->user, F_NAV_LINKS, v, 7);
}

uint32_t en3_audio_config_screen(En2Env* e, uint32_t cfg)
{
    const uint32_t F = e->scratch;
    int32_t ndev = (int32_t)en2_call0(e, I_waveOutGetNumDevs);
    en2_call0(e, F_GAME_LOOP);
    en2_call1(e, F_SOUND_STOP, en2_r32(e, 0x00626010u));
    uint32_t disp = en2_r32(e, 0x00626014u);
    en2_call2(e, en2_r32(e, en2_r32(e, disp + 8) + 4), disp, 5);
    en2_call3(e, F_FILE_PATH, F, 0x004e2d31u, 1);
    en2_call3(e, F_RENDER_BG, 0x009f6cd8u, F, 0);
    en2_call4(e, F_RENDER_CFG, 0x009f6cd8u, 0, 0, 0);
    en2_call0(e, F_DISPLAY_SYNC);
    /* the current volumes = the sound card's row (cfg +0x4c + card * 20, 5 dwords) */
    uint32_t row = cfg + 0x4c + en2_r32(e, cfg + 0x1128) * 20;
    for (uint32_t i = 0; i < 20; i += 4) en2_w32(e, cfg + 0x88 + i, en2_r32(e, row + i));

    label(e, cfg, 0x48c, 0x65, 0x004e2d47u, 3, 0x78, 0xca, 0xe1, 0xe0, 0x004899a0u);
    label(e, cfg, 0x658, 0x66, 0x004e2d48u, 3, 0xa3, 0xfc, 0xdd, 0xdc, 0x004899a0u);
    label(e, cfg, 0x9f0, 0x67, 0x004e2d49u, 3, 0xce, 0xfc, 0xdb, 0xda, 0x004899a0u);
    label(e, cfg, 0x824, 0x68, 0x004e2d4au, 3, 0xf9, 0xb8, 0xd9, 0xd8, 0x004899a0u);
    label(e, cfg, 0xd88, 0x69, 0x004e2d4bu, 3, 0x124, 0x5f, 0xd7, 0xd6, 0x004899a0u);
    label(e, cfg, 0xbbc, 0x6a, 0x004e2d4cu, 3, 0x14f, 0x7c, 0xe3, 0xe2, 0x004899a0u);
    label(e, cfg, 0x2bc, 0x6b, 0x004e2d4du, 1, 0x17a, 0xf2, 0xdf, 0xde, 0x00489990u);   /* the OK button */

    /* the sound card list */
    const uint32_t cb = cfg + 0x9c;
    {
        uint32_t v[8] = {cb, en2_r32(e, cfg), 0x2bc, 0x10000003u, 0x16a, 0x78, 0xcc, 0xc8};
        e->call(e->user, F_COMBO_CREATE, v, 8);
    }
    en2_call5(e, F_COMBO_HANDLER, cb, cfg, 0x00489980u, 0, 0);
    for (int32_t i = 0; i < ndev; i++) {
        en2_call3(e, I_waveOutGetDevCapsA, (uint32_t)i, F + 0x12c, 0x30);   /* WAVEOUTCAPSA */
        en2_call2(e, F_COMBO_ADD, cb, F + 0x134);                           /* its szPname */
    }
    en2_call2(e, F_COMBO_SEL, cb, en2_r32(e, cfg + 0x1128));

    const uint32_t t1 = cfg + 0x118, t2 = cfg + 0x16c, t3 = cfg + 0x1c0, ok = cfg + 0x2bc;
    trackbar(e, cfg, 0x118, 0x2bd, 0xa3);
    trackbar(e, cfg, 0x16c, 0x2be, 0xf9);
    trackbar(e, cfg, 0x1c0, 0x2bf, 0xce);
    uint32_t tr[3] = {t1, t2, t3};
    for (int k = 0; k < 3; k++) en2_call4(e, F_TRACK_RANGE, tr[k], 0, 0x1999, 0);
    for (int k = 0; k < 3; k++) en2_call3(e, F_TRACK_415, tr[k], 0x28f, 0);
    for (int k = 0; k < 3; k++) en2_call2(e, F_TRACK_POS, tr[k], 0x28f);
    nav(e, cb, ok, t1, 0, 0, ok, t1);
    nav(e, t1, cb, t3, 0, 0, cb, t3);
    nav(e, t3, t1, t2, 0, 0, t1, t2);
    nav(e, t2, t3, ok, 0, 0, t3, ok);
    nav(e, ok, t2, cb, t2, cb, t2, cb);
    if (en2_r32(e, cfg + 0x1120) != 0) {   /* the mixer answered: two more sliders */
        const uint32_t t4 = cfg + 0x214, t5 = cfg + 0x268;
        trackbar(e, cfg, 0x214, 0x2c0, 0x124);
        trackbar(e, cfg, 0x268, 0x2c1, 0x14f);
        en2_call4(e, F_TRACK_RANGE, t4, 0, 0x1999, 0);
        en2_call4(e, F_TRACK_RANGE, t5, 0, 0x1999, 0);
        en2_call3(e, F_TRACK_415, t4, 0x28f, 0);
        en2_call3(e, F_TRACK_415, t5, 0x28f, 0);
        en2_call2(e, F_TRACK_POS, t4, 0x28f);
        en2_call2(e, F_TRACK_POS, t5, 0x28f);
        nav(e, t2, t3, t5, 0, 0, t3, t5);
        nav(e, t5, t2, t4, 0, 0, t2, t4);
        nav(e, t4, t5, ok, t5, ok, t5, ok);
        nav(e, ok, t4, cb, t4, cb, t4, cb);
    }
    en2_call2(e, F_APPLY_VOLUME, cfg, 2);
    en2_call1(e, F_SLIDERS_SET, cfg);
    en2_call1(e, F_FOCUS, t1);
    en2_w32(e, cfg + 0x1124, 0);
    en2_call1(e, F_AUDIO_RUN, cfg);
    static const uint16_t k_free_a[5] = {0x9c, 0x118, 0x16c, 0x1c0, 0x2bc};
    static const uint16_t k_free_b[6] = {0x48c, 0x658, 0x824, 0x9f0, 0xd88, 0xbbc};
    for (int k = 0; k < 5; k++) en2_call1(e, F_BRUSH_FREE, cfg + k_free_a[k]);
    if (en2_r32(e, cfg + 0x1120) != 0) {
        en2_call1(e, F_BRUSH_FREE, cfg + 0x214);
        en2_call1(e, F_BRUSH_FREE, cfg + 0x268);
    }
    for (int k = 0; k < 6; k++) en2_call1(e, F_BRUSH_FREE, cfg + k_free_b[k]);
    disp = en2_r32(e, 0x00626014u);
    en2_call2(e, en2_r32(e, en2_r32(e, disp + 8) + 8), disp, 0);
    en2_call1(e, F_OVERLAY, 0x1b);
    return 1;
}
