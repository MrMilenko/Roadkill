/*
 * cb2_world_tables.c: worldWarpsSetup 0x00453fac and worldCollapseReset 0x00454ae0, with their
 * per level data read from the player's TM2.EXE (see cb2_world_tables.h).
 */
#include "cb2_world_tables.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../assets/cb2_exe_stores.h"

#define FN_WARPS_SETUP 0x00453facu
#define FN_COLLAPSE_RESET 0x00454ae0u

static void put(uint8_t* base, const Cb2Put* p) {
    for (int i = 0; i < p->size; i++) base[p->off + i] = (uint8_t)(p->value >> (8 * i));
}

static int add(Cb2Put* arr, int* n, int cap, const Cb2Store* s, uint32_t base, uint32_t size,
               char* err, size_t errlen) {
    if (s->va < base || s->va - base + s->size > size) {
        snprintf(err, errlen, "store at %08x writes %08x, outside the expected block", s->pc, s->va);
        return -1;
    }
    if (*n >= cap) { snprintf(err, errlen, "too many constant stores"); return -1; }
    arr[(*n)++] = (Cb2Put){ (uint16_t)(s->va - base), s->size, s->value };
    return 0;
}

int cb2_warps_table_extract(const PeImage* pe, Cb2WarpsTable* t, char* err, size_t errlen) {
    static Cb2StoreList sl;
    memset(t, 0, sizeof *t);
    for (int32_t ev = -1; ev <= 10; ev++) {
        uint32_t init[8] = { (uint32_t)ev };
        if (cb2_exe_run_stores(pe, FN_WARPS_SETUP, init, 1u << CB2_EAX, &sl, err, errlen)) return -1;
        int nrel = 0;
        Cb2PutList* L = ev >= 1 && ev <= 8 ? &t->level[ev - 1] : 0;
        for (int i = 0; i < sl.n; i++) {
            const Cb2Store* s = &sl.s[i];
            if (!s->absolute) {
                /* the clear loop: record r +0x00 byte, +0x04 word, +0x20 dword, all 0 */
                uint32_t r = (uint32_t)nrel / 3, k = (uint32_t)nrel % 3;
                uint32_t want = CB2_WARPS_VA + 0x28 * r + (k == 0 ? 0 : k == 1 ? 4 : 0x20);
                if (r > 2 || s->va != want || s->size != (k == 0 ? 1 : k == 1 ? 2 : 4) || s->value != 0) {
                    snprintf(err, errlen, "warps clear loop not as expected at %08x", s->pc);
                    return -1;
                }
                nrel++;
                continue;
            }
            if (nrel != 9) { snprintf(err, errlen, "warp constants before the clear loop ended"); return -1; }
            if (!L) { snprintf(err, errlen, "event %d has warp constants", ev); return -1; }
            if (add(L->put, &L->n, 64, s, CB2_WARPS_VA, CB2_WARPS_SIZE, err, errlen)) return -1;
        }
        if (nrel != 9) { snprintf(err, errlen, "warps clear loop not as expected"); return -1; }
    }
    return 0;
}

int cb2_collapse_table_extract(const PeImage* pe, Cb2CollapseTable* t, char* err, size_t errlen) {
    static Cb2StoreList sl;
    static const struct { uint8_t off, size; uint8_t value; } loop[14] = {
        { 0x10, 4, 1 }, { 0x1a, 1, 0 }, { 0x19, 1, 0 }, { 0x04, 4, 0 }, { 0x08, 4, 0 }, { 0x0c, 4, 0 },
        { 0x00, 4, 0 }, { 0x1c, 1, 0xff }, { 0x1d, 1, 0xff }, { 0x1e, 1, 0xff }, { 0x1f, 1, 0xff },
        { 0x20, 1, 0xff }, { 0x21, 1, 0xff }, { 0x18, 1, 0 },
    };
    uint32_t init[8] = { 0 };
    memset(t, 0, sizeof *t);
    if (cb2_exe_run_stores(pe, FN_COLLAPSE_RESET, init, 0, &sl, err, errlen)) return -1;
    int nrel = 0;
    for (int i = 0; i < sl.n; i++) {
        const Cb2Store* s = &sl.s[i];
        if (!s->absolute) {
            /* the section loop, checked store by store against the port's loop below */
            int r = nrel / 14, k = nrel % 14;
            if (r > 34 || s->va != CB2_COLLAPSE_VA + 0x24u * (uint32_t)r + loop[k].off || s->size != loop[k].size ||
                (s->value & (s->size == 1 ? 0xffu : 0xffffffffu)) != loop[k].value) {
                snprintf(err, errlen, "collapse section loop not as expected at %08x", s->pc);
                return -1;
            }
            nrel++;
            continue;
        }
        if (nrel == 0) {
            if (add(t->pre.put, &t->pre.n, 64, s, CB2_COLLAPSE_VA, CB2_COLLAPSE_SIZE, err, errlen)) return -1;
            /* the state must not overlap the table the loop clears */
            if (s->va < CB2_COLLAPSE_VA + 35 * 0x24) { snprintf(err, errlen, "state store inside the table"); return -1; }
        } else if (nrel == 35 * 14) {
            if (add(t->put, &t->n, 512, s, CB2_COLLAPSE_VA, CB2_COLLAPSE_SIZE, err, errlen)) return -1;
        } else {
            snprintf(err, errlen, "constant store inside the section loop at %08x", s->pc);
            return -1;
        }
    }
    if (nrel != 35 * 14) { snprintf(err, errlen, "collapse section loop not as expected"); return -1; }
    return 0;
}

int cb2_world_tables_load(const char* exe_path, Cb2WarpsTable* warps, Cb2CollapseTable* collapse,
                          char* err, size_t errlen) {
    FILE* f = fopen(exe_path, "rb");
    if (!f) { snprintf(err, errlen, "cannot open %s", exe_path); return -1; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* d = n > 0 ? malloc((size_t)n) : 0;
    if (!d || fread(d, 1, (size_t)n, f) != (size_t)n) {
        fclose(f); free(d);
        snprintf(err, errlen, "cannot read %s", exe_path);
        return -1;
    }
    fclose(f);
    PeImage pe;
    int rc = -1;
    if (pe_open(&pe, d, (size_t)n)) snprintf(err, errlen, "%s is not a PE executable", exe_path);
    else if (!cb2_warps_table_extract(&pe, warps, err, errlen) &&
             !cb2_collapse_table_extract(&pe, collapse, err, errlen)) rc = 0;
    free(d);
    return rc;
}

void cb2_world_warps_setup(uint8_t* records, int32_t event, const Cb2WarpsTable* t) {
    for (int r = 0; r < 3; r++) {
        uint8_t* w = records + 0x28 * r;
        w[0x00] = 0;
        w[0x04] = 0; w[0x05] = 0;
        memset(w + 0x20, 0, 4);
    }
    uint32_t c = (uint32_t)event - 1u;       /* dec edx; cmp edx, 7; ja default */
    if (c > 7) return;
    const Cb2PutList* L = &t->level[c];
    for (int i = 0; i < L->n; i++) put(records, &L->put[i]);
}

void cb2_world_collapse_reset(uint8_t* block, const Cb2CollapseTable* t) {
    for (int i = 0; i < t->pre.n; i++) put(block, &t->pre.put[i]);
    for (int s = 0; s < 35; s++) {
        uint8_t* r = block + 0x24 * s;
        memset(r + 0x00, 0, 0x10);
        r[0x10] = 1; r[0x11] = 0; r[0x12] = 0; r[0x13] = 0;
        r[0x18] = 0; r[0x19] = 0; r[0x1a] = 0;
        memset(r + 0x1c, 0xff, 6);
    }
    for (int i = 0; i < t->n; i++) put(block, &t->put[i]);
}
