/*
 * gp6_car_object.c: the car / object branch of 0x00419604 (see gp6_car_object.h).
 *
 * The labels name the original's addresses; the control flow follows the machine code, whose
 * register values carried between blocks are the C locals named after their registers where a
 * later block reads them. sar16(a) is the original's "mov r, dword [a]; sar r, 16" (the i16 at
 * a + 2).
 */
#include "gp6_car_object.h"

#include "../math/tm2math.h"

#define M(a) e->mem(e->user, (a))
static uint32_t r32(En2Env* e, uint32_t a) { uint32_t v; memcpy(&v, M(a), 4); return v; }
static uint16_t r16(En2Env* e, uint32_t a) { uint16_t v; memcpy(&v, M(a), 2); return v; }
static uint8_t r8(En2Env* e, uint32_t a) { return *M(a); }
static void w32(En2Env* e, uint32_t a, uint32_t v) { memcpy(M(a), &v, 4); }
static void w16(En2Env* e, uint32_t a, uint32_t v) { uint16_t h = (uint16_t)v; memcpy(M(a), &h, 2); }
static void w8(En2Env* e, uint32_t a, uint32_t v) { *M(a) = (uint8_t)v; }
static int32_t s16(En2Env* e, uint32_t a) { return (int16_t)r16(e, a); }
static int32_t sar16(En2Env* e, uint32_t a) { return (int32_t)r32(e, a) >> 16; }

#define R32(a) r32(e, (a))
#define R16(a) r16(e, (a))
#define R8(a) r8(e, (a))
#define W32(a, v) w32(e, (a), (uint32_t)(v))
#define W16(a, v) w16(e, (a), (uint32_t)(v))
#define W8(a, v) w8(e, (a), (uint32_t)(v))
#define S16(a) s16(e, (a))
#define SAR16(a) sar16(e, (a))
#define F(o) (fb + (uint32_t)(o))
#define FR32(o) R32(F(o))
#define FI32(o) ((int32_t)R32(F(o)))
#define FR8(o) R8(F(o))

static uint32_t call(En2Env* e, uint32_t fn, int n, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
    uint32_t v[5] = {a0, a1, a2, a3, a4};
    return e->call(e->user, fn, v, n);
}
#define C0(fn) call(e, (fn), 0, 0, 0, 0, 0, 0)
#define C1(fn, a) call(e, (fn), 1, (a), 0, 0, 0, 0)
#define C2(fn, a, b) call(e, (fn), 2, (a), (b), 0, 0, 0)
#define C3(fn, a, b, c) call(e, (fn), 3, (a), (b), (c), 0, 0)
#define C4(fn, a, b, c, d) call(e, (fn), 4, (a), (b), (c), (d), 0)
#define C5(fn, a, b, c, d, f) call(e, (fn), 5, (a), (b), (c), (d), (f))

/* idiv (a zero divisor faults in the original: 0 here) */
static int32_t sdiv(int32_t a, int32_t b) { return b == 0 ? 0 : (a == INT32_MIN && b == -1) ? a : a / b; }
static int32_t srem(int32_t a, int32_t b) { return b == 0 || b == -1 ? 0 : a % b; }
static int32_t iabs(int32_t v) { return v < 0 ? (int32_t)(0u - (uint32_t)v) : v; }
static int32_t mul(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
static int32_t add(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
static int32_t sub(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }

/* the integer square root inlined three times (bit by bit, signed compares) */
static uint32_t isqrt(int32_t c)
{
    uint32_t r = 0, bit = 0x40000000u;
    do {
        if (!(c < (int32_t)bit) && !(sub(c, (int32_t)bit) < (int32_t)r)) {
            c = sub(sub(c, (int32_t)bit), (int32_t)r);
            r = (r >> 1) | bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    } while (bit);
    return r;
}

/* 0x0048c9f4 / 0x0048ca2c on frame or block memory */
static void wrap32(En2Env* e, uint32_t a) { int32_t v = (int32_t)R32(a); tm2_angle_wrap32(&v); W32(a, v); }
static void wrap16(En2Env* e, uint32_t a) { int16_t v = (int16_t)R16(a); tm2_angle_wrap16(&v); W16(a, v); }

/* 0x0048c360(angles, out) then 0x0048cc14(out, vec, res) on the frame */
static void build_matrix(En2Env* e, uint32_t ang, uint32_t out)
{
    int16_t a[3] = {(int16_t)R16(ang), (int16_t)R16(ang + 2), (int16_t)R16(ang + 4)};
    int32_t m[9];
    tm2_build_matrix(a, m);
    for (int i = 0; i < 9; i++) W32(out + 4u * (uint32_t)i, m[i]);
}
static void mat_vec_row(En2Env* e, uint32_t ma, uint32_t va, uint32_t oa)
{
    int32_t m[9], v[3], o[3];
    for (int i = 0; i < 9; i++) m[i] = (int32_t)R32(ma + 4u * (uint32_t)i);
    for (int i = 0; i < 3; i++) v[i] = (int32_t)R32(va + 4u * (uint32_t)i);
    tm2_mat_vec_mul_row(m, v, o);
    for (int i = 0; i < 3; i++) W32(oa + 4u * (uint32_t)i, o[i]);
}

/* sar 31 / shl k / sbb / sar k: signed division by 2^k toward zero */
static int32_t div_pow2(int32_t v, int k) { return v < 0 ? -(int32_t)((0u - (uint32_t)v) >> k) : v >> k; }

/* the per car descriptor of the two car blocks (one of the six contacts at hit base +0x274 that
 * is free, else hit base +0x328) */
static uint32_t free_contact(En2Env* e, uint32_t base)
{
    for (int i = 0; i < 6; i++)
        if (R8(base + 0x274u + 0x24u * (uint32_t)i) == 0) return base + 0x274u + 0x24u * (uint32_t)i;
    return base + 0x328u;
}

/* strengths of the crash sounds 5 and 3 from the combined impact speed */
static void crash_sounds(En2Env* e, int32_t v)
{
    int32_t s = (v < 1000 || v >= 2000) ? 0x63 : sdiv(mul(v, 0x30), 2000) + 0x33;
    uint32_t vol = C0(0x0046f990u);
    uint32_t pan = C0(0x0046f988u);
    C4(0x0046f8b0u, 5, pan, vol, (uint32_t)s);
    s = v < 1000 ? sdiv(mul(v, 0x31), 1000) + 0x32 : 0x63;
    vol = C0(0x0046f990u);
    pan = C0(0x0046f988u);
    C4(0x0046f8b0u, 3, pan, vol, (uint32_t)s);
}

/* the rumble position: 0x0046f974(pos) without split screen (0x0046cc54() == 0), else
 * 0x0046f980(pos, -1) */
static void rumble_at(En2Env* e, uint32_t pos)
{
    if (C0(0x0046cc54u) == 0) C1(0x0046f974u, pos);
    else C2(0x0046f980u, pos, 0xffffffffu);
}

/* the turn of one contact after the push (0x0041adb5.. / 0x0041aef8..): c = the contact, body =
 * its car's body, d = that car's angle difference (frame +0xB8 / +0xBC), share = its mass share
 * (ebp / frame +0xD4), ob = the other body, ost = the other stats */
static void contact_turn(En2Env* e, uint32_t c, uint32_t body, int32_t d, int32_t share, uint32_t ob, uint32_t ost)
{
    W16(c + 0x10, R16(c + 8) - R16(body + 0x34));
    wrap16(e, c + 0x10);
    int32_t t = SAR16(c + 0xe);
    if (mul(d, t) > 0) {
        if (iabs(t) < 0x200) {
            if (t < 0) W8(c + 0x11, R8(c + 0x11) + 4);
            else W16(c + 0x10, R16(c + 0x10) - 0x400);
        } else {
            W8(c + 0x11, R8(c + 0x11) + 8);
        }
        wrap16(e, c + 0x10);
    }
    int32_t k = sdiv(mul(mul(iabs(SAR16(ob + 8)), 2), share), (int32_t)R32(ost + 0xb4));
    if (k > 100) {
        W16(c + 0x12, (uint32_t)R16(c + 0x12) * 2u);
        k = 100;
    }
    W16(c + 0x10, sdiv(mul(SAR16(c + 0xe), k), 100));
}

void gp6_car_object_hit(En2Env* e, uint32_t fb, uint32_t ebp)
{
    uint32_t eax, ebx, ecx, edx, esi = 0, edi = 0;
    uint32_t sum = FR32(0x9c);
    W8(sum + 1, 0);
    W8(sum, 0);
    /* 0x0045b0ec(entity, &kind, &index): al = it is a car */
    if ((uint8_t)C3(0x0045b0ecu, R32(FR32(0x98) + 0x14), F(0x1bc), F(0x74)) == 0) goto L_41b448;
    if (FR8(0x1bc) != 0) {
        ecx = C1(0x0045afbcu, FR32(0x74));          /* the player car */
        if (R8(FR32(0x8c) + 0x1a) != 0 && (int32_t)R32(FR32(0x90) + 0xa0) > 0 && R8(ecx + 0xb4) == 0 && R8(ecx + 0xa8) == 0) {
            W8(ecx + 0xb4, 1);
            W16(ecx + 0x15a, 0);
            W32(ecx + 0x88c, C2(0x00450de4u, ecx + 0x6e4, 1));
            W8(FR32(0x8c) + 0x1a, 0);
            edx = R32(FR32(0x94) + 0x44);
            if (edx) W16(edx + 0xc, 0);
        }
    } else {
        ecx = C1(0x0045b080u, FR32(0x74));          /* the AI car */
        if (R8(FR32(0x8c) + 0x1a) != 0 && (int32_t)R32(FR32(0x90) + 0xa0) > 0 && R8(ecx + 0x82) == 0 && R16(ecx + 0x30) != 0xf0) {
            W8(ecx + 0x82, 1);
            W16(ecx + 0x50e, 0);
            W32(ecx + 0x878, C2(0x00450de4u, ecx + 0x728, 1));
            W8(FR32(0x8c) + 0x1a, 0);
            edi = R32(FR32(0x94) + 0x44);
            if (edi) W16(edi + 0xc, 0);
        }
    }
    ebx = ecx;
    /* 0x00419e9b: this car (A) and the other car (B) */
    W32(F(0xa0), FR32(0x70));
    W8(F(0x1ac), FR8(0x1b4));
    W32(F(0xa4), FR32(0x6c));
    W16(F(0x174), R16(sum + 0xe));
    W8(F(0x19c), 0);
    {
        uint32_t car = FR32(0x70);
        if (FR8(0x1b4)) {
            W32(F(0x108), car + 0x9a);
            W32(F(0xf0), car + 0x138);
            W32(F(0x100), car + 0x21c);
            W32(F(0xf8), car + 0x6c4);
            W32(F(0xa8), S16(car));
            W32(F(0x17c), (car & 0xffff0000u) | R16(car + 0x90));
            W32(F(0xb0), SAR16(car));
            W32(F(0x110), car + 0x58c);
            W32(F(0xe8), 0);
            W32(F(0x118), car + 0x848);
        } else {
            W32(F(0x108), car + 0x68);
            W32(F(0xf0), car + 0x4ec);
            W32(F(0x100), car + 0xcc);
            W32(F(0xf8), car + 0x708);
            W32(F(0xa8), SAR16(car + 2));
            W32(F(0x17c), (car & 0xffff0000u) | R16(car + 0x30));
            W32(F(0xb0), SAR16(car + 4));
            W32(F(0x110), car + 0x5d0);
            W32(F(0x118), 0);
            W32(F(0xe8), car + 0x32);
        }
    }
    if (FR8(0x1bc)) {
        W32(F(0x10c), ecx + 0x9a);
        W32(F(0xf4), ecx + 0x138);
        W32(F(0x104), ecx + 0x21c);
        W32(F(0xfc), ecx + 0x6c4);
        W32(F(0xac), S16(ecx));
        W32(F(0xb4), SAR16(ecx));
        W32(F(0xec), 0);
        W32(F(0x114), ecx + 0x58c);
    } else {
        W32(F(0x10c), ecx + 0x68);
        W32(F(0xf4), ecx + 0x4ec);
        W32(F(0x104), ecx + 0xcc);
        W32(F(0xfc), ecx + 0x708);
        W32(F(0xac), SAR16(ecx + 2));
        W32(F(0xb4), SAR16(ecx + 4));
        W32(F(0x114), ecx + 0x5d0);
        W32(F(0xec), ecx + 0x32);
    }
    esi = free_contact(e, FR32(0x100));
    edi = free_contact(e, FR32(0x104));
    /* 0x0041a15c */
    if (R8(FR32(0xf0) + 2) == 0) {
        if (R8(FR32(0x10c) + 0x15) != 0) W8(F(0x19c), 1);
        goto L_41a6f9;
    }
    if (R8(FR32(0x108) + 0x14) != 0) {
        W8(F(0x19c), 1);
        goto L_41a6f9;
    }
    {
        int32_t idx = FI32(0xa4);
        int front = idx == 0 || idx == 1 || idx == 4, back = idx == 2 || idx == 3;
        int32_t v = SAR16(FR32(0xf8) + 0x18);
        if (!(front && v > 0) && !(back && v < 0)) goto L_41a6f9;
    }
    /* 0x0041a1ed: a special car hit (the 0x20 car's boost, the damage) */
    {
        uint32_t carA = FR32(0xa0), bodyA, turboA;
        W32(F(0x154), carA);
        W8(F(0x1a0), FR8(0x1ac));
        W8(F(0x188), 1);
        W8(F(0x1a8), FR8(0x1bc));
        if (FR8(0x1ac)) {
            W32(F(0x12c), carA + 0x9a);
            bodyA = carA + 0x6c4;
            W16(F(0x180), R16(carA + 0x90));
            W32(F(0x128), carA + 0x138);
            W32(F(0x134), carA + 0x58c);
            W32(F(0x120), SAR16(carA));
            turboA = carA + 0x848;
        } else {
            W32(F(0x12c), carA + 0x68);
            bodyA = carA + 0x708;
            W16(F(0x180), R16(carA + 0x30));
            W32(F(0x128), carA + 0x4ec);
            W32(F(0x134), carA + 0x5d0);
            W32(F(0x120), SAR16(carA + 4));
            turboA = carA + 0x834;
        }
        uint32_t stB;
        if (FR8(0x1a8)) {
            W32(F(0x130), ecx + 0x9a);
            stB = ecx + 0x138;
            W32(F(0x138), ecx + 0x58c);
            int32_t id = S16(ecx);
            W32(F(0x11c), id);
            W32(F(0x178), ((uint32_t)id & 0xffff0000u) | R16(ecx + 0x90));
            W32(F(0x124), SAR16(ecx));
        } else {
            W32(F(0x130), ecx + 0x68);
            stB = ecx + 0x4ec;
            W32(F(0x138), ecx + 0x5d0);
            int32_t id = SAR16(ecx + 2);
            W32(F(0x11c), id);
            W32(F(0x178), ((uint32_t)id & 0xffff0000u) | R16(ecx + 0x30));
            W32(F(0x124), SAR16(ecx + 4));
        }
        if (R8(FR32(0x12c) + 0x14)) {
            W8(F(0x19c), 1);
            goto L_41a6f9;
        }
        if (FR8(0x1a0) && R16(F(0x180)) == 0x20) {
            if (SAR16(turboA + 0xc) < 1) goto L_41a6f9;
            W16(turboA + 0xe, R16(turboA + 0xe) - 1);
        }
        if (S16(F(0x180)) == 0x20) {
            if (FR8(0x1a0) || FR8(0x1a8)) {
                C4(0x0046f8b0u, 0x25, 0, 0, 0x63);
            } else {
                rumble_at(e, bodyA + 0x20);
                uint32_t vol = C0(0x0046f990u), pan = C0(0x0046f988u);
                C4(0x0046f8b0u, 0x25, pan, vol, 0x63);
            }
        }
        if (R16(F(0x180)) != 0xf0) {
            W8(FR32(0x130) + 0xd, 1);
            W16(stB + 0x1a, 0x1e);
            C4(0x00421920u, FR32(0x154), FR8(0x1a0), 8, 0);
            W8(F(0x188), 0);
            W8(FR32(0x130) + 0x15, 1);
        }
        uint32_t stA = FR32(0x128);
        W8(FR32(0x12c) + 0x14, 1);
        W16(stA + 0x96, R16(F(0x178)));
        int16_t h = (int16_t)R16(stB + 0x50), h2 = (int16_t)R16(stB + 0x4e);
        W16(stA + 0x30, h < h2 ? h2 : h);
        W16(stA + 0x30, add(SAR16(stA + 0x2e), sdiv(SAR16(stA + 0x48), 2)));
        W16(stA + 0x32, R16(stB + 0x42) + R16(stB + 0x46) + 0x18);
        if (SAR16(stA + 0x30) > 0x68) W16(stA + 0x32, 0x68);
        /* the eight wheel jolts: rand() % stats +0x42 times the frame ticks */
        static const uint16_t offs[4] = {0x36, 0x7e, 0xc6, 0x10e};
        for (int k = 0; k < 8; k++) {
            int32_t d = SAR16(stB + 0x40);
            int32_t r = srem((int32_t)C0(0x004899c2u), d);
            int32_t t = (int16_t)C0(0x004931d4u);
            W16(FR32(k < 4 ? 0x134 : 0x138) + offs[k & 3], mul(r, t));
        }
        ebx = C2(0x0045c1c4u, FR32(0x120), FR32(0x124));
        /* 0x004210e4(entity type of B's slot, level kinds, the pair vector, 0, then on the stack
         * the i16 at +0x120) */
        uint32_t w = (uint32_t)S16(F(0x120));
        edx = C1(0x00449eb4u, w);
        uint32_t slot = C1(FR8(0x1a8) ? 0x0045ae24u : 0x0045ae2cu, FR32(0x11c));
        C5(0x004210e4u, R32(slot + 0x158), edx, ebx, 0, w);
        if (FR8(0x188)) W8(F(0x19c), 1);
    }
L_41a6f9:
    if ((int32_t)C0(0x0045b3dcu) < 1 && R16(F(0x17c)) == 0xc0) {
        int32_t idx = FI32(0xa4);
        if ((idx == 0 || idx == 1 || idx == 4) && SAR16(FR32(0xf8) + 0x18) > 0) {
            eax = FR32(0x118);
            if (eax && R32(eax + 0x48) && R8(R32(eax + 0x48)) == 0) {
                C1(0x0044c81cu, eax);
                W8(R32(FR32(0x118) + 0x48) + 0xb, 1);
                W8(R32(FR32(0x118) + 0x48), 1);
                if (FR8(0x1ac) == 0) W8(FR32(0xa0) + 0x44c, 1);
            }
        }
    }
    if (FR8(0x19c)) goto L_41b435;
    {
        uint32_t bA = FR32(0xf8), bB = FR32(0xfc), stA = FR32(0xf0), stB = FR32(0xf4);
        W16(esi + 0xc, 0);
        W16(edi + 0xc, 0);
        W32(F(0xc4), iabs(SAR16(bA + 0x18)) > (int32_t)R32(stA + 0xb0) ? R32(bA + 0xf4) : R32(bA + 0x34));
        W32(F(0xc8), iabs(SAR16(bB + 0x18)) > (int32_t)R32(stB + 0xb0) ? R32(bB + 0xf4) : R32(bB + 0x34));
        W32(F(0xc0), sub(FI32(0xc4), FI32(0xc8)));
        wrap32(e, F(0xc0));
        int32_t v = add(add(iabs(add(SAR16(bA + 0x16), SAR16(bB + 0x16))), iabs(add(SAR16(bA + 0x18), SAR16(bB + 0x18)))),
                        iabs(add(SAR16(bA + 0x1a), SAR16(bB + 0x1a))));
        rumble_at(e, bA + 0x20);
        crash_sounds(e, v);
        ebx = C2(0x0045c1c4u, FR32(0xb0), FR32(0xb4));
        ecx = ebx;
        uint32_t pv = C2(0x0045c1c4u, FR32(0xb4), FR32(0xb0));
        if (R32(ebx + 4) == 0) W32(ebx + 4, 1);
        W32(F(0xcc), tm2_atan2((int32_t)R32(ecx), (int32_t)R32(ecx + 4)));
        W32(F(0xd0), tm2_atan2((int32_t)R32(pv), (int32_t)R32(pv + 4)));
        W16(esi + 0xa, FR32(0xcc));
        W16(edi + 0xa, FR32(0xd0));
        W16(edi + 0x14, FR32(0xb0));
        W16(esi + 0x14, FR32(0xb4));
        if (SAR16(F(0x172)) > 8) {
            int32_t a = iabs(SAR16(bA + 0x18)), b = iabs(SAR16(bB + 0x18));
            int32_t m = div_pow2(a < b ? b : a, 5);
            if (SAR16(F(0x172)) > m) W16(F(0x174), m);
        }
        W32(F(0xb8), sub(FI32(0xc4), FI32(0xcc)));
        W32(F(0xbc), sub(FI32(0xc8), FI32(0xd0)));
        W8(F(0x198), iabs(FI32(0xb8)) < iabs(FI32(0xbc)));
        /* mass shares (frame ebp / +0xD4, the larger one 100) */
        int32_t ma = SAR16(stA + 0x78), mb = SAR16(stB + 0x78);
        int32_t sa = sdiv(mul(ma, 100), add(ma, mb)), sb = 100 - sa;
        W32(F(0xd4), sb);
        if (sa > sb) {
            W32(F(0xd4), sdiv(mul(sb, 100), sa));
            sa = 100;
        } else {
            sa = sdiv(mul(sa, 100), sb);
            W32(F(0xd4), 100);
        }
        /* the push direction from both cars' probe positions */
        uint32_t pA = FR32(0x110), pB = FR32(0x114);
        int32_t shb = FI32(0xd4);
        W32(F(0xd8), mul((int32_t)R32(pB + 0xc), shb));
        W32(F(0xdc), mul((int32_t)R32(pB + 0x10), shb));
        int32_t x = sdiv(add(mul((int32_t)R32(pA + 0xc), sa), FI32(0xd8)), 200);
        int32_t y = sdiv(add(mul((int32_t)R32(pA + 0x10), sa), FI32(0xdc)), 200);
        int32_t ang = tm2_atan2(x, y);
        W16(esi + 8, ang);
        W16(edi + 8, ang);
        uint32_t len = isqrt(add(mul(x, x), mul(y, y))) << 5;
        W32(F(0xe0), len);
        int32_t lim = sdiv(mul((int16_t)C0(0x004931d4u), 0xbe), 100) << 5;
        if (lim > (int32_t)len) W32(F(0xe0), sdiv(mul((int16_t)C0(0x004931d4u), 0xbe), 100) << 5);
        int32_t L = FI32(0xe0);
        W16(esi + 6, L);
        W16(edi + 6, L);
        if (sa > add(FI32(0xd4), 10)) {
            W16(edi + 6, sdiv(mul(L, sa), FI32(0xd4)));
            if (SAR16(edi + 4) > mul(L, 2)) W16(edi + 6, mul(L, 2));
        } else if (add(sa, 10) < FI32(0xd4)) {
            W16(esi + 6, sdiv(mul(L, FI32(0xd4)), sa));
            if (SAR16(esi + 4) > mul(L, 2)) W16(esi + 6, mul(L, 2));
        }
        {
            int32_t mx = (int32_t)R32(stB + 0xb4);
            if (!(mx > (int32_t)R32(stA + 0xb4))) mx = (int32_t)R32(stA + 0xb4);
            int32_t s = add(sdiv(mul(L, 0x78), mx), 0x28);
            W16(esi + 0x12, s);
            W16(edi + 0x12, s);
        }
        {
            int32_t t = SAR16(esi + 4), cap = (int32_t)R32(stA + 0xb4);
            if (t > cap) W16(esi + 6, R16(stA + 0xb4));
            else if (t < -cap) W16(esi + 6, 0u - R16(stA + 0xb4));
            t = SAR16(edi + 4);
            cap = (int32_t)R32(stB + 0xb4);
            if (t > cap) W16(edi + 6, R16(stB + 0xb4));
            else if (t < -cap) W16(edi + 6, 0u - R16(stB + 0xb4));
        }
        W16(esi + 0x10, 0);
        W16(edi + 0x10, 0);
        if (FR8(0x198)) {
            contact_turn(e, edi, bB, FI32(0xb8), sa, bA, stA);
            int32_t d = iabs(FI32(0xb8));
            if (d < 0xe3) {
                W16(edi + 0x10, sdiv(mul(SAR16(edi + 0xe), d), 0xe3));
                int32_t c = SAR16(edi + 4);
                W16(edi + 6, add(sdiv(c, 0x14), c));
            } else {
                int32_t b = SAR16(edi + 4);
                W16(edi + 6, sub(b, sdiv(b, 0x14)));
                b = SAR16(esi + 4);
                W16(esi + 6, add(sdiv(b, 0x14), b));
            }
        } else {
            contact_turn(e, esi, bA, FI32(0xbc), FI32(0xd4), bB, stB);
            int32_t d = iabs(FI32(0xbc));
            if (d < 0xe3) {
                W16(esi + 0x10, sdiv(mul(SAR16(esi + 0xe), d), 0xe3));
                int32_t c = SAR16(esi + 4);
                W16(esi + 6, add(sdiv(c, 0x14), c));
            } else {
                int32_t b = SAR16(esi + 4);
                W16(esi + 6, sub(b, sdiv(b, 0x14)));
                b = SAR16(edi + 4);
                W16(edi + 6, add(sdiv(b, 0x14), b));
            }
        }
        /* 0x0041b03d: still touching from the last frame: keep them apart */
        uint32_t hA = FR32(0x100), hB = FR32(0x104);
        if (SAR16(hA + 0xa) == FI32(0xac) && SAR16(hB + 0xa) == FI32(0xa8) && SAR16(hA + 8) < 10) {
            W8(F(0x19c), 1);
            if (SAR16(hA + 0x10) < 0x50) {
                W16(hA + 0x12, 0x50);
                W16(hB + 0x12, 0x50);
            }
        }
        if (FR8(0x19c)) goto L_41b31b;
        /* the push size from where each car's front probe lands */
        for (int k = 0; k < 2; k++) {
            uint32_t c = k ? edi : esi, pc = k ? pB : pA, cap = k ? stA : stB;
            W16(F(0x48), 0);
            W16(F(0x4a), 0);
            W16(F(0x4c), R16(c + 8));
            build_matrix(e, F(0x48), F(0));
            W32(F(0x30), 0);
            W32(F(0x34), div_pow2(SAR16(c + 4), 5));
            W32(F(0x38), 0);
            mat_vec_row(e, F(0), F(0x30), F(0x3c));
            int32_t dx = sub((int32_t)R32(pc + 0xc), FI32(0x3c)), dy = sub((int32_t)R32(pc + 0x10), FI32(0x40));
            uint32_t r = isqrt(add(mul(dx, dx), mul(dy, dy))) << 5;
            W32(F(0xe0), r);
            W16(c + 4, sdiv(mul((int32_t)r, 0xc), (int32_t)R32(cap + 0xb4)));
        }
        if (FR8(0x198)) {
            W16(esi + 4, (int16_t)R16(esi + 4) / 2);
            W16(edi + 4, (uint32_t)R16(edi + 4) * 2u);
        } else {
            W16(edi + 4, (int16_t)R16(edi + 4) / 2);
            W16(esi + 4, (uint32_t)R16(esi + 4) * 2u);
        }
        int32_t a = iabs(FI32(0xb8));
        if (a < 0x400) W16(esi + 4, div_pow2(mul(SAR16(esi + 2), a), 10));
        a = iabs(FI32(0xbc));
        if (a < 0x400) W16(edi + 4, div_pow2(mul(SAR16(edi + 2), a), 10));
        W16(hA + 0xc, FR32(0xac));
        W16(hB + 0xc, FR32(0xa8));
    }
L_41b31b:
    {
        uint32_t bA = FR32(0xf8), bB = FR32(0xfc);
        uint32_t ta = FR32(0xe8);
        if (ta) {
            W32(F(0xe4), sub((int32_t)R32(bA + 0x40), SAR16(esi + 6)));
            wrap32(e, F(0xe4));
            if (iabs(FI32(0xe4)) > 0x200) W8(ta + 0xb, 1);
        }
        if (FR32(0xec)) {
            W32(F(0xe4), sub((int32_t)R32(bB + 0x40), SAR16(edi + 6)));
            wrap32(e, F(0xe4));
            if (iabs(FI32(0xe4)) > 0x200) W8(FR32(0xec) + 0xb, 1);
        }
        if (FR8(0x19c)) {
            W8(esi, 0);
            W8(edi, 0);
            W16(esi + 0xc, 0);
            W16(edi + 0xc, 0);
            W16(esi + 8, R16(bA + 0x40));
            W16(edi + 8, R16(bB + 0x40));
            W16(esi + 6, R16(bA + 0xa));
            W16(edi + 6, R16(bB + 0xa));
        } else {
            W8(FR32(0x108) + 0x19, 0);
            W8(FR32(0x10c) + 0x19, 0);
            W8(esi, 1);
            W8(edi, 1);
            W8(esi + 1, 0);
            W8(edi + 1, 0);
            W8(esi + 3, 0xff);
            W8(edi + 3, 0xff);
        }
    }
L_41b435:
    W8(F(0x1b8), FR8(0x19c));
    return;

L_41b448:
    /* a platform object (entity types 0x64 / 0x65) */
    {
        uint32_t ent = R32(FR32(0x98) + 0x14);
        uint32_t type = R32(ent + 0x158);
        if ((type != 0x64 && type != 0x65) || FI32(0x78) > 0xfa0) {
            W8(F(0x1b8), 1);
            return;
        }
        uint32_t ctl = FR32(0x8c), stats = FR32(0x90);
        uint32_t pt = C1(0x0045600cu, type - 0x64);
        int32_t a = tm2_atan2(sub((int32_t)R32(ebp + 0x20), (int32_t)R32(pt)), sub((int32_t)R32(ebp + 0x24), (int32_t)R32(pt + 4)));
        int32_t h = (int32_t)C1(0x00455fe8u, type - 0x64);
        W32(F(0x7c), h);
        W32(F(0x80), sub(h, a));
        wrap32(e, F(0x80));
        int32_t kind;
        if (R16(F(0x170)) == 0xf0) {
            kind = 0x19;
        } else {
            int32_t d = iabs(FI32(0x80));
            if (d > 0x6aa) {
                W16(sum + 4, 1);
                int32_t lim = sdiv(mul((int16_t)C0(0x004931d4u), 0x474), 100) << 5;
                kind = 1;
                if (SAR16(ebp + 8) > lim) W16(ebp + 0xa, sdiv(mul((int16_t)C0(0x004931d4u), 0x474), 100) << 5);
                int32_t r = srem((int32_t)C0(0x004899c2u), 0x2aa);
                W32(F(0x84), r);
                int32_t s;
                if (FI32(0x80) > 0) {
                    W32(ebp + 0x40, sub(FI32(0x7c), r));
                    s = -SAR16(stats + 0x62);
                } else {
                    W32(ebp + 0x40, add(FI32(0x7c), r));
                    s = SAR16(stats + 0x62);
                }
                W16(ebp, s / 2);
                W8(F(0x1b8), 1);
                W8(ctl + 1, 1);
            } else if (d < 0x155) {
                W16(sum + 4, 3);
                W16(ebp + 0xa, sdiv(mul((int16_t)C0(0x004931d4u), 0x76c), 100) << 5);
                int32_t r = srem((int32_t)C0(0x004899c2u), 0x2aa);
                kind = 4;
                W32(F(0x84), r);
                int32_t s;
                if (FI32(0x80) > 0) {
                    W32(ebp + 0x40, sub(FI32(0x7c), r));
                    s = -SAR16(stats + 0x62);
                } else {
                    W32(ebp + 0x40, add(FI32(0x7c), r));
                    s = SAR16(stats + 0x62);
                }
                W16(ebp, s / 2);
                W32(F(0x80), sub(FI32(0x7c), (int32_t)R32(ebp + 0x34)));
                wrap32(e, F(0x80));
                int32_t t = iabs(FI32(0x80));
                if (t > 0x1c7 && t < 0x5c7) {
                    W8(ctl + 0x12, 1);
                    W16(ebp + 2, R16(ebp + 0x40));
                    W16(stats + 0x20, 0);
                    if (FI32(0x80) < 0) {
                        W16(ebp + 0x12, 1);
                        W16(ebp + 4, R16(ebp + 0x40) + 0x400);
                    } else {
                        W16(ebp + 0x12, 0xffff);
                        W16(ebp + 4, R16(ebp + 0x40) - 0x400);
                    }
                }
                C4(0x00421920u, FR32(0x70), FR8(0x1b4), 0xa, 1);
                W8(F(0x1b8), 1);
                W8(ctl + 1, 1);
            } else {
                int32_t s = tm2_sin(div_pow2(FI32(0x84), 2));
                int32_t v = iabs(mul(iabs(SAR16(ebp + 8)), s));
                W16(sum + 4, sdiv(sdiv(mul(v, 3), (int32_t)R32(stats + 0xb4)), 0xfff));
                kind = 2;
                if (FI32(0x84) > 0) W32(ebp + 0x34, R32(ebp + 0x34) + 0x71);
                else W32(ebp + 0x34, R32(ebp + 0x34) - 0x71);
                W8(sum, 1);
                W8(sum + 1, 1);
            }
        }
        /* 0x0041b7c5: damage for a player car */
        uint8_t pl = FR8(0x1b4);
        if (pl) {
            uint32_t who = C2(0x0045b154u, FR32(0x88), pl);
            C5(0x004210e4u, who, (uint32_t)SAR16(sum + 2), 0, 2, 0xffffffffu);
        }
        if (R16(F(0x170)) != 0xf0 && (C0(0x004931f4u) & 5) != 1) return;
        if (R32(R32(FR32(0x98) + 0x14) + 0x158) == 0x64) C3(0x00470d3cu, 0x1fa, 4, (uint32_t)kind);
        else C3(0x00470d3cu, 0x1fb, 5, (uint32_t)kind);
    }
}
