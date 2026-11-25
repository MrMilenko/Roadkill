/*
 * en7_d.c: see en7_d.h.
 */
#include "en7_d.h"

#define EN7_timeGetTime        0x00c90640u
#define EN7_WaitForSingleObject 0x00c9030cu
#define EN7_ReleaseMutex       0x00c902fcu
#define J 0x00a82164u

static uint32_t m(En2Env* e, uint32_t obj, uint32_t off) { return en2_r32(e, en2_r32(e, obj) + off); }

/* 0x004a247c: the joystick state (device 2 a joystick, input_get_device_type): unless held off
 * (0x00a82160, cleared when 0x0049e108 answers), the joystick 0x00a82164 is polled (0x004c407c) and
 * the bytes 0x00a8214d..50 get the axis values when x > 0 / x < 0 / y > 0 / y < 0 (each asked again,
 * low byte), 0x00a82151..53 the three buttons; then while 0x0049e108 says nothing the hold starts
 * (0x00a82160 = 1, clock 0x00a82154 = timeGetTime). Held: 0x00a82158 now, 0x00a8215c elapsed; past
 * 0x004e8498 ms the hold ends. */
void en7_joy_state_read(En2Env* e)
{
    if ((en2_call1(e, 0x004932a0u, 2) & 0xff) != 1) return;
    if (en2_call0(e, 0x0049e108u) != 0) en2_w32(e, 0x00a82160u, 0);
    if (en2_r32(e, 0x00a82160u) == 0) {
        en2_call1(e, 0x004c407cu, J);
        if ((int32_t)en2_call1(e, 0x004c4130u, J) > 0) en2_w8(e, 0x00a8214du, (uint8_t)en2_call1(e, 0x004c4130u, J));
        if ((int32_t)en2_call1(e, 0x004c4130u, J) < 0) en2_w8(e, 0x00a8214eu, (uint8_t)en2_call1(e, 0x004c4130u, J));
        if ((int32_t)en2_call1(e, 0x004c414cu, J) > 0) en2_w8(e, 0x00a8214fu, (uint8_t)en2_call1(e, 0x004c414cu, J));
        if ((int32_t)en2_call1(e, 0x004c414cu, J) < 0) en2_w8(e, 0x00a82150u, (uint8_t)en2_call1(e, 0x004c414cu, J));
        en2_w8(e, 0x00a82151u, (uint8_t)en2_call1(e, 0x004c41c8u, J));
        en2_w8(e, 0x00a82152u, (uint8_t)en2_call1(e, 0x004c4208u, J));
        en2_w8(e, 0x00a82153u, (uint8_t)en2_call1(e, 0x004c41e8u, J));
        if (en2_call0(e, 0x0049e108u) != 0) return;
        en2_w32(e, 0x00a82160u, 1);
        en2_w32(e, 0x00a82154u, en2_call0(e, EN7_timeGetTime));
        return;
    }
    uint32_t t = en2_call0(e, EN7_timeGetTime);
    en2_w32(e, 0x00a82158u, t);
    int32_t el = (int32_t)(t - en2_r32(e, 0x00a82154u));
    en2_w32(e, 0x00a8215cu, (uint32_t)el);
    if (el > (int32_t)en2_r32(e, 0x004e8498u)) en2_w32(e, 0x00a82160u, 0);
}

/* 0x004ab2f0: under the mutex +0x1f4, removes the players marked gone (+0 != 0) from the list
 * {head +0x214, tail +0x218, count +0x220, next +0x1cc}: their list box rows (+0x224: 0x004a54b4
 * count, 0x004a54fc item, 0x004a5430 delete) and the node (0x00497a28(node + 0x1b0, 0), freed as
 * its answer - 0x1b0). ORIGINAL BUG kept: after a removal the "previous" node is set to the next
 * node itself (not the last one kept), so a second removal in a row relinks through the wrong node. */
void en7_player_list_purge(En2Env* e, uint32_t o)
{
    en2_call2(e, EN7_WaitForSingleObject, en2_r32(e, o + 0x1f4), 0xffffffffu);
    uint32_t prev = 0, cur = en2_r32(e, o + 0x214);
    while (cur != 0) {
        if (en2_r32(e, cur) == 0) {
            prev = cur;
            cur = en2_r32(e, cur + 0x1cc);
            continue;
        }
        uint32_t lb = en2_r32(e, o + 0x224);
        if (lb != 0) {
            int32_t n = (int32_t)en2_call1(e, 0x004a54b4u, lb);
            for (int32_t i = 0; i < n; i++)
                if (en2_call3(e, 0x004a54fcu, en2_r32(e, o + 0x224), (uint32_t)i, (uint32_t)i) == cur)
                    en2_call2(e, 0x004a5430u, en2_r32(e, o + 0x224), (uint32_t)i);
        }
        if (cur == en2_r32(e, o + 0x214)) {
            prev = en2_r32(e, cur + 0x1cc);
            en2_w32(e, o + 0x214, prev);
        } else {
            uint32_t nx = en2_r32(e, cur + 0x1cc);
            en2_w32(e, prev + 0x1cc, nx);
            if (nx == 0) en2_w32(e, o + 0x218, prev);
            prev = en2_r32(e, prev + 0x1cc);
        }
        en2_call1(e, 0x004aa39cu, en2_call2(e, 0x00497a28u, cur + 0x1b0, 0) - 0x1b0);
        en2_w32(e, o + 0x220, en2_r32(e, o + 0x220) - 1);
        cur = prev;
    }
    en2_call1(e, EN7_ReleaseMutex, en2_r32(e, o + 0x1f4));
}

static void del(En2Env* e, uint32_t p, uint32_t fn)
{
    if (p != 0) en2_call1(e, 0x004aa39cu, en2_call2(e, fn, p, 0));
}

/* 0x004aa450: frees the sound object's parts: sample +0x38 (0x00497a28), list +0x2c (0x004acd80),
 * +0x30 (0x004ab9d0), +0x34 (0x004984e0) (each destructor then 0x004aa39c), the buffer +0x24 (method
 * +0x10, Release) and +0x28 (Release), both cleared. The same buffer block follows a second time;
 * with both pointers just cleared its calls (0x004aa4f0..0x004aa501, 0x004aa50f..0x004aa515) never
 * run. Returns o. */
uint32_t en7_sound_object_free(En2Env* e, uint32_t o)
{
    del(e, en2_r32(e, o + 0x38), 0x00497a28u);
    del(e, en2_r32(e, o + 0x2c), 0x004acd80u);
    del(e, en2_r32(e, o + 0x30), 0x004ab9d0u);
    del(e, en2_r32(e, o + 0x34), 0x004984e0u);
    for (int k = 0; k < 2; k++) {
        uint32_t b = en2_r32(e, o + 0x24);
        if (b != 0) {
            en2_call1(e, m(e, b, 0x10), b);
            b = en2_r32(e, o + 0x24);
            en2_call1(e, m(e, b, 8), b);
            en2_w32(e, o + 0x24, 0);
        }
        uint32_t p = en2_r32(e, o + 0x28);
        if (p != 0) {
            en2_call1(e, m(e, p, 8), p);
            en2_w32(e, o + 0x28, 0);
        }
    }
    return o;
}

/* 0x004ad624: outlines the rectangle 0x00c7fa84 (left; 0 = none) / 0x00c7fa88 (right) / 0x00c7fa8c
 * (top) / 0x00c7fa90 (bottom) one pixel outside on the locked 16 bit screen (0x004961b8 /
 * 0x004961d4; pointer 0x00a04674, pitch 0x00a04670) in the color 0x00496590(r, g, b): top row left
 * to right, right side down, bottom row right to left, left side up (pitch in 16 bit words =
 * pitch / 2, signed). */
void en7_screen_rect_outline(En2Env* e, uint32_t r, uint32_t g, uint32_t b)
{
    if (en2_r32(e, 0x00c7fa84u) == 0) return;
    en2_call0(e, 0x004961b8u);
    int32_t pw = (int32_t)en2_r32(e, 0x00a04670u) / 2;
    uint16_t c = (uint16_t)en2_call3(e, 0x00496590u, r & 0xff, g & 0xff, b & 0xff);
    uint32_t L = en2_r32(e, 0x00c7fa84u), R = en2_r32(e, 0x00c7fa88u), T = en2_r32(e, 0x00c7fa8cu),
             B = en2_r32(e, 0x00c7fa90u);
    uint32_t p = en2_r32(e, 0x00a04674u) + 2 * L - 2 + 2 * ((T - 1) * (uint32_t)pw);
    for (int32_t n = (int32_t)(R - L + 2); n > 0; n--) { p += 2; en2_w16(e, p - 2, c); }
    for (int32_t n = (int32_t)(B - T + 2); n > 0; n--) { en2_w16(e, p, c); p += 2 * (uint32_t)pw; }
    for (int32_t n = (int32_t)(R - L + 2); n > 0; n--) { p -= 2; en2_w16(e, p + 2, c); }
    for (int32_t n = (int32_t)(B - T + 2); n > 0; n--) { en2_w16(e, p, c); p -= 2 * (uint32_t)pw; }
    en2_call0(e, 0x004961d4u);
}

/* 0x00497d2c: syncs the list box lb (0x004a54b4 count, 0x004a54fc item data, 0x004a5430 delete,
 * 0x004a53c4 add) with the keyed list o (next +0x134): rows whose data is no node of the list are
 * deleted (the count asked again), then every node not yet a row is added with its name +0x94
 * formatted by path_sprintf 0x004b27e6 (cdecl; format 0x004e2f5a) into a stack buffer. */
void en7_session_listbox_sync(En2Env* e, uint32_t o, uint32_t lb)
{
    const uint32_t buf = e->scratch;
    int32_t n = (int32_t)en2_call1(e, 0x004a54b4u, lb);
    if (n > 0) {
        int32_t i = 0, done = 0;
        while (!done) {
            uint32_t it = en2_call3(e, 0x004a54fcu, lb, (uint32_t)i, o);
            int found = 0;
            for (uint32_t p = en2_r32(e, o); p != 0 && !found; p = en2_r32(e, p + 0x134))
                if (p == it) found = 1;
            if (!found) {
                en2_call2(e, 0x004a5430u, lb, (uint32_t)i);
                n = (int32_t)en2_call1(e, 0x004a54b4u, lb);
            } else {
                i++;
            }
            if (i == n) done = 1;
        }
    }
    for (uint32_t s = en2_r32(e, o); s != 0; s = en2_r32(e, s + 0x134)) {
        int32_t k = (int32_t)en2_call1(e, 0x004a54b4u, lb);
        int have = 0;
        for (int32_t j = 0; j < k; j++)
            if (en2_call3(e, 0x004a54fcu, lb, (uint32_t)j, (uint32_t)j) == s) have = 1;
        if (!have) {
            uint32_t v[5] = {buf, buf, buf, 0x004e2f5au, s + 0x94};
            e->call(e->user, 0x004b27e6u, v, 5);
            en2_call3(e, 0x004a53c4u, lb, buf, s);
        }
    }
}
