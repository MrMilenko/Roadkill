/*
 * en_ot.c: ordering table, ported from TM2.EXE 0x00496680, 0x004967f4, 0x004963b4, 0x0049681c,
 * 0x00496380 (see en_ot.h). Accesses outside mem read 0 and drop writes.
 */
#include "en_ot.h"

#include <string.h>

static uint32_t rd(EnOtEnv* e, uint32_t a) {
    uint32_t o = a - e->base, v = 0;
    if ((size_t)o <= e->size && e->size - o >= 4) memcpy(&v, e->mem + o, 4);
    return v;
}
static void wr(EnOtEnv* e, uint32_t a, uint32_t v) {
    uint32_t o = a - e->base;
    if ((size_t)o <= e->size && e->size - o >= 4) memcpy(e->mem + o, &v, 4);
}

void en_ot_insert(EnOtEnv* e, uint32_t slot, uint32_t rec) {
    if (e->hw) {
        wr(e, rec, rd(e, slot));
        wr(e, slot, rec);
        return;
    }
    uint32_t d = e->desc;
    uint32_t tail = d + 0x4004 + (((slot - rd(e, d)) >> 2) << 2);   /* sar then shl: low two bits cleared */
    uint32_t first = rd(e, slot);
    uint32_t last = rd(e, tail);
    if (slot + 4 == first) wr(e, slot, rec);   /* empty bucket */
    wr(e, last, rec);
    wr(e, tail, rec);
    wr(e, rec, slot + 4);
}

void en_prim_queue(EnOtEnv* e, uint32_t prim) {
    if (e->hw) { e->hw_prim(e->user, prim); return; }
    e->lock(e->user);
    e->dispatch(e->user, prim);
    e->unlock(e->user);
}

void en_add_prim(EnOtEnv* e, uint32_t prim) {
    if (e->hw) { en_prim_queue(e, prim); return; }
    e->lock(e->user);
    e->dispatch(e->user, prim);
    e->unlock(e->user);
}

void en_ot_flush(EnOtEnv* e, uint32_t ot) {
    if (e->hw) { en_ot_flush_hw(e, ot); return; }
    e->lock(e->user);
    uint32_t link = ot, end = ot + 4;
    for (int i = 0; i < 0xfff; i++) {
        for (uint32_t r = rd(e, link); r != end; r = rd(e, r)) {
            e->dispatch(e->user, r);
            link = r;
        }
        link = end;
        end += 4;
    }
    e->unlock(e->user);
}

void en_ot_flush_hw(EnOtEnv* e, uint32_t ot) {
    if (e->hw) { e->hw_flush(e->user); return; }
    en_ot_flush(e, ot);
}
