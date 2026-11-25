/*
 * pf8_sesslist.c: TM2.EXE 0x00497e34, an unlisted function (not in Ghidra's function list; reached from
 * 0x004aa900): brings a list view in line with a linked list of entries (sessions / players).
 * Ported over Pf2Env (pf2_env.h); emulator diff: tools/emu/check_pf8_sesslist.sh. Calls get the four
 * registers as the original has them (leftovers included).
 *
 * eax = the owner (+0 the first entry; entries chained through +0x134), edx = the list view object,
 * ebx = show all columns. List view calls: 0x004a63d8 item count, 0x004a6390(lv, i) item data,
 * 0x004a6244(lv, i) delete, 0x004a64c4(lv, text, index, data) insert, 0x004a6540 (select / state),
 * 0x004a641c(lv, i, column, buffer; 0x100) get text, 0x004a6564(lv, text, i, column) set text;
 * 0x004a91b0(a, b) the string compare (non zero: different). Entry: +0x10 a flag (texts 0x004e2f5e /
 * 0x004e2f62 or 0x004e2f66 / 0x004e2f69 in column 3), +0x14 a second text, +0x94 the name, +0x118 an
 * index into the text table 0x004e8404.
 */
#include "pf2_env.h"
#include "pf8_sesslist.h"

#define R32 pf2_r32

typedef struct { Pf2Env* e; uint32_t a, d, b, c; } Regs;
static uint32_t call_n(Regs* r, uint32_t fn, int ns, uint32_t s0) {
    uint32_t v[5] = {r->a, r->d, r->b, r->c, s0};
    return r->a = r->e->call(r->e->user, fn, v, 4 + ns);
}
#define CALL(fn) call_n(&R, (fn), 0, 0)

/* compare column `col` of item i with text; different: set column `setcol` to `set` */
static void sync_text(Regs* rr, uint32_t lv, uint32_t i, uint32_t col, uint32_t buf, uint32_t text, uint32_t set,
                      uint32_t setcol) {
    Regs R = *rr;
    R.c = buf;
    R.b = col;
    R.d = i;
    R.a = lv;
    call_n(&R, 0x004a641cu, 1, 0x100);
    R.d = text;
    R.a = buf;
    if (CALL(0x004a91b0u)) {
        R.c = setcol;
        R.d = set;
        R.b = i;
        R.a = lv;
        CALL(0x004a6564u);
    }
    *rr = R;
}

uint32_t pf8_00497e34(Pf2Env* e, uint32_t a, uint32_t d, uint32_t b, uint32_t c) {
    Regs R = {e, a, d, b, c};
    uint32_t owner = a, lv = d, all = b, buf = e->scratch;
    R.a = lv;
    uint32_t cnt = CALL(0x004a63d8u);
    R.d = 0;
    if ((int32_t)cnt > 0) {
        int done = 0;
        uint32_t i = 0;
        do {
            R.d = i;
            R.a = lv;
            R.b = owner;
            uint32_t item = CALL(0x004a6390u);
            R.b = R32(e, R.b);
            R.c = 0;
            while (R.b && !R.c) {
                if (R.b == item) R.c = 1;
                R.b = R32(e, R.b + 0x134);
            }
            if (!R.c) {
                R.d = i;
                R.a = lv;
                CALL(0x004a6244u);
                R.a = lv;
                cnt = CALL(0x004a63d8u);
            } else {
                i++;
            }
            if (i == cnt) done = 1;
        } while (!done);
    }
    uint32_t p = R32(e, owner);
    uint32_t di = 0;
    while (p) {
        R.a = lv;
        uint32_t n2 = CALL(0x004a63d8u);
        R.c = 0;
        R.b = 0;
        for (; (int32_t)R.b < (int32_t)n2; R.b++) {
            R.d = R.b;
            R.a = lv;
            if (CALL(0x004a6390u) == p) { R.c = 1; di = R.b; }
        }
        if (!R.c) {
            di = n2;
            R.d = 0x004e2f5du;
            R.c = p;
            R.a = lv;
            R.b = di;
            CALL(0x004a64c4u);
            if (di == 0) {
                R.c = 2;
                R.d = di;
                R.a = lv;
                R.b = 2;
                CALL(0x004a6540u);
            }
        }
        if (R32(e, p + 0x10)) sync_text(&R, lv, di, 3, buf, 0x004e2f5eu, 0x004e2f62u, 3);
        else sync_text(&R, lv, di, 3, buf, 0x004e2f66u, 0x004e2f69u, 3);
        uint32_t name = p + 0x94;
        if (all) {
            sync_text(&R, lv, di, 0, buf, name, name, 0);
            sync_text(&R, lv, di, 1, buf, p + 0x14, p + 0x14, 1);
        } else {
            sync_text(&R, lv, di, 1, buf, name, name, 1);
        }
        uint32_t t = R32(e, 0x004e8404u + 4 * R32(e, p + 0x118));
        sync_text(&R, lv, di, 2, buf, t, R32(e, 0x004e8404u + 4 * R32(e, p + 0x118)), 2);
        p = R32(e, p + 0x134);
    }
    return R.a;
}
