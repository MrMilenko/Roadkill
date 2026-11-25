/*
 * en7_e.c: see en7_e.h.
 */
#include "en7_e.h"

#define POOL 0x00a3d150u   /* trie nodes of 0x204 bytes: +0 code (-1 = none), +4 + 4 k child */

static uint32_t new_node(En2Env* e)
{
    uint32_t k = en2_r32(e, 0x00a5d34cu);
    uint32_t n = POOL + k * 0x204u;
    en2_w32(e, 0x00a5d34cu, k + 1);
    en2_w32(e, n, 0xffffffffu);
    for (uint32_t a = n + 4; a <= n + 0x200; a += 4) en2_w32(e, a, 0);
    return n;
}

/* 0x0049de80: builds a key sequence trie from s (len bytes: key bytes, a 0, the code byte, ...)
 * with the callback cb (0x00a5d350): a new root from the node pool (count 0x00a5d34c) becomes
 * 0x00a5d36c; each key goes down (a missing child is a new node; the pool full at 0xff stops); a 0
 * stores the following byte as the code of the node reached and restarts at the root. The cursor
 * 0x00a5d368 = the root. Returns the number of codes. ORIGINAL BUG: a node has 128 child slots
 * (0x204 bytes) but keys index up to 255, reaching into the next node; verified for keys below 0x80. (0x0049deca / 0x0049decc, a null root, are
 * dead.) */
uint32_t en7_key_sequences_build(En2Env* e, uint32_t s, uint32_t len, uint32_t cb)
{
    en2_w32(e, 0x00a5d350u, cb);
    uint32_t root = new_node(e);
    en2_w32(e, 0x00a5d36cu, root);
    uint32_t cur = root, codes = 0;
    int done = 0;
    for (int32_t i = 0; !done;) {
        uint8_t c = en2_r8(e, s);
        if (c == 0) {
            i++;
            uint8_t code = en2_r8(e, s + 1);
            s++;
            codes++;
            en2_w32(e, cur, code);
            cur = en2_r32(e, 0x00a5d36cu);
        } else {
            uint32_t nx = en2_r32(e, cur + 4u * c + 4);
            if (nx == 0) {
                nx = new_node(e);
                en2_w32(e, cur + 4u * en2_r8(e, s) + 4, nx);
                if ((int32_t)en2_r32(e, 0x00a5d34cu) >= 0xff) done = 1;
            }
            cur = nx;
        }
        i++;
        s++;
        if (i >= (int32_t)len) done = 1;
    }
    en2_w32(e, 0x00a5d368u, en2_r32(e, 0x00a5d36cu));
    return codes;
}

/* 0x0049e4ac: the keyboard by GetDeviceState (+0x24; 0x100 bytes into 0x00a5d610) of 0x00a5d378:
 * DIERR_INPUTLOST -> acquired flag 0x00a5d5f0 = 0 and Acquire (+0x1c), 0x004937b8 when that or any
 * other read fails. Read: pressed keys (bit 7) become 1, the arrow bytes 0x48 / 0x4b / 0x4d / 0x50
 * move to 0xc8 / 0xcb / 0xcd / 0xd0, shift / alt / control flags 0x00a5d5f8 / 5fc / 600, caps lock
 * toggling 0x00a5d604 once per press (0x00a5d608). */
void en7_keyboard_read_state(En2Env* e)
{
    const uint32_t ST = 0x00a5d610u;
    uint32_t d = en2_r32(e, 0x00a5d378u);
    uint32_t r = en2_call3(e, en2_r32(e, en2_r32(e, d) + 0x24), d, 0x100, ST);
    if (r != 0) {
        if (r == 0x8007001eu) {
            d = en2_r32(e, 0x00a5d378u);
            en2_w32(e, 0x00a5d5f0u, 0);
            if (en2_call1(e, en2_r32(e, en2_r32(e, d) + 0x1c), d) == 0) return;
        }
        en2_call0(e, 0x004937b8u);
        return;
    }
    for (uint32_t k = 0; k < 0x100; k++)
        if (en2_r8(e, ST + k) & 0x80) en2_w8(e, ST + k, (uint8_t)(en2_r8(e, ST + k) >> 7));
    static const uint8_t mv[4] = {0x48, 0x4b, 0x4d, 0x50};
    for (int k = 0; k < 4; k++) {
        uint8_t b = en2_r8(e, ST + mv[k]);
        if (b != 0) { en2_w8(e, ST + mv[k] + 0x80, b); en2_w8(e, ST + mv[k], 0); }
    }
    en2_w32(e, 0x00a5d5f8u, (en2_r8(e, ST + 0x2a) || en2_r8(e, ST + 0x36)) ? 1 : 0);
    en2_w32(e, 0x00a5d5fcu, (en2_r8(e, ST + 0x38) || en2_r8(e, ST + 0xb8)) ? 1 : 0);
    en2_w32(e, 0x00a5d600u, (en2_r8(e, ST + 0x1d) || en2_r8(e, ST + 0x9d)) ? 1 : 0);
    if (en2_r8(e, ST + 0x3a) != 0 && en2_r32(e, 0x00a5d608u) != 1) {
        uint32_t t = en2_r32(e, 0x00a5d604u);
        en2_w32(e, 0x00a5d608u, 1);
        en2_w32(e, 0x00a5d604u, ~t);
    } else if (en2_r8(e, ST + 0x3a) == 0) {
        en2_w32(e, 0x00a5d608u, 0);
    }
}

/* 0x004a798c: a widget's default key handler (navigation, only when +0x18 is set and no dialog
 * runs, 0x00bdfaec): up / down / left / right / tab / shift tab = links 0..5 (+0x1c + 4 d); the
 * first enabled widget (GWL_STYLE without WS_DISABLED) along that link, at most 200 steps and not
 * back to o, gets the focus (SetFocus, GetPropA "ST Object") -> 0; other keys or no target -> 2.
 * (0x004a7a6a / 0x004a7a6c, a link number out of 0..5 inside the walk, are dead.) */
uint32_t en7_widget_navigate(En2Env* e, uint32_t o, uint32_t key)
{
    if (en2_r32(e, o + 0x18) == 0 || en2_r32(e, 0x00bdfaecu) != 0) return 2;
    int32_t d;
    switch (key) {
    case 0x26: d = 0; break;
    case 0x28: d = 1; break;
    case 0x25: d = 2; break;
    case 0x27: d = 3; break;
    case 9: d = (en2_call1(e, 0x00c904b8u, 0x10) & 0x8000) ? 4 : 5; break;
    default: return 2;
    }
    uint32_t cur = en2_r32(e, o + 0x1c + 4u * (uint32_t)d);
    if (cur == 0) return 2;
    for (int32_t i = 0; cur != 0 && cur != o && i < 0xc8; i++) {
        uint32_t st = en2_call2(e, EN2_GetWindowLongA, en2_r32(e, cur), (uint32_t)-16);
        if ((st & 0x08000000u) == 0) break;
        cur = en2_r32(e, cur + 0x1c + 4u * (uint32_t)d);
    }
    if (cur != 0 && cur != o) {
        uint32_t p = en2_call1(e, 0x00c90560u, en2_r32(e, cur));
        en2_call2(e, 0x00c904e0u, p, 0x004e3194u);
    }
    return 0;
}
