/*
 * gp3_boss.c: per frame weapon logic of one car with the Dark Tooth boss part, TM2.EXE
 * 0x0043e6e0 (see gp3_boss.h).
 */
#include "gp3_boss.h"

#include <string.h>

#include "../math/tm2math.h"

static int16_t rd16(const uint8_t* p) { int16_t v; memcpy(&v, p, 2); return v; }
static int32_t rd32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static uint32_t rdu(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void wr16(uint8_t* p, int16_t v) { memcpy(p, &v, 2); }
static void wr32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }

static uint32_t call(const Gp3BossEnv* e, uint32_t addr, int n, uint32_t a0, uint32_t a1, uint32_t a2,
                     uint32_t a3, uint32_t a4, void* l1, void* l2) {
    uint32_t a[5] = {a0, a1, a2, a3, a4};
    return e->call(e->user, addr, n, a, l1, l2);
}
#define C0(addr) call(e, addr, 0, 0, 0, 0, 0, 0, 0, 0)
#define C1(addr, a) call(e, addr, 1, (uint32_t)(a), 0, 0, 0, 0, 0, 0)
#define C2(addr, a, b) call(e, addr, 2, (uint32_t)(a), (uint32_t)(b), 0, 0, 0, 0, 0)
#define C3(addr, a, b, c) call(e, addr, 3, (uint32_t)(a), (uint32_t)(b), (uint32_t)(c), 0, 0, 0, 0)
#define C4(addr, a, b, c, d) call(e, addr, 4, (uint32_t)(a), (uint32_t)(b), (uint32_t)(c), (uint32_t)(d), 0, 0, 0)

static int32_t iabs(int32_t v) { return (int32_t)(((uint32_t)v ^ (uint32_t)(v >> 31)) - (uint32_t)(v >> 31)); }
static int32_t sub(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }

/* max(|dx|, |dy|, |dz|), signed compares */
static int32_t cheb(const uint8_t* a, const uint8_t* b) {
    int32_t ax = iabs(sub(rd32(a), rd32(b)));
    int32_t ay = iabs(sub(rd32(a + 4), rd32(b + 4)));
    int32_t az = iabs(sub(rd32(a + 8), rd32(b + 8)));
    int32_t m = ay < az ? az : ay;
    return ax < m ? m : ax;
}

/* the shoot / hold draw of the AI */
static void ai_draw(const Gp3BossEnv* e, uint8_t* c, int second) {
    uint8_t chance = c[0x83e];
    int32_t r = (int32_t)C0(0x4899c2) / 0x148;
    if ((int32_t)chance > r) {
        c[0x839] = second ? 1 : 0;
        if ((int8_t)c[0x841] < 8) { c[0x5d] = 0; c[0x5b] = 1; }
        else { c[0x5d] = 1; c[0x5b] = 0; }
        if (!second && rd16(c + 0x30) == 0x30) {
            if (c[0x36]) c[0x841] = 0;
            else if ((int8_t)c[0x841] == 0) c[0x841] = 1;
        }
    } else {
        if (second) c[0x5d] = 0;
        c[0x5b] = 0;
        if (rd16(c + 0x85a) < 1) wr16(c + 0x85a, rd16(e->g_obj + 4));
    }
}

#define C1CNT (e->g_count + 0)
#define C2CNT (e->g_count + 2)
#define YAWR (e->g_count + 4)
#define ROLLR (e->g_count + 6)
#define B(off) (e->g_bomb + ((off) - 0x4fef28))
#define BOMBH() rdu(e->g_obj)
#define BOMB() e->mem(e->user, BOMBH())
#define TGTH() rdu(B(0x4fef34))
#define TGT() e->mem(e->user, TGTH())

static void boss(const Gp3BossEnv* e, uint32_t car_h) {
    uint8_t* c = e->mem(e->user, car_h);
    c[0x5c] = 0;
    if (c[0x75]) wr16(B(0x4fef2c), 100);
    int16_t old = rd16(C1CNT);
    wr16(C1CNT, (int16_t)(old + 1));
    if (old > rd16(B(0x4fef28)) && c[0x75] == 0) {
        wr16(C1CNT, 0);
        int32_t r = (int32_t)C0(0x4899c2);
        *B(0x4fef2a) ^= 1;
        wr16(B(0x4fef28), (int16_t)(r / 500 + 10));
        int side = rd16(B(0x4fef2a)) != 0;
        int32_t v[3], pt[3], m[9], ang[3];
        uint32_t t = 0;
        int ai1 = c[0xc8] == 1;
        if (ai1) { v[0] = side ? -0x48 : 0x48; v[1] = -0xe0; v[2] = 0x38; }
        else { v[0] = side ? -0x78 : 0x78; v[1] = 0x110; v[2] = 0x68; }
        memcpy(m, c + 0x758, sizeof m);
        tm2_mat_vec_mul_row(m, v, pt);
        for (int i = 0; i < 3; i++) pt[i] = tm2_wadd(pt[i], rd32(c + 0x728 + 4 * i));
        if (ai1) call(e, 0x449048, 3, (uint32_t)(int32_t)rd16(c + 4), 0, 0, 0, 0, &t, 0);
        else call(e, 0x448de4, 4, (uint32_t)(int32_t)rd16(c + 4), 0, 0, 0, 0, &t, 0);
        c = e->mem(e->user, car_h);
        ang[0] = 0xaa;
        ang[1] = 0;
        ang[2] = tm2_wadd(rd16(c + 0x20), rd32(c + 0x73c)) & 0xfff;
        uint32_t model = C1(0x45ae2c, (int32_t)rd16(c + 4));
        call(e, 0x4142ac, 5, rdu(e->mem(e->user, model) + 0x158), t, 0, 0, 5, ang, pt);
        c = e->mem(e->user, car_h);
    }
    {
        uint8_t* b = BOMB();
        wr32(B(0x4fef38), rd32(b + 0xe8));
        wr32(B(0x4fef3c), rd32(b + 0xec));
        wr32(B(0x4fef40), rd32(b + 0xf0));
        uint8_t low = (uint8_t)C1(0x45a8bc, rd32(b + 0xf0));
        wr32(B(0x4fef30), low ? -0x18 : 0xf0);
    }
    if (rd16(C2CNT) == rd16(B(0x4fef2c)) - 0x1e) {
        C1(0x46f974, BOMBH() + 0xe8);
        uint32_t bv = C0(0x46f990);
        uint32_t dv = C0(0x46f988);
        C4(0x46f8b0, 0x3d, dv >> 2, bv, 0x63);
    }
    old = rd16(C2CNT);
    wr16(C2CNT, (int16_t)(old + 1));
    c = e->mem(e->user, car_h);
    if (old > rd16(B(0x4fef2c)) && c[0x75] == 0) {
        int16_t idx = (int16_t)C2(0x45acb8, car_h + 0x728, -1);
        wr16(B(0x4fef50), idx);
        wr32(B(0x4fef34), (int32_t)(C1(0x45afbc, (int32_t)idx) + 0x6e4));
        c = e->mem(e->user, car_h);
        if ((cheb(c + 0x728, TGT()) >> 3) < 500) {
            wr16(C2CNT, 0);
            wr16(B(0x4fef2c), (int16_t)((int32_t)C0(0x4899c2) / 0x147 + 100));
            wr16(B(0x4fef2e), 1);
            C1(0x46f974, BOMBH() + 0xe8);
            uint32_t bv = C0(0x46f990);
            uint32_t dv = C0(0x46f988);
            C4(0x46f8b0, 0x42, dv / 3u, bv, 0x63);
        }
    }
    c = e->mem(e->user, car_h);
    if (rd16(B(0x4fef2e)) == 0) {
        int32_t v[3] = {0, 0, rd32(B(0x4fef30))}, o[3], m[9];
        memcpy(m, c + 0x758, sizeof m);
        tm2_mat_vec_mul_col(m, v, o);
        for (int i = 0; i < 3; i++) o[i] = tm2_wadd(o[i], rd32(c + 0x728 + 4 * i));
        for (int i = 0; i < 3; i++) {
            uint8_t* b = BOMB();
            wr32(b + 0xe8 + 4 * i, tm2_angle_step(rd32(b + 0xe8 + 4 * i), o[i], 0x14));
        }
        c = e->mem(e->user, car_h);
        if (c[0x75] == 0) {
            uint8_t* b = BOMB();
            wr16(b + 0xe2, (int16_t)(rd16(b + 0xe2) + rd16(ROLLR)));
            if (rd16(b + 0xe2) > 0x2aa) wr16(ROLLR, -100);
            b = BOMB();
            if (rd16(b + 0xe2) < -0x2aa) wr16(ROLLR, 100);
            b = BOMB();
            wr16(b + 0xe4, (int16_t)(rd16(b + 0xe4) + rd16(YAWR)));
            int16_t a[3];
            int32_t mm[9];
            memcpy(a, b + 0xe0, sizeof a);
            tm2_build_matrix(a, mm);
            memcpy(b + 0xb0, mm, sizeof mm);
        }
    } else {
        for (int i = 0; i < 3; i++) {
            uint8_t* b = BOMB();
            wr32(b + 0xe8 + 4 * i, tm2_angle_step(rd32(b + 0xe8 + 4 * i), rd32(TGT() + 4 * i), 0xd));
        }
        {
            uint8_t* b = BOMB();
            int32_t s = tm2_sin(tm2_wmul(rd16(C2CNT), 200));
            wr32(b + 0xf0, tm2_wadd(rd32(b + 0xf0), tm2_wmul(s, 0xa0) / 0xfff));
            int32_t dx = sub(rd32(b + 0xe8), rd32(TGT()));
            int32_t dy = sub(rd32(b + 0xec), rd32(TGT() + 4));
            int32_t at = tm2_atan2(dx, dy ? dy : 1);
            b = BOMB();
            wr16(b + 0xe4, (int16_t)tm2_angle_interp(rd16(b + 0xe4), at, 3));
            int16_t a[3];
            int32_t mm[9];
            memcpy(a, b + 0xe0, sizeof a);
            tm2_build_matrix(a, mm);
            memcpy(b + 0xb0, mm, sizeof mm);
        }
        if ((cheb(BOMB() + 0xe8, TGT()) >> 3) < 0x14) {
            C2(0x450984, BOMBH() + 0xe8, 0);
            uint8_t* b = BOMB();
            wr32(B(0x4fef44), sub(rd32(b + 0xe8), rd32(B(0x4fef38))));
            wr32(B(0x4fef48), sub(rd32(b + 0xec), rd32(B(0x4fef3c))));
            wr32(B(0x4fef4c), 0);
            c = e->mem(e->user, car_h);
            int32_t push = rd16(c + 6);
            uint32_t model = C1(0x45ae24, (int32_t)rd16(B(0x4fef50)));
            call(e, 0x4210e4, 5, rdu(e->mem(e->user, model) + 0x158), 7, e->g_bomb_addr + 0x1c, 0, (uint32_t)push, 0, 0);
#define TC() e->mem(e->user, C1(0x45afbc, (int32_t)rd16(B(0x4fef50))))
            if (TC()[0xb4] == 0 && TC()[0xa8] == 0 && TC()[0xbb] == 0 && TC()[0xbd] == 0) {
                TC()[0xb4] = 1;
                wr16(TC() + 0x15a, 0);
                uint32_t r = C2(0x450de4, TGTH(), 1);
                wr32(TC() + 0x88c, (int32_t)r);
                C3(0x451014, TGTH(), 0, 0x63);
                C2(0x4508f4, TGTH(), 0);
                C4(0x46f8b0, 6, 0, 0, 0x63);
            }
#undef TC
            wr16(B(0x4fef2e), 0);
        } else {
            uint8_t l[4] = {0};
            uint32_t p = call(e, 0x48df8c, 4, rdu(BOMB() + 0x158), (uint32_t)-1, BOMBH() + 0xe8, 0, 0, l, 0);
            if (rd32(e->mem(e->user, p)) != 0) {
                C2(0x450984, BOMBH() + 0xe8, 0);
                wr16(B(0x4fef2e), 0);
            }
        }
    }
    c = e->mem(e->user, car_h);
    if ((cheb(c + 0x728, TGT()) >> 3) >= 500 || rd16(C2CNT) > 0x28) wr16(B(0x4fef2e), 0);
}

void gp3_boss_update(const Gp3BossEnv* e, uint32_t car_h, uint8_t human) {
    uint8_t* c = e->mem(e->user, car_h);
    uint32_t ws_h, input_h, flags_h;
    int32_t id;
    if (human) {
        uint32_t w = car_h + 0x848;
        if (c[0xe6]) {
            if (c[0xe3]) { C3(0x4469bc, w, 1, 8); c = e->mem(e->user, car_h); c[0xe6] = 0; }
            else if (c[0xe4]) { C3(0x4469bc, w, -1, 8); c = e->mem(e->user, car_h); c[0xe6] = 0; }
            else if ((int8_t)c[0xe5] > 0) { C3(0x4469bc, w, (int32_t)(int8_t)c[0xe5], 8); c = e->mem(e->user, car_h); c[0xe6] = 0; }
        } else if (c[0xe3] == 0 && c[0xe4] == 0 && (int8_t)c[0xe5] == 0) {
            c[0xe6] = 1;
        }
        ws_h = car_h + 0x848; input_h = car_h + 0xcc; flags_h = car_h + 0x9a;
        id = rd16(c);
    } else {
        if (c[0x32]) {
            int32_t n = 4;
            if (c[0x36]) n = 3;
            if (c[0x37]) n = 2;
            if (c[0x38]) n = 1;
            int32_t a = iabs(rd16(c + 0x20));
            if (a < 0x88 && rd32(c + 0x28) < 4000 && c[0x71] == 0) c[0x5c] = 1;
            if (iabs(rd16(c + 0x20)) < n * 0x71) { ai_draw(e, c, 0); c = e->mem(e->user, car_h); }
            if (c[0xc8] == 1 && iabs(rd16(c + 0x20)) > ((0xb4 - n * 10) << 12) / 0x168) {
                ai_draw(e, c, 1);
                c = e->mem(e->user, car_h);
            }
        }
        if ((uint8_t)C0(0x474508) == 8 && rd16(e->mem(e->user, car_h) + 0x30) == 0xf0) boss(e, car_h);
        c = e->mem(e->user, car_h);
        ws_h = car_h + 0x834; input_h = car_h + 0x40; flags_h = car_h + 0x68;
        id = rd16(c + 4);
    }
    uint8_t* ws = e->mem(e->user, ws_h);
    if (rd32(ws) != 0 || !human) {
        for (int k = 0; k < 10; k++) wr16(ws + 0xe + 2 * k, 1);
        wr16(ws + 0x32, 0x3de);
        wr16(ws + 0x30, 3000);
        wr16(ws + 0x1e, 0);
    }
    int16_t t = (int16_t)(rd16(ws + 0x2e) + tm2_frame_ticks60());
    wr16(ws + 0x2e, t);
    if (!(t < rd16(ws + 0x2c))) {
        wr16(ws + 0xe, (int16_t)(rd16(ws + 0xe) + 1));
        wr16(ws + 0x2e, 0);
        if (rd16(ws + 0xe) > (int8_t)ws[0xb]) wr16(ws + 0xe, (int16_t)(int8_t)ws[0xb]);
        if ((int8_t)ws[0xd] >= 8 && rd16(ws + 0xe) >= 1) ws[0xd] = 0;
    }
    wr16(ws + 0x30, (int16_t)(rd16(ws + 0x30) + tm2_frame_ticks60()));
    if (rd16(ws + 0x30) > 3000) wr16(ws + 0x30, 3000);
    C2(0x4401fc, car_h, human);
    C2(0x44342c, car_h, human);
    uint8_t* in = e->mem(e->user, input_h);
    if (in[0x1c]) {
        in[0x1c] = 0;
        C2(0x443c9c, car_h, human);
        uint8_t* f = e->mem(e->user, flags_h);
        if (f[0] == 0 && f[0xd] == 0) return;
        if (human) C2(0x46f860, 9, id);
        else C2(0x46f898, 9, id);
        return;
    }
    if (human) {
        if (id == 0) { C2(0x46f860, 9, 0); return; }
        if (C0(0x46feb0) != 0) return;
        C2(0x46f860, 9, id);
        return;
    }
    C2(0x46f898, 9, id);
}
