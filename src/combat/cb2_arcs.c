/*
 * cb2_arcs.c: weaponArcsDraw, TM2.EXE 0x00447748 (see cb2_arcs.h).
 */
#include "cb2_arcs.h"

#include <string.h>

#include "../engine/en_misc.h"
#include "../math/tm2math.h"

static int16_t rd16(const uint8_t* p) { int16_t v; memcpy(&v, p, 2); return v; }
static uint32_t rd32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void wr16(uint8_t* p, int16_t v) { memcpy(p, &v, 2); }
static void wr32(uint8_t* p, uint32_t v) { memcpy(p, &v, 4); }

void cb2_arcs_draw(const Cb2ArcsEnv* e, uint32_t ot) {
    uint8_t prim[64];
    int32_t pt[16][3];
    int32_t local[3];       /* the original's stack local whose address goes into the record */
    memset(prim, 0, sizeof prim);
    en_prim_init(prim, 0);
    en_prim_set_38(prim, 0, 0);
    *e->active = 0;
#define POS(h) ((h) == e->local_handle ? local : e->resolve(e->user, (h)))
    for (int k = 0; k < 5; k++) {
        uint8_t* r = e->arcs + 0x14 * k;
        if (rd16(r + 4) < 1) continue;
        *e->active = 1;
        if (rd16(r + 8) < 0) {
            wr32(r + 0x10, e->local_handle);
            const int32_t* s = POS(rd32(r + 0x0c));
            local[0] = (int32_t)((uint32_t)s[0] + (uint32_t)(tm2_cos(rd16(r + 4) * 400) / 5));
            s = POS(rd32(r + 0x0c));
            local[1] = (int32_t)((uint32_t)s[1] + (uint32_t)(tm2_sin(rd16(r + 4) * 400) / 5));
            local[2] = POS(rd32(r + 0x0c))[2];
        }
        const int32_t* d = POS(rd32(r + 0x10));
        const int32_t* s = POS(rd32(r + 0x0c));
        int32_t dx = (int32_t)((uint32_t)d[0] - (uint32_t)s[0]);
        int32_t dy = (int32_t)((uint32_t)d[1] - (uint32_t)s[1]);
        int32_t dz = (int32_t)((uint32_t)d[2] - (uint32_t)s[2]);
        pt[0][0] = s[0];
        pt[0][1] = s[1];
        pt[0][2] = (int32_t)((uint32_t)s[2] + 0x1e);
        for (int a = 0; a < 3; a++) {
            int32_t v = (int32_t)e->rand(e->user);
            pt[15][a] = (int32_t)((uint32_t)POS(rd32(r + 0x10))[a] + (uint32_t)(v / 0x333));
        }
        for (int i = 1; i < 15; i++) {
            s = POS(rd32(r + 0x0c));
            pt[i][0] = (int32_t)((uint32_t)(tm2_wmul(dx, i) / 16) + (uint32_t)s[0]);
            pt[i][1] = (int32_t)((uint32_t)(tm2_wmul(dy, i) / 16) + (uint32_t)s[1]);
            pt[i][2] = (int32_t)((uint32_t)(tm2_wmul(dz, i) / 16) + (uint32_t)s[2] + 0x1e);
            for (int a = 2; a >= 0; a--)
                pt[i][a] = (int32_t)((uint32_t)pt[i][a] + ((e->rand(e->user) & 0x3f) - 0x20));
        }
        if ((int32_t)e->rand(e->user) < 0x200) e->smoke(e->user, rd32(r + 0x10));
        if ((int32_t)e->rand(e->user) < 0x200) e->explosion(e->user, rd32(r + 0x10));
        if (rd16(r + 8) >= 0) {
            int16_t t = rd16(r + 4);
            if (t == 0x78 || t <= 0) e->damage(e->user, rd16(r + 8), 10, 0, 2, rd16(r + 2));
        }
        wr16(r + 4, (int16_t)(rd16(r + 4) - e->ticks(e->user)));
        if (rd16(r + 4) < 0) wr16(r + 4, 0);
        for (int i = 0; i < 15; i++) {
            en_prim_set_vec(prim, 0, pt[i][0], pt[i][1], pt[i][2]);
            en_prim_set_vec(prim, 1, pt[i + 1][0], pt[i + 1][1], pt[i + 1][2]);
            if (rd16(r + 6) == 0) en_prim_set_rgb(prim, 0, 0xaa, 0xb4, 0xff);
            else en_prim_set_rgb(prim, 0, 0xff, 0xc8, 0x32);
            e->draw(e->user, ot, prim);
        }
    }
#undef POS
}
