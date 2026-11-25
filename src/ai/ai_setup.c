/*
 * ai_setup.c: AI route data setup, ported from TM2.EXE (see ai.h). Verified against the original
 * under emulation (tools/emu/check_ai_setup.sh).
 */
#include "ai.h"
#include "ai_mem.h"


static uint8_t* nd(uint8_t* nodes, int i) { return nodes + AI_NODE_SIZE * i; }
static void count(uint8_t* nodes, int i, uint8_t c) { nd(nodes, i)[0x16] = c; }
static void succ(uint8_t* nodes, int i, int k, int16_t s) { w16(nd(nodes, i), 0xa + 2 * k, s); }
/* GRP route tables from zone 24 */
static void route(uint8_t* grp, int to, int8_t node, int8_t zone) {
    grp[0x354 + 24 * 35 + to] = (uint8_t)node;
    grp[0x81d + 24 * 35 + to] = (uint8_t)zone;
}

void ai_route_patch_boss_level(uint8_t* nodes, uint8_t* grp) {
    static const uint8_t two[] = { 4, 9, 41, 43, 45, 5, 8, 37, 39, 112, 117 };
    static const uint8_t one[] = { 25, 49, 51, 53, 55 };
    for (unsigned k = 0; k < sizeof two; k++) count(nodes, two[k], 2);
    for (unsigned k = 0; k < sizeof one; k++) count(nodes, one[k], 1);
    count(nodes, 0, 3);
    succ(nodes, 11, 3, 44);
    succ(nodes, 44, 0, 11);
    succ(nodes, 112, 0, 11);
    succ(nodes, 112, 1, 44);
    succ(nodes, 2, 3, 40);
    succ(nodes, 40, 1, 2);
    succ(nodes, 117, 0, 2);
    succ(nodes, 117, 1, 40);
    /* nodes 84..95 in four groups of three: one successor, fixed x */
    static const int16_t to[4] = { 4, 9, 11, 2 };
    static const int32_t x[4] = { -0x12c0, 0x12c0, 0x12c0, -0x12c0 };
    for (int g = 0; g < 4; g++) {
        for (int i = 84 + 3 * g; i < 87 + 3 * g; i++) {
            succ(nodes, i, 0, to[g]);
            w32(nd(nodes, i), 0, x[g]);
            count(nodes, i, 1);
        }
    }
    /* take-route nodes from zone 24 to zones 4..7 and 20..22 */
    static const int8_t take[][2] = { { 4, 44 }, { 5, 40 }, { 6, 4 }, { 7, 9 }, { 20, 49 }, { 21, 51 }, { 22, 53 } };
    for (unsigned k = 0; k < sizeof take / sizeof take[0]; k++) grp[0xce6 + 24 * 35 + take[k][0]] = (uint8_t)take[k][1];
    route(grp, 8, 37, 4);
    route(grp, 14, 37, 4);
    route(grp, 18, 37, 4);
    route(grp, 11, 37, 5);   /* as in the original: node 37 with zone 5 */
    route(grp, 9, 39, 5);
    route(grp, 10, 39, 5);
    route(grp, 15, 39, 5);
    route(grp, 17, 39, 5);
}

void ai_car_setup_profile(uint8_t* ai, const AiEnv* env) {
    uint8_t base = (uint8_t)(env->event_type * 3);
    uint8_t skill = (uint8_t)((uint8_t)env->difficulty * 12);
    uint16_t type = (uint16_t)i16(ai, 0x30);
    int32_t add = -1;   /* per type base added to event * 3 + difficulty * 12; -2 = fixed 0x50 */
    switch (type) {
    case 0x10: case 0xe0: case 0xf0: add = -2; break;
    case 0x20: case 0x30: case 0x50: case 0x60: case 0x70: case 0x80: case 0xa0: case 0xb0: add = 0x11; break;
    case 0x40: add = 0x16; break;
    case 0x90: case 0xd0: add = 0x1e; break;
    case 0xc0: add = 0x14; break;
    default: break;
    }
    if (add != -1) {
        uint32_t t = 0x004e64dcu + 0x18u * (uint32_t)(type / 0x10 - 1);
        w32(ai, 0x86c, (int32_t)t);
        w32(ai, 0x870, (int32_t)(t + 0xc));
        ai[0x83e] = add == -2 ? 0x50 : (uint8_t)(base + add + skill);
    }
    if (env->event_type == 6 || env->event_type == 5) ai[0x83e] = (uint8_t)(ai[0x83e] + 0x2d);
    if (env->game_mode == 2) ai[0x83e] = (uint8_t)(ai[0x83e] + 0xf);
    if (env->game_mode == 3 && env->ai_count == 1) ai[0x83e] = 100;
    if (ai[0x83e] >= 100) ai[0x83e] = 100;
}
