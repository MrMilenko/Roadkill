/*
 * en_misc2.c: small pure leaves, ported from TM2.EXE (see en_misc2.h).
 */
#include "en_misc2.h"

#include <string.h>

static uint32_t r32(EnMisc2Env* e, uint32_t a) { uint32_t v; memcpy(&v, e->mem(e->user, a), 4); return v; }
static void w32(EnMisc2Env* e, uint32_t a, uint32_t v) { memcpy(e->mem(e->user, a), &v, 4); }
static int ok(EnMisc2Env* e, uint32_t obj, int32_t i) { return i >= 0 && i < (int32_t)r32(e, obj + 0x24); }
static uint32_t ptr(EnMisc2Env* e, uint32_t obj, int32_t i) { return r32(e, obj + 4 + 4u * (uint32_t)i); }

uint32_t en_tab_entry_block(EnMisc2Env* e, uint32_t obj, int32_t i) { return ok(e, obj, i) ? ptr(e, obj, i) + 0x94 : 0; }
uint32_t en_tab_entry_key(EnMisc2Env* e, uint32_t obj, int32_t i) { return ok(e, obj, i) ? r32(e, ptr(e, obj, i)) : 0; }
uint32_t en_tab_find_114(EnMisc2Env* e, uint32_t obj, uint32_t key) {
    int32_t k = -1;
    for (int32_t i = 0; i < (int32_t)r32(e, obj + 0x24); i++)
        if (key == r32(e, ptr(e, obj, i))) k = i;
    return k >= 0 ? r32(e, ptr(e, obj, k) + 0x114) : 0xffffffffu;
}
uint32_t en_tab_get(EnMisc2Env* e, uint32_t obj, int32_t i, uint32_t ofs) {
    return ok(e, obj, i) ? r32(e, ptr(e, obj, i) + ofs) : 0xffffffffu;
}
uint32_t en_tab_set(EnMisc2Env* e, uint32_t obj, int32_t i, uint32_t ofs, uint32_t v) {
    if (!ok(e, obj, i)) return obj;
    uint32_t p = ptr(e, obj, i);
    w32(e, p + ofs, v);
    return p;
}
void en_tab_remove(EnMisc2Env* e, uint32_t obj, uint32_t key) {
    for (int32_t i = 0; i < (int32_t)r32(e, obj + 0x24); i++) {
        if (key != r32(e, ptr(e, obj, i))) continue;
        w32(e, obj + 0x24, r32(e, obj + 0x24) - 1);
        for (int32_t j = i + 1; j < 8; j++) {
            uint32_t src = ptr(e, obj, j), dst = ptr(e, obj, j - 1);
            /* rep movsd forward, 4 bytes at a time (overlap behaves as the original) */
            for (uint32_t k = 0; k < 0x138; k += 4) w32(e, dst + k, r32(e, src + k));
        }
    }
}
uint32_t en_list_find(EnMisc2Env* e, uint32_t head, uint32_t key) {
    uint32_t n = r32(e, head);
    while (n) {
        if (key == r32(e, n)) return n;
        n = r32(e, n + 0x134);
    }
    return 0;
}
uint32_t en_word_at(EnMisc2Env* e, uint32_t a) { return r32(e, a); }
uint32_t en_list_free_all(EnMisc2Env* e, uint32_t list) {
    uint32_t n = r32(e, list);
    w32(e, list + 8, n);
    while (n) {
        uint32_t next = r32(e, r32(e, list + 8) + 0x134);
        e->free_fn(e->user, r32(e, list + 8));
        w32(e, list + 8, next);
        n = next;
    }
    w32(e, list + 4, 0);
    w32(e, list + 0xc, 0);
    w32(e, list, 0);
    return list;
}
