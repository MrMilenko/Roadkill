/*
 * en5_dbscan.c: dbscanHierarchy 0x004a15d4 (see en5_dbscan.h).
 *
 * Globals: 0x00a81e38 stack count, 0x00a81e34 the stack (entries of 0x18 bytes: node, matrix
 * index, x, y, z, level; at most 0x44d, the last slot is overwritten when full), 0x00a81e3c the
 * matrix array (0x30 bytes each; matrix 0 = the identity at 0x004e8450 in the EXE), 0x00a81e40
 * the base position. The walk pops a node and dispatches on its type byte (+0):
 *   0 mesh: meshSetPrimQuadrants 0x0048b460(node); emit with flag = u16 +2
 *   1 group: emit flag +2; the node becomes the callback's ecx; children: byte +0x14, at +0x18
 *   2 list: flag +2; dword count +0x10 (signed), children = [[+0x14 + 4 i] + 8]
 *   3 animated: flag +2; +0x0a == 1 -> +0x0d = 1; +0x12 == 1 -> animRegister 0x004a1430(node);
 *     children byte +0x0b at +0x20
 *   4 translate: flag +2; position += (+4, +8, +0x0c); children byte +0x12 at +0x14
 *   5 transform: flag +2; position += (+0x28, +0x2c, +0x30), kept as v (emitted instead of the
 *     position for this node); position = M(+4) * v (0x0048cb80); M transposed (0x0048bc3c) times
 *     the current matrix (0x0048b960 / 0x0048bfb8) into the matrix of the stack slot index;
 *     children byte +0x36 at +0x38, each with its own stack index as matrix index (ORIGINAL quirk:
 *     only the first index's matrix was written)
 *   6, 7, 0x0a, > 0x0d: flag 0;  8, 0x0d: flag +2 (nothing pushed)
 *   9 switch: flag +2; children byte +6 at +0x10
 *   0x0b pair: flag +2; children +8 and +0x0c
 *   0x0c level: flag 0; position shifted by (level - u16 +4) (left for negative, arithmetic right
 *     for positive); children u16 +6 at +8 with level u16 +4
 * Children are pushed in order (so popped last first) with the parent's matrix index, position and
 * level. Emit (flag != 0): the position (or v for type 5) is scaled by the level difference to
 * lvl0 (shifts by the count's low 5 bits, as x86 does), transformed by the entry's matrix
 * (0x0048cb80) into out, and cb(flag, node, &out, group node) is called (eax, edx, ebx, ecx).
 * At the end the stack count is -1 (it is left at the failed pop). No stack: nothing (count
 * rewritten unchanged).
 */
#include "en5_dbscan.h"

#define SPA 0x00a81e38u
#define STA 0x00a81e34u
#define MTA 0x00a81e3cu

typedef struct { uint32_t node, mi, x, y, z, lvl; } Ent;

static void put(En2Env* e, int32_t i, const Ent* t)
{
    uint32_t a = en2_r32(e, STA) + (uint32_t)i * 0x18u;
    en2_w32(e, a, t->node);
    en2_w32(e, a + 4, t->mi);
    en2_w32(e, a + 8, t->x);
    en2_w32(e, a + 12, t->y);
    en2_w32(e, a + 16, t->z);
    en2_w32(e, a + 20, t->lvl);
}

static uint32_t shl(uint32_t v, int32_t d) { return v << ((uint32_t)(-(uint32_t)d) & 31); }
static uint32_t sar(uint32_t v, int32_t d) { return (uint32_t)((int32_t)v >> ((uint32_t)d & 31)); }

/* push the children with the given fields; returns the new count. ebp grows only below 0x44c. */
static int32_t push(En2Env* e, int32_t ebp, uint32_t child, const Ent* c)
{
    Ent t = *c;
    t.node = child;
    put(e, ebp, &t);
    if (ebp < 0x44c) ebp++;
    return ebp;
}

void en5_dbscan_hierarchy(En2Env* e, uint32_t root, uint32_t lvl0, uint32_t cb)
{
    const uint32_t S = e->scratch;
    const uint32_t L30 = S + 0x30, L68 = S + 0x68, L78 = S + 0x78, L84 = S + 0x84, L9c = S + 0x9c;
    uint32_t bx = en2_r32(e, 0x00a81e40u), by = en2_r32(e, 0x00a81e44u), bz = en2_r32(e, 0x00a81e48u);
    uint32_t group = 0;
    int32_t ebp = (int32_t)en2_r32(e, SPA);
    if (en2_r32(e, STA) == 0) {
        en2_w32(e, SPA, (uint32_t)ebp);
        return;
    }
    Ent r0 = {root, 0, bx, by, bz, lvl0};
    put(e, 0, &r0);
    en2_w32(e, SPA, 1);
    for (uint32_t k = 0; k < 0x30; k += 4) en2_w32(e, en2_r32(e, MTA) + k, en2_r32(e, 0x004e8450u + k));
    for (;;) {
        ebp = (int32_t)en2_r32(e, SPA) - 1;
        if (ebp < 0) {
            en2_w32(e, SPA, (uint32_t)ebp);
            return;
        }
        uint32_t a = en2_r32(e, STA) + (uint32_t)ebp * 0x18u;
        Ent c = {en2_r32(e, a), en2_r32(e, a + 4), en2_r32(e, a + 8), en2_r32(e, a + 12), en2_r32(e, a + 16),
                 en2_r32(e, a + 20)};
        en2_w32(e, SPA, (uint32_t)ebp);
        uint32_t n = c.node, flag = 0;
        uint8_t type = en2_r8(e, n);
        switch (type) {
        case 0:
            en2_call1(e, 0x0048b460u, n);
            flag = en2_r16(e, n + 2);
            break;
        case 1:
            flag = en2_r16(e, n + 2);
            group = n;
            for (uint32_t i = 0; i < en2_r8(e, n + 0x14); i++) {
                ebp = push(e, ebp, en2_r32(e, n + 0x18 + 4 * i), &c);
                en2_w32(e, SPA, (uint32_t)ebp);
            }
            en2_w32(e, SPA, (uint32_t)ebp);
            break;
        case 2:
            flag = en2_r16(e, n + 2);
            for (int32_t i = 0; i < (int32_t)en2_r32(e, n + 0x10); i++) {
                ebp = push(e, ebp, en2_r32(e, en2_r32(e, n + 0x14 + 4u * (uint32_t)i) + 8), &c);
                en2_w32(e, SPA, (uint32_t)ebp);
            }
            break;
        case 3:
            flag = en2_r16(e, n + 2);
            if (en2_r8(e, n + 0x0a) == 1) en2_w8(e, n + 0x0d, 1);
            en2_w32(e, SPA, (uint32_t)ebp);
            if (en2_r8(e, n + 0x12) == 1) en2_call1(e, 0x004a1430u, n);
            ebp = (int32_t)en2_r32(e, SPA);
            for (uint32_t i = 0; i < en2_r8(e, n + 0x0b); i++) {
                ebp = push(e, ebp, en2_r32(e, n + 0x20 + 4 * i), &c);
                en2_w32(e, SPA, (uint32_t)ebp);
            }
            en2_w32(e, SPA, (uint32_t)ebp);
            break;
        case 4:
            flag = en2_r16(e, n + 2);
            c.x += en2_r32(e, n + 4);
            c.y += en2_r32(e, n + 8);
            c.z += en2_r32(e, n + 0x0c);
            for (uint32_t i = 0; i < en2_r8(e, n + 0x12); i++) {
                ebp = push(e, ebp, en2_r32(e, n + 0x14 + 4 * i), &c);
                en2_w32(e, SPA, (uint32_t)ebp);
            }
            en2_w32(e, SPA, (uint32_t)ebp);
            break;
        case 5: {
            flag = en2_r16(e, n + 2);
            c.x += en2_r32(e, n + 0x28);
            c.y += en2_r32(e, n + 0x2c);
            c.z += en2_r32(e, n + 0x30);
            en2_w32(e, L68, c.x); en2_w32(e, L68 + 4, c.y); en2_w32(e, L68 + 8, c.z);
            en2_w32(e, L78, c.x); en2_w32(e, L78 + 4, c.y); en2_w32(e, L78 + 8, c.z);
            en2_call3(e, 0x0048cb80u, n + 4, L78, L68);
            en2_call2(e, 0x0048bc3cu, n + 4, L30);
            en2_call1(e, 0x0048b960u, en2_r32(e, MTA) + c.mi * 0x30u);
            ebp = (int32_t)en2_r32(e, SPA);
            en2_call4(e, 0x0048bfb8u, L30, en2_r32(e, MTA) + (uint32_t)ebp * 0x30u, L68, 0);
            ebp = (int32_t)en2_r32(e, SPA);
            c.x = en2_r32(e, L68); c.y = en2_r32(e, L68 + 4); c.z = en2_r32(e, L68 + 8);
            for (uint32_t i = 0; i < en2_r8(e, n + 0x36); i++) {
                Ent t = c;
                t.node = en2_r32(e, n + 0x38 + 4 * i);
                t.mi = (uint32_t)ebp;
                put(e, ebp, &t);
                if (ebp < 0x44c) ebp++;
                en2_w32(e, SPA, (uint32_t)ebp);
            }
            en2_w32(e, SPA, (uint32_t)ebp);
            break;
        }
        case 8: case 0x0d:
            flag = en2_r16(e, n + 2);
            break;
        case 9:
            flag = en2_r16(e, n + 2);
            for (uint32_t i = 0; i < en2_r8(e, n + 6); i++) {
                ebp = push(e, ebp, en2_r32(e, n + 0x10 + 4 * i), &c);
                en2_w32(e, SPA, (uint32_t)ebp);
            }
            en2_w32(e, SPA, (uint32_t)ebp);
            break;
        case 0x0b:
            flag = en2_r16(e, n + 2);
            ebp = push(e, ebp, en2_r32(e, n + 8), &c);
            ebp = push(e, ebp, en2_r32(e, n + 0x0c), &c);
            en2_w32(e, SPA, (uint32_t)ebp);
            break;
        case 0x0c: {
            uint32_t lv = en2_r16(e, n + 4);
            int32_t d = (int32_t)(c.lvl - lv);
            if (d < 0) { c.x = shl(c.x, d); c.y = shl(c.y, d); c.z = shl(c.z, d); }
            else if (d > 0) { c.y = sar(c.y, d); c.z = sar(c.z, d); c.x = sar(c.x, d); }
            ebp = (int32_t)en2_r32(e, SPA);
            for (uint32_t i = 0; i < en2_r16(e, n + 6); i++) {
                Ent t = c;
                t.lvl = en2_r16(e, n + 4);
                ebp = push(e, ebp, en2_r32(e, n + 8 + 4 * i), &t);
                en2_w32(e, SPA, (uint32_t)ebp);
            }
            en2_w32(e, SPA, (uint32_t)ebp);
            break;
        }
        default:   /* 6, 7, 0x0a, above 0x0d */
            flag = 0;
            break;
        }
        if (flag == 0) continue;
        int32_t d = (int32_t)(c.lvl - lvl0);
        uint32_t m = en2_r32(e, MTA) + c.mi * 0x30u;
        if (type == 5) {
            uint32_t v[3] = {en2_r32(e, L78), en2_r32(e, L78 + 4), en2_r32(e, L78 + 8)};
            for (int k = 0; k < 3; k++) v[k] = d < 0 ? shl(v[k], d) : (d > 0 ? sar(v[k], d) : v[k]);
            for (int k = 0; k < 3; k++) en2_w32(e, L78 + 4u * (uint32_t)k, v[k]);
            en2_call3(e, 0x0048cb80u, m, L78, L84);
        } else {
            uint32_t p[3] = {c.x, c.y, c.z};
            for (int k = 0; k < 3; k++) en2_w32(e, L9c + 4u * (uint32_t)k, d < 0 ? shl(p[k], d) : (d > 0 ? sar(p[k], d) : p[k]));
            en2_call3(e, 0x0048cb80u, m, L9c, L84);
        }
        en2_call4(e, cb, flag, n, L84, group);
    }
}
