/*
 * dpc.c: DPC relocation, ported from TM2.EXE (see dpc.h).
 */
#include "dpc.h"

#include <stdlib.h>

static uint32_t rd32(const uint8_t* b, size_t o) { return b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24; }
static void wr32(uint8_t* b, size_t o, uint32_t v) { b[o] = (uint8_t)v; b[o + 1] = (uint8_t)(v >> 8); b[o + 2] = (uint8_t)(v >> 16); b[o + 3] = (uint8_t)(v >> 24); }
static uint16_t rd16(const uint8_t* b, size_t o) { return (uint16_t)(b[o] | b[o + 1] << 8); }
static void wr16(uint8_t* b, size_t o, uint16_t v) { b[o] = (uint8_t)v; b[o + 1] = (uint8_t)(v >> 8); }

int dpc_read_header(const uint8_t* buf, size_t size, DpcHeader* out) {
    if (size < 0x18 || rd32(buf, 0) != 0x4d504344u || rd32(buf, 4) != 0x43) return -1;
    out->base = rd32(buf, 0xc);
    out->alloc_count = rd32(buf, 0x10);
    out->root_list = rd32(buf, 0x14);
    return 0;
}

/* FUN_004502b0 form: signed division by 4 (truncates toward zero). */
uint32_t dpc_reloc(uint32_t ps1_ptr, uint32_t base, uint32_t addr_base) {
    int32_t d = (int32_t)(ps1_ptr - base);
    return addr_base + (uint32_t)((d / 4) * 4);
}

/* FUN_0049eee0 form: arithmetic shift (rounds down). */
static uint32_t reloc_shr(uint32_t ps1_ptr, uint32_t base, uint32_t addr_base) {
    int32_t d = (int32_t)(ps1_ptr - base);
    return addr_base + (uint32_t)((d >> 2) * 4);
}

typedef struct {
    uint8_t* buf;
    size_t size;
    uint32_t base, addr_base;
    int16_t tex_shift;
    uint32_t* stack;
    size_t cap;
    int err;
} Walk;

/* buffer offset of a relocated pointer value, or SIZE_MAX if it does not fit need bytes */
static size_t off_of(Walk* w, uint32_t v, size_t need) {
    uint32_t o = v - w->addr_base;
    if ((size_t)o > w->size || w->size - o < need) { w->err = -2; return (size_t)-1; }
    return o;
}

/* relocate field at node offset o + f in place, returns the new value */
static uint32_t fix(Walk* w, size_t o, size_t f) {
    if (o + f + 4 > w->size) { w->err = -2; return 0; }
    uint32_t v = reloc_shr(rd32(w->buf, o + f), w->base, w->addr_base);
    wr32(w->buf, o + f, v);
    return v;
}

static void push(Walk* w, size_t* top, uint32_t v) {
    if (*top >= w->cap) {
        size_t nc = w->cap * 2;
        uint32_t* ns = realloc(w->stack, nc * sizeof *ns);
        if (!ns) { w->err = -3; return; }
        w->stack = ns; w->cap = nc;
    }
    w->stack[(*top)++] = v;
}

/* FUN_0049f440: shift the 9 bit texture page of every polygon in a type 0 node */
static void tex_shift_polys(Walk* w, size_t node) {
    uint32_t n = rd32(w->buf, node + 0x10);
    if (n == 0) return;
    size_t p = off_of(w, rd32(w->buf, node + 0xc), 0x18);
    for (uint32_t i = 0; i < n && !w->err; i++) {
        if (p == (size_t)-1 || p + 0x18 > w->size) { w->err = -2; return; }
        uint8_t b0 = w->buf[p];
        if (b0 & 0x7f) {
            size_t t = p + (size_t)(b0 & 0x7f) * 4 + ((rd32(w->buf, p + 0x10) & 0x7fffffffu) >> 28) * 4 + 0x18;
            if (t + 2 > w->size) { w->err = -2; return; }
            uint16_t u = rd16(w->buf, t);
            wr16(w->buf, t, (uint16_t)(((u >> 8 & 0xfe) << 8) | (((u & 0x1ff) + w->tex_shift) & 0x1ff)));
        }
        p += (size_t)w->buf[p + 2] * 4;
    }
}

/* push count children whose pointers start at node + f (relocated in place) */
static void children(Walk* w, size_t node, size_t f, uint32_t count, size_t* top) {
    for (uint32_t i = 0; i < count && !w->err; i++) push(w, top, fix(w, node, f + 4 * i));
}

/* FUN_0049eee0 */
static void walk(Walk* w, uint32_t root) {
    size_t n = 0;
    push(w, &n, root);
    while (n >= 1 && !w->err) {
        size_t top = n - 1;
        uint32_t v = w->stack[top];
        if (v != 0) {
            size_t o = off_of(w, v, 0x3a);
            if (o == (size_t)-1) {
                /* small nodes near the end of the buffer: retry with the smallest header */
                w->err = 0;
                o = off_of(w, v, 0x10);
                if (o == (size_t)-1) return;
            }
            uint8_t* b = w->buf;
            int done = b[o + 1] == DPC_DONE_MARK;
            switch (b[o]) {
            case 0:
                if (!done) {
                    fix(w, o, 4); fix(w, o, 8); fix(w, o, 0xc);
                    if (w->tex_shift != 0) tex_shift_polys(w, o);
                    b[o + 1] = DPC_DONE_MARK;
                }
                break;
            case 1:
                if (!done) { children(w, o, 0x18, b[o + 0x14], &top); b[o + 1] = DPC_DONE_MARK; }
                break;
            case 2:
                if (!done) {
                    int32_t cnt = (int32_t)rd32(b, o + 0x10);
                    for (int32_t i = 0; i < cnt && !w->err; i++) {
                        uint32_t e = fix(w, o, 0x14 + 4 * (size_t)i);
                        size_t eo = off_of(w, e, 0xc);
                        if (eo == (size_t)-1) return;
                        push(w, &top, fix(w, eo, 8));
                    }
                    b[o + 1] = DPC_DONE_MARK;
                }
                break;
            case 3:
                if (!done) {
                    children(w, o, 0x20, b[o + 0xb], &top);
                    if (rd32(b, o + 0x1c) != 0) fix(w, o, 0x1c);
                    b[o + 1] = DPC_DONE_MARK;
                }
                break;
            case 4:
                if (!done) { children(w, o, 0x14, b[o + 0x12], &top); b[o + 1] = DPC_DONE_MARK; }
                break;
            case 5:
                if (!done) { children(w, o, 0x38, b[o + 0x36], &top); b[o + 1] = DPC_DONE_MARK; }
                break;
            case 7:
                if (!done) { if (rd32(b, o + 0xc) != 0) fix(w, o, 0xc); b[o + 1] = DPC_DONE_MARK; }
                break;
            case 8:
                if (rd16(b, o + 2) == 500) wr16(b, o + 2, 0);     /* applied even on done nodes */
                if (!done) { if (rd32(b, o + 0xc) != 0) fix(w, o, 0xc); b[o + 1] = DPC_DONE_MARK; }
                break;
            case 9:
                if (!done) {
                    children(w, o, 0x10, b[o + 6], &top);
                    if (rd32(b, o + 0xc) != 0) fix(w, o, 0xc);
                    b[o + 1] = DPC_DONE_MARK;
                }
                break;
            case 0xb:
                if (!done) {
                    uint32_t a = fix(w, o, 8), c = fix(w, o, 0xc);
                    for (uint32_t i = 0; i < b[o + 6] && !w->err; i++) fix(w, o, 0x10 + 4 * (size_t)i);
                    push(w, &top, a);
                    push(w, &top, c);
                    b[o + 1] = DPC_DONE_MARK;
                }
                break;
            case 0xc:
                if (!done) { children(w, o, 8, rd16(b, o + 6), &top); b[o + 1] = DPC_DONE_MARK; }
                break;
            case 0xd:
                if (!done) { if (rd32(b, o + 8) != 0) fix(w, o, 8); b[o + 1] = DPC_DONE_MARK; }
                break;
            case 0xf:
                if (!done) {
                    fix(w, o, 4);
                    children(w, o, 0xc, b[o + 0xa], &top);
                    b[o + 1] = DPC_DONE_MARK;
                }
                break;
            default:
                break;   /* other types: untouched, not marked (as in the original) */
            }
        }
        n = top;
    }
}

int dpc_relocate(uint8_t* buf, size_t size, uint32_t addr_base, int16_t tex_shift) {
    DpcHeader h;
    if (dpc_read_header(buf, size, &h) != 0) return -1;
    Walk w = { buf, size, h.base, addr_base, tex_shift, malloc(256 * sizeof(uint32_t)), 256, 0 };
    if (!w.stack) return -3;
    size_t list = off_of(&w, dpc_reloc(h.root_list, h.base, addr_base), 4);
    if (list == (size_t)-1) { free(w.stack); return -2; }
    uint32_t count = rd32(buf, list);
    uint32_t i = 0;
    for (; i < count && !w.err; i++) {
        size_t p = list + 4 + 4 * (size_t)i;
        if (p + 4 > size) { w.err = -2; break; }
        uint32_t v = dpc_reloc(rd32(buf, p), h.base, addr_base);
        wr32(buf, p, v);
        if (v == 0) break;                 /* original returns on a null root */
        walk(&w, v);
    }
    free(w.stack);
    return w.err ? w.err : (int)i;
}

/* FUN_0049eee0 / FUN_0049f440 on their own (port code: the runtime binds them by address; the
 * walk is the one dpc_relocate runs per root, verified through it) */
int dpc_relocate_tree(uint8_t* buf, size_t size, uint32_t addr_base, uint32_t base, uint32_t root, int16_t tex_shift) {
    Walk w = { buf, size, base, addr_base, tex_shift, malloc(256 * sizeof(uint32_t)), 256, 0 };
    if (!w.stack) return -3;
    walk(&w, root);
    free(w.stack);
    return w.err;
}

int dpc_shift_tex_pages(uint8_t* buf, size_t size, uint32_t addr_base, uint32_t node, int16_t tex_shift) {
    Walk w = { buf, size, 0, addr_base, tex_shift, NULL, 0, 0 };
    size_t o = off_of(&w, node, 0x14);
    if (o == (size_t)-1) return w.err;
    tex_shift_polys(&w, o);
    return w.err;
}
