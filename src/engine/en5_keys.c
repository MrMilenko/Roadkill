/*
 * en5_keys.c: see en5_keys.h.
 *
 * Key queue: bytes 0x00a5d910 (0x100), write index 0x00a5da1c, read index 0x00a5da14 (pushed
 * forward when the queue is full), count 0x00a5da10. Key state 0x00a5d710 (0x100 bytes: 0x80 held,
 * 1 = pressed and released in this frame, 0 up).
 */
#include "en5_keys.h"

#define EN5_timeGetTime 0x00c90640u
#define CUR  0x00a5d368u
#define ROOT 0x00a5d36cu
#define CB   0x00a5d350u

/* 0x0049dfac: feeds key k to the key sequence trie (nodes: +0 code, +4 + 4 k child): the cursor
 * 0x00a5d368 (0 = off) moves to the child; at the root the sequence clock 0x00a5d354 starts.
 * A node with a code >= 0 ends the sequence: from inside the trie the average time per key
 * (0x00a5d358 now, 0x00a5d35c elapsed, 0x00a5d360 = elapsed / keys (idiv), 0x00a5d364 keys) must
 * be below 500 ms for the callback 0x00a5d350(code); a sequence starting at the root calls it
 * at once; either way the cursor goes back to the root. A key without child restarts from the root.
 * Return not defined. */
uint32_t en5_key_sequence_feed(En2Env* e, uint32_t k)
{
    uint32_t cur = en2_r32(e, CUR);
    if (cur == 0) return 0;
    if (cur == en2_r32(e, ROOT)) {
        en2_w32(e, 0x00a5d354u, en2_call0(e, EN5_timeGetTime));
        en2_w32(e, 0x00a5d364u, 0);
    }
    uint32_t off = (k & 0xff) * 4;
    uint32_t nx = en2_r32(e, en2_r32(e, CUR) + off + 4);
    if (nx != 0) {
        uint32_t c = en2_r32(e, 0x00a5d364u);
        en2_w32(e, CUR, nx);
        en2_w32(e, 0x00a5d364u, c + 1);
        if ((int32_t)en2_r32(e, nx) <= -1) return 0;
        uint32_t t = en2_call0(e, EN5_timeGetTime);
        en2_w32(e, 0x00a5d358u, t);
        int32_t el = (int32_t)(t - en2_r32(e, 0x00a5d354u));
        en2_w32(e, 0x00a5d35cu, (uint32_t)el);
        int32_t avg = el / (int32_t)en2_r32(e, 0x00a5d364u);
        en2_w32(e, 0x00a5d360u, (uint32_t)avg);
        uint32_t f = en2_r32(e, CB);
        if (f != 0 && avg < 0x1f4) en2_call1(e, f, en2_r32(e, en2_r32(e, CUR)));
    } else {
        uint32_t root = en2_r32(e, ROOT);
        uint32_t n2 = en2_r32(e, root + off + 4);
        en2_w32(e, CUR, root);
        if (n2 == 0) return 0;
        en2_w32(e, 0x00a5d354u, en2_call0(e, EN5_timeGetTime));
        en2_w32(e, CUR, n2);
        uint32_t code = en2_r32(e, n2);
        en2_w32(e, 0x00a5d364u, 1);
        if ((int32_t)code <= -1) return 0;
        uint32_t f = en2_r32(e, CB);
        if (f != 0) en2_call1(e, f, code);
    }
    en2_w32(e, CUR, en2_r32(e, ROOT));
    return 0;
}

static uint32_t inc_wrap(uint32_t v) { return (int32_t)v < 0xff ? v + 1 : 0; }

/* 0x0049e638: reads the buffered keyboard (IDirectInputDevice::GetDeviceData, method +0x28, of
 * 0x00a5d378 with 0x004e843c entries of 0x10 bytes) until it returns no entries or fails. Press:
 * state 0x80, the key into the queue, the sequence matcher 0x0049dfac(key). Release: the shift keys
 * (0x2a, 0x36) are queued too (the indexes advanced but stored only with the state change), state
 * 0x80 -> 1, else 0. DI_BUFFEROVERFLOW (1) sets 0x00a5d60c (cleared on entry); DIERR_INPUTLOST
 * reacquires (0x004937b8 on failure, acquired flag 0x00a5d5f0); other errors 0x004937b8. After:
 * every state byte with 0x80 set becomes exactly 0x80; the bytes of keys 0x48 / 0x4b / 0x4d /
 * 0x50 (the arrows) are moved to 0xc8 / 0xcb / 0xcd / 0xd0 when set; 0x00a5d5f8 = shift held
 * (0x2a or 0x36), 0x00a5d5fc = alt (0x38 or 0xb8), 0x00a5d600 = control (0x1d or 0x9d);
 * caps lock 0x3a toggles 0x00a5d604 once per press (0x00a5d608). */
void en5_keyboard_read(En2Env* e)
{
    const uint32_t S = e->scratch, CNT = S + 0xa0, DONE = S + 0xa4;
    const uint32_t ST = 0x00a5d710u, Q = 0x00a5d910u, WI = 0x00a5da1cu, RI = 0x00a5da14u, QN = 0x00a5da10u;
    uint32_t ebx = en2_r32(e, WI), edi = en2_r32(e, RI);
    if (en2_r32(e, 0x00a5d60cu) == 1) en2_w32(e, 0x00a5d60cu, 0);
    en2_w32(e, DONE, 0);
    en2_w32(e, WI, ebx);
    en2_w32(e, RI, edi);
    for (;;) {
        en2_w32(e, CNT, en2_r32(e, 0x004e843cu));
        uint32_t dev = en2_r32(e, 0x00a5d378u);
        uint32_t v[5] = {dev, 0x10, S, CNT, 0};
        uint32_t r = e->call(e->user, en2_r32(e, en2_r32(e, dev) + 0x28), v, 5);
        if (r != 0 && r != 1) {
            if (r == 0x8007001eu) {
                en2_call0(e, 0x004937b8u);
                uint32_t d = en2_r32(e, 0x00a5d378u);
                en2_w32(e, 0x00a5d5f0u, 0);
                if (d != 0) {
                    if (en2_call1(e, en2_r32(e, en2_r32(e, d) + 0x1c), d) != 0) {
                        en2_call0(e, 0x004937b8u);
                        en2_w32(e, 0x00a5d5f0u, 0);
                    } else {
                        en2_w32(e, 0x00a5d5f0u, 1);
                    }
                }
            } else {
                en2_call0(e, 0x004937b8u);
            }
            en2_w32(e, DONE, 1);
            break;
        }
        edi = en2_r32(e, RI);
        ebx = en2_r32(e, WI);
        if (r == 1) en2_w32(e, 0x00a5d60cu, 1);
        uint32_t cnt = en2_r32(e, CNT);
        en2_w32(e, WI, ebx);
        en2_w32(e, RI, edi);
        if (cnt == 0) {
            en2_w32(e, DONE, 1);
            break;
        }
        for (uint32_t i = 0, off = 0; i < en2_r32(e, CNT); i++, off += 0x10) {
            edi = en2_r32(e, RI);
            ebx = en2_r32(e, WI);
            uint32_t ofs = en2_r32(e, S + off);
            if (en2_r8(e, S + off + 4) & 0x80) {
                en2_w8(e, ST + ofs, 0x80);
                en2_w8(e, Q + ebx, (uint8_t)en2_r8(e, S + off));
                en2_w32(e, QN, en2_r32(e, QN) + 1);
                ebx = inc_wrap(ebx);
                en2_w32(e, WI, ebx);
                en2_w32(e, RI, edi);
                if (ebx == edi) {
                    edi++;
                    en2_w32(e, RI, edi);
                    if ((int32_t)edi > 0xff) { edi = 0; en2_w32(e, RI, edi); }
                }
                en2_call1(e, 0x0049dfacu, en2_r8(e, S + off));
            } else {
                if (ofs == 0x2a || ofs == 0x36) {
                    en2_w8(e, Q + ebx, (uint8_t)en2_r8(e, S + off));
                    en2_w32(e, QN, en2_r32(e, QN) + 1);
                    ebx = inc_wrap(ebx);
                    if (ebx == edi) {
                        edi++;
                        if ((int32_t)edi > 0xff) edi = 0;
                    }
                }
                uint8_t sv = en2_r8(e, ST + en2_r32(e, S + off));
                en2_w32(e, WI, ebx);
                en2_w32(e, RI, edi);
                en2_w8(e, ST + en2_r32(e, S + off), sv == 0x80 ? 1 : 0);
            }
        }
        if (en2_r32(e, DONE) != 0) break;   /* (never set here: read again) */
    }
    edi = en2_r32(e, RI);
    ebx = en2_r32(e, WI);
    for (uint32_t k = 0; k < 0x100; k++)
        if (en2_r8(e, ST + k) & 0x80) en2_w8(e, ST + k, 0x80);
    static const uint8_t mv[4] = {0x48, 0x4b, 0x4d, 0x50};
    for (int k = 0; k < 4; k++) {
        uint8_t b = en2_r8(e, ST + mv[k]);
        if (b != 0) {
            en2_w8(e, ST + mv[k] + 0x80, b);
            en2_w8(e, ST + mv[k], 0);
        }
    }
    en2_w32(e, 0x00a5d5f8u, (en2_r8(e, ST + 0x2a) != 0 || en2_r8(e, ST + 0x36) != 0) ? 1 : 0);
    en2_w32(e, 0x00a5d5fcu, (en2_r8(e, ST + 0x38) != 0 || en2_r8(e, ST + 0xb8) != 0) ? 1 : 0);
    en2_w32(e, 0x00a5d600u, (en2_r8(e, ST + 0x1d) != 0 || en2_r8(e, ST + 0x9d) != 0) ? 1 : 0);
    if (en2_r8(e, ST + 0x3a) != 0 && en2_r32(e, 0x00a5d608u) != 1) {
        en2_w32(e, 0x00a5d608u, 1);
        en2_w32(e, 0x00a5d604u, ~en2_r32(e, 0x00a5d604u));
    } else if (en2_r8(e, ST + 0x3a) == 0) {
        en2_w32(e, 0x00a5d608u, 0);
    }
    en2_w32(e, RI, edi);
    en2_w32(e, WI, ebx);
}
