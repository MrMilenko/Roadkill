/*
 * sh4_net_car.c: a network player's car state and messages, TM2.EXE 0x00473714 (see
 * sh4_net_car.h). Frame (fb): +0 armor maximum, +4 own player id, +8 / +0x0c / +0x10 the stream
 * base, +0x14 the source, +0x18 the player car id word, +0x1c u8 continue flag.
 * Machine details kept: the speeds are multiplied by the ticks in 16 bits; command 13 does not
 * advance past its argument byte (the next command reads it again as an opcode); a command
 * number above 13 only skips its own byte.
 */
#include "sh4_net_car.h"

#define R8(a) sh_r8(e, a)
#define R16(a) sh_r16(e, a)
#define R32(a) sh_r32(e, a)
#define W8(a, v) sh_w8(e, a, v)
#define W16(a, v) sh_w16(e, a, v)
#define W32(a, v) sh_w32(e, a, v)
#define FB(o) (fb + (uint32_t)(o))
#define C0(f) sh3_call(e, f, 0, 0, 0, 0, 0, 0)
#define C1(f, a) sh3_call(e, f, 1, a, 0, 0, 0, 0)
#define C2(f, a, b) sh3_call(e, f, 2, a, b, 0, 0, 0)
#define C3(f, a, b, c) sh3_call(e, f, 3, a, b, c, 0, 0)

#define NET 0x0050f9dcu        /* the network session (+0x30 own player id) */
#define SEND 0x00624d84u       /* write position in player 0's stream (0x00622d3a) */

static int32_t imax(int32_t a, int32_t b) { return a < b ? b : a; }

void sh4_net_car_apply(const ShEnv* e, uint32_t fb, uint32_t src, int32_t len, uint32_t i) {
    W32(FB(0x14), src);
    uint32_t p = C1(0x0045afbcu, i);
    uint32_t off = 0x40c * i, rec = 0x00622d24u + off;
    C3(0x0048ce88u, rec, src, (uint32_t)((len + 3) / 4));
    W32(p + 0x1d8, (uint32_t)(int32_t)(int16_t)R16(rec + 2));
    W32(0x00624d94u + 4 * i, C0(0x004931f4u));
    W32(p + 0x6f8, (uint32_t)(int32_t)(int16_t)R16(rec));
    W32(p + 0x704, C2(0x0048b71cu, R32(rec + 8) - R32(p + 0x6e4), R32(rec + 0xc) - R32(p + 0x6e8)));
    W32(p + 0x6e4, R32(rec + 8));
    W32(p + 0x6e8, R32(rec + 0xc));
    int32_t m = imax((int32_t)R32(p + 0x5e8), (int32_t)R32(p + 0x630));
    W32(FB(0), (uint32_t)m);
    m = imax(m, (int32_t)R32(p + 0x678));
    W32(FB(0), (uint32_t)m);
    m = imax(m, (int32_t)R32(p + 0x6c0));
    int32_t armor = (int32_t)R32(rec + 0x10);
    W32(FB(0), (uint32_t)m);
    if (m < armor) {
        W32(p + 0x5e8, (uint32_t)armor);
        W32(p + 0x630, (uint32_t)armor);
        W32(p + 0x678, (uint32_t)armor);
        W32(p + 0x6c0, (uint32_t)armor);
    }
    static const uint16_t flag_at[12] = {0xcc, 0xcd, 0xce, 0xcf, 0xd2, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf};
    for (int k = 0; k < 12; k++) W8(p + flag_at[k], (R16(rec + 0x14) >> k) & 1 ? 0xff : 0);
    uint16_t frozen = (R16(rec + 0x14) >> 12) & 1 ? 0xffff : 0;
    if (frozen != R16(p + 0x154)) {
        if (frozen) {
            W8(p + 0xa7, 1);
            W16(p + 0x152, 0x12c);
            C2(0x00470940u, (uint32_t)(int32_t)(int16_t)R16(p + 0x90), 1);
            W16(p + 0x87c, 0);
        } else {
            W8(p + 0xa7, 0);
            C2(0x00470940u, (uint32_t)(int32_t)(int16_t)R16(p + 0x90), 3);
        }
        W16(p + 0x154, (R16(rec + 0x14) >> 12) & 1 ? 0xffff : 0);
    }
    W32(p + 0x6ec, R32(rec + 0x10));
    W16(p + 0x6ce, (uint16_t)(((int32_t)(int16_t)R16(rec + 4) * (int32_t)R32(p + 0x1ec)) / 100));
    W16(p + 0x6ce, (uint16_t)(R16(p + 0x6ce) * C0(0x004931d4u)));
    W16(p + 0x6c4, (uint16_t)(((int32_t)(int16_t)R16(rec + 6) * (int32_t)(int16_t)R16(p + 0x19c)) / 100));
    W16(p + 0x6c4, (uint16_t)(R16(p + 0x6c4) * C0(0x004931d4u)));

    uint32_t b = rec + 0x16;     /* the stream */
    W8(FB(0x1c), 1);
    W32(FB(0x10), b);
    W32(FB(8), b);
    W32(FB(0xc), b);
    int32_t x = 0;
    while (x < 0x3de) {
        uint8_t op = R8(b + (uint32_t)x++);
        if (op == 0x7f || op != 0x2a) {
            W8(FB(0x1c), 0);
            return;
        }
        uint8_t cmd = R8(b + (uint32_t)x++);
        switch (cmd) {
        case 0:     /* car reset (not when networked) */
            x += 2;
            if (!C0(0x004632d4u)) C1(0x0045ba64u, i);
            break;
        case 1: {   /* the car id of a player */
            W32(FB(0x18), R16(b + (uint32_t)x));
            x += 2;
            if (!C0(0x004632d4u)) {
                uint32_t id = R32(b + (uint32_t)x);
                uint32_t car = C1(0x0045b988u, C2(0x00498660u, R32(NET), id));
                W32(car + 0x208, R16(FB(0x18)));
                if (id == R32(R32(NET) + 0x30)) W8(0x00624d88u, 1);
            }
            x += 4;
            break;
        }
        case 2:
            x += 2;
            W8(0x00624d8cu + i, 1);
            break;
        case 3: {   /* a kill credited to us: answer with our score */
            x += 2;
            uint32_t who = R32(R32(FB(0x10)) + (uint32_t)x);
            x += 4;
            uint32_t me = R32(R32(NET) + 0x30);
            if (who != me) break;
            C1(0x0045b9d4u, 0);
            uint32_t sc = C1(0x0045b9ccu, 0);
            W32(FB(4), R32(R32(NET) + 0x30));
            int32_t c = (int32_t)R32(SEND);
            if (c < 0x3d6) {
                c++;
                W32(SEND, (uint32_t)c);
                W8(0x00622d39u + (uint32_t)c, 0x2a);
                c++;
                W32(SEND, (uint32_t)c);
                W8(0x00622d39u + (uint32_t)c, 4);
                W16(0x00622d3au + R32(SEND), sc);
                uint32_t d = R32(SEND) + 2;
                W32(SEND, d);
                W32(0x00622d3au + d, R32(FB(4)));
                W32(SEND, d + 4);
            } else {
                C0(0x004632b0u);
            }
            break;
        }
        case 4: {
            uint32_t v = R16(R32(FB(8)) + (uint32_t)x);
            x += 6;
            C2(0x0045b9dcu, i, v);
            break;
        }
        case 5: {
            uint32_t v = R16(R32(FB(0xc)) + (uint32_t)x);
            x += 6;
            C2(0x0045ba20u, i, v);
            break;
        }
        case 7:
            C2(0x0044df78u, i, (uint32_t)(int32_t)(int8_t)R8(b + (uint32_t)x));
            x += 2;
            break;
        case 8:
            C2(0x0044df18u, i, (uint32_t)(int32_t)(int8_t)R8(b + (uint32_t)x));
            x += 2;
            break;
        case 10:
            C2(0x00449ef4u, R8(b + (uint32_t)x) + 0x190u, R8(b + (uint32_t)x + 1));
            x += 2;
            break;
        case 11:
            C2(0x00449ebcu, R8(b + (uint32_t)x), R8(b + (uint32_t)x + 1));
            x += 2;
            break;
        case 12:
            C1(0x0044dfe8u, i);
            x += 2;
            break;
        case 13:    /* no advance */
            C2(0x0044dff8u, i, (uint32_t)(int32_t)(int8_t)R8(b + (uint32_t)x));
            break;
        default:    /* 6, 9, > 13 */
            break;
        }
    }
}
