/*
 * cb2_exe_stores.c: a tiny x86 register machine that lists the stores of a constant setter
 * function in the player's TM2.EXE (see cb2_exe_stores.h).
 */
#include "cb2_exe_stores.h"

#include <stdio.h>
#include <string.h>

#define MAX_STEPS 100000
#define STACK_DEPTH 64

typedef struct {
    uint32_t r[8];
    uint8_t k[8];           /* known bytes: bit 0 = byte 0, bit 1 = byte 1, bit 2 = bytes 2..3 */
    int zf, cf, sf, of, flags_ok;
    uint32_t st[STACK_DEPTH];
    uint8_t stk[STACK_DEPTH];
    int sp;
} M;

static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* operand access: size 1 (reg 0..3 = al..bl, 4..7 = ah..bh), 2 or 4 */
static int get(const M* m, int reg, int size, uint32_t* v) {
    if (size == 1) {
        int r = reg & 3, hi = reg >> 2;
        if (!(m->k[r] & (hi ? 2 : 1))) return -1;
        *v = (m->r[r] >> (hi ? 8 : 0)) & 0xff;
        return 0;
    }
    if (size == 2) {
        if ((m->k[reg] & 3) != 3) return -1;
        *v = m->r[reg] & 0xffff;
        return 0;
    }
    if (m->k[reg] != 7) return -1;
    *v = m->r[reg];
    return 0;
}

static void set(M* m, int reg, int size, uint32_t v) {
    if (size == 1) {
        int r = reg & 3, sh = (reg >> 2) ? 8 : 0;
        m->r[r] = (m->r[r] & ~(0xffu << sh)) | (v & 0xff) << sh;
        m->k[r] |= sh ? 2 : 1;
    } else if (size == 2) {
        m->r[reg] = (m->r[reg] & 0xffff0000u) | (v & 0xffff);
        m->k[reg] |= 3;
    } else {
        m->r[reg] = v;
        m->k[reg] = 7;
    }
}

static void flags_sub(M* m, uint32_t a, uint32_t b, int size) {
    uint32_t mask = size == 4 ? 0xffffffffu : (size == 2 ? 0xffffu : 0xffu), sb = mask ^ (mask >> 1);
    a &= mask; b &= mask;
    uint32_t r = (a - b) & mask;
    m->zf = r == 0; m->cf = a < b; m->sf = (r & sb) != 0;
    m->of = (((a ^ b) & (a ^ r)) & sb) != 0;
    m->flags_ok = 1;
}

static void flags_logic(M* m, uint32_t r, int size) {
    uint32_t mask = size == 4 ? 0xffffffffu : (size == 2 ? 0xffffu : 0xffu), sb = mask ^ (mask >> 1);
    r &= mask;
    m->zf = r == 0; m->cf = 0; m->of = 0; m->sf = (r & sb) != 0;
    m->flags_ok = 1;
}

static int cond(const M* m, int cc) {
    switch (cc) {
    case 0x2: return m->cf;
    case 0x3: return !m->cf;
    case 0x4: return m->zf;
    case 0x5: return !m->zf;
    case 0x6: return m->cf || m->zf;
    case 0x7: return !m->cf && !m->zf;
    case 0xc: return m->sf != m->of;
    case 0xd: return m->sf == m->of;
    case 0xe: return m->zf || m->sf != m->of;
    case 0xf: return !m->zf && m->sf == m->of;
    default: return -1;
    }
}

int cb2_exe_run_stores(const PeImage* pe, uint32_t va, const uint32_t init[8], unsigned init_known,
                       Cb2StoreList* out, char* err, size_t errlen) {
    M m;
    memset(&m, 0, sizeof m);
    for (int i = 0; i < 8; i++)
        if (init_known >> i & 1) { m.r[i] = init[i]; m.k[i] = 7; }
    out->n = 0;
    uint32_t a = va;
    for (int step = 0; step < MAX_STEPS; step++) {
        const uint8_t* p = pe_at(pe, a, 16);
        if (!p) { snprintf(err, errlen, "code at %08x not in the image", a); return -1; }
        int at = 0, o16 = 0, cs = 0;
        for (;;) {
            if (p[at] == 0x66) { o16 = 1; at++; }
            else if (p[at] == 0x2e) { cs = 1; at++; }
            else break;
        }
        (void)cs;
        int op = p[at++];
        if (op == 0x0f) op = 0x100 | p[at++];
        int isz = o16 ? 2 : 4;

        /* modrm decode (when used) */
        int mod = 0, reg = 0, rm = 0, sib = -1, mem = 0;
        int32_t disp = 0;
        int has_modrm = op == 0x30 || op == 0x31 || op == 0x88 || op == 0x89 || op == 0x8a || op == 0x8b ||
                        op == 0x8d || op == 0x83 || op == 0x81 || op == 0x39 || op == 0x3b || op == 0x38 ||
                        op == 0x3a || op == 0x29 || op == 0x01 || op == 0xc6 || op == 0xc7 || op == 0xff;
        if (has_modrm) {
            uint8_t b = p[at++];
            mod = b >> 6; reg = (b >> 3) & 7; rm = b & 7;
            if (mod != 3) {
                mem = 1;
                if (rm == 4) sib = p[at++];
                if (mod == 0 && (rm == 5 || (sib >= 0 && (sib & 7) == 5))) { disp = (int32_t)rd32(p + at); at += 4; }
                else if (mod == 1) { disp = (int8_t)p[at]; at += 1; }
                else if (mod == 2) { disp = (int32_t)rd32(p + at); at += 4; }
            }
        }
        uint32_t next = a + (uint32_t)at;
        uint32_t v, w;
        int size;

#define FAIL(...) do { snprintf(err, errlen, __VA_ARGS__); return -1; } while (0)
#define GET(rg, sz, dst) do { if (get(&m, (rg), (sz), &(dst))) FAIL("unknown register read at %08x", a); } while (0)

        /* effective address of a memory operand (no index register) */
        uint32_t ea = 0;
        int absolute = 0;
        if (mem && op != 0xff) {
            if (sib >= 0) FAIL("scaled index operand at %08x", a);
            if (mod == 0 && rm == 5) { ea = (uint32_t)disp; absolute = 1; }
            else { GET(rm, 4, v); ea = v + (uint32_t)disp; }
        }

        switch (op) {
        case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
            if (m.sp >= STACK_DEPTH) FAIL("stack overflow at %08x", a);
            m.st[m.sp] = m.r[op - 0x50]; m.stk[m.sp] = m.k[op - 0x50]; m.sp++;
            break;
        case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f:
            if (m.sp <= 0) FAIL("pop of the caller's stack at %08x", a);
            m.sp--; m.r[op - 0x58] = m.st[m.sp]; m.k[op - 0x58] = m.stk[m.sp];
            break;
        case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
        case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f: {
            int r = op & 7, cf = m.cf;
            GET(r, isz, v);
            uint32_t res = op < 0x48 ? v + 1 : v - 1;
            if (op < 0x48) flags_sub(&m, v, (uint32_t)-1, isz); else flags_sub(&m, v, 1, isz);
            m.cf = cf;      /* inc / dec keep CF */
            set(&m, r, isz, res);
            break;
        }
        case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4: case 0xb5: case 0xb6: case 0xb7:
            set(&m, op - 0xb0, 1, p[at]); next++;
            break;
        case 0xb8: case 0xb9: case 0xba: case 0xbb: case 0xbc: case 0xbd: case 0xbe: case 0xbf:
            if (o16) { set(&m, op - 0xb8, 2, (uint32_t)(p[at] | p[at + 1] << 8)); next += 2; }
            else { set(&m, op - 0xb8, 4, rd32(p + at)); next += 4; }
            break;
        case 0x30: case 0x31:   /* xor r/m, r (registers only) */
            if (mem) FAIL("xor to memory at %08x", a);
            size = op == 0x30 ? 1 : isz;
            if (rm == reg) v = w = 0;          /* xor r, r: known zero */
            else { GET(rm, size, v); GET(reg, size, w); }
            set(&m, rm, size, v ^ w); flags_logic(&m, v ^ w, size);
            break;
        case 0x38: case 0x39: case 0x3a: case 0x3b:   /* cmp (registers only) */
            if (mem) FAIL("cmp with memory at %08x", a);
            size = (op & 1) ? isz : 1;
            GET(rm, size, v); GET(reg, size, w);
            if (op <= 0x39) flags_sub(&m, v, w, size); else flags_sub(&m, w, v, size);
            break;
        case 0x01: case 0x29:   /* add / sub r/m, r (registers only) */
            if (mem) FAIL("arithmetic on memory at %08x", a);
            GET(rm, isz, v); GET(reg, isz, w);
            flags_sub(&m, v, op == 0x01 ? (uint32_t)-w : w, isz);
            set(&m, rm, isz, op == 0x01 ? v + w : v - w);
            break;
        case 0x83: case 0x81: {
            if (mem) FAIL("arithmetic on memory at %08x", a);
            uint32_t imm;
            if (op == 0x83) { imm = (uint32_t)(int32_t)(int8_t)p[at]; next += 1; }
            else if (o16) { imm = (uint32_t)(p[at] | p[at + 1] << 8); next += 2; }
            else { imm = rd32(p + at); next += 4; }
            GET(rm, isz, v);
            if (reg == 0) { flags_sub(&m, v, (uint32_t)-imm, isz); set(&m, rm, isz, v + imm); }
            else if (reg == 5) { flags_sub(&m, v, imm, isz); set(&m, rm, isz, v - imm); }
            else if (reg == 7) flags_sub(&m, v, imm, isz);
            else FAIL("unhandled group 1 operation at %08x", a);
            break;
        }
        case 0x8d:
            if (!mem) FAIL("lea without memory at %08x", a);
            set(&m, reg, isz, ea);
            break;
        case 0x88: case 0x89: case 0x8a: case 0x8b:
            size = (op & 1) ? isz : 1;
            if (!mem) {
                if (op <= 0x89) { GET(reg, size, v); set(&m, rm, size, v); }
                else { GET(rm, size, v); set(&m, reg, size, v); }
                break;
            }
            if (op >= 0x8a) FAIL("memory read at %08x", a);
            GET(reg, size, v);
            goto store;
        case 0xc6: case 0xc7:
            if (!mem || reg != 0) FAIL("unhandled mov form at %08x", a);
            size = op == 0xc6 ? 1 : isz;
            v = size == 1 ? p[next - a] : size == 2 ? (uint32_t)(p[next - a] | p[next - a + 1] << 8) : rd32(p + (next - a));
            next += (uint32_t)size;
            goto store;
        case 0xa2: case 0xa3:
            ea = rd32(p + at); next += 4; absolute = 1;
            size = op == 0xa2 ? 1 : isz;
            GET(CB2_EAX, size, v);
            goto store;
        case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
        case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7e: case 0x7f:
        case 0x180: case 0x181: case 0x182: case 0x183: case 0x184: case 0x185: case 0x186: case 0x187:
        case 0x188: case 0x189: case 0x18a: case 0x18b: case 0x18c: case 0x18d: case 0x18e: case 0x18f: {
            int32_t rel;
            if (op < 0x100) { rel = (int8_t)p[at]; next += 1; }
            else { rel = (int32_t)rd32(p + at); next += 4; }
            if (!m.flags_ok) FAIL("branch on unknown flags at %08x", a);
            int t = cond(&m, op & 0xf);
            if (t < 0) FAIL("unhandled condition at %08x", a);
            a = t ? next + (uint32_t)rel : next;
            continue;
        }
        case 0xeb: a = next + 1 + (uint32_t)(int32_t)(int8_t)p[at]; continue;
        case 0xe9: a = next + 4 + rd32(p + at); continue;
        case 0xff: {
            /* jmp [index * 4 + table] only */
            if (reg != 4 || mod != 0 || rm != 4 || sib < 0 || (sib & 7) != 5 || (sib >> 6) != 2)
                FAIL("unhandled indirect jump at %08x", a);
            GET((sib >> 3) & 7, 4, v);
            const uint8_t* t = pe_at(pe, (uint32_t)disp + v * 4, 4);
            if (!t) FAIL("jump table entry outside the image at %08x", a);
            a = rd32(t);
            continue;
        }
        case 0x90: break;
        case 0xc3:
            if (m.sp != 0) FAIL("ret with pushed values at %08x", a);
            return 0;
        default:
            FAIL("unhandled instruction %02x at %08x", op, a);
        }
        a = next;
        continue;
    store:
        if (out->n >= (int)(sizeof out->s / sizeof out->s[0])) FAIL("too many stores");
        out->s[out->n++] = (Cb2Store){ a, ea, (uint8_t)size, (uint8_t)absolute, v };
        a = next;
#undef GET
#undef FAIL
    }
    snprintf(err, errlen, "no ret within %d instructions", MAX_STEPS);
    return -1;
}
