/*
 * exe_carstats.c: the per car stat tables, read from the player's own TM2.EXE (see exe_carstats.h).
 */
#include "exe_carstats.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FN_CAR_STATS 0x0043846cu
#define MAX_ID 0x200
#define MAX_STEPS 4096

/* ---- a small x86 (32 bit) decoder: lengths for the opcodes Watcom uses here, plus the few
 *      meanings the walk needs. Anything else returns 0. ---------------------------------- */

typedef struct {
    int len;
    int op;             /* primary opcode, 0x100 | second byte after 0x0F */
    int o16;            /* 0x66 prefix */
    int has_modrm, mod, reg, rm, sib;
    int32_t disp;
    int imm_size;
    uint32_t imm;
    int32_t rel;        /* branch displacement */
} X86;

static int modrm(const uint8_t* p, int avail, X86* x, int at) {
    if (at >= avail) return -1;
    uint8_t b = p[at++];
    x->has_modrm = 1; x->mod = b >> 6; x->reg = (b >> 3) & 7; x->rm = b & 7; x->sib = 0; x->disp = 0;
    if (x->mod == 3) return at;
    int disp = 0;
    if (x->rm == 4) {
        if (at >= avail) return -1;
        uint8_t s = p[at++];
        x->sib = 1;
        if (x->mod == 0 && (s & 7) == 5) disp = 4;
    } else if (x->mod == 0 && x->rm == 5) disp = 4;
    if (x->mod == 1) disp = 1;
    if (x->mod == 2) disp = 4;
    if (at + disp > avail) return -1;
    if (disp == 1) x->disp = (int8_t)p[at];
    if (disp == 4) x->disp = (int32_t)((uint32_t)p[at] | (uint32_t)p[at + 1] << 8 | (uint32_t)p[at + 2] << 16 | (uint32_t)p[at + 3] << 24);
    return at + disp;
}

static int imm(const uint8_t* p, int avail, X86* x, int at, int size) {
    if (at + size > avail) return -1;
    x->imm_size = size; x->imm = 0;
    for (int i = 0; i < size; i++) x->imm |= (uint32_t)p[at + i] << (8 * i);
    return at + size;
}

static int decode(const uint8_t* p, int avail, X86* x) {
    memset(x, 0, sizeof *x);
    int at = 0;
    if (avail > 0 && p[0] == 0x66) { x->o16 = 1; at = 1; }
    if (at >= avail) return 0;
    int op = p[at++], isz = x->o16 ? 2 : 4;
    if (op == 0x0f) {
        if (at >= avail) return 0;
        op = 0x100 | p[at++];
    }
    x->op = op;
    if (op < 0x40 && (op & 7) < 4 && op != 0x0f) at = modrm(p, avail, x, at);           /* ALU r/m */
    else if (op < 0x40 && (op & 7) == 4) at = imm(p, avail, x, at, 1);
    else if (op < 0x40 && (op & 7) == 5) at = imm(p, avail, x, at, isz);
    else if (op >= 0x40 && op <= 0x5f) { }                                               /* inc/dec/push/pop */
    else if (op == 0x69) { at = modrm(p, avail, x, at); if (at > 0) at = imm(p, avail, x, at, isz); }
    else if (op == 0x6b || op == 0x80 || op == 0x83 || op == 0xc0 || op == 0xc1 || op == 0xc6) {
        at = modrm(p, avail, x, at); if (at > 0) at = imm(p, avail, x, at, 1);
    }
    else if (op >= 0x70 && op <= 0x7f) { if (at >= avail) return 0; x->rel = (int8_t)p[at++]; }
    else if (op == 0xeb) { if (at >= avail) return 0; x->rel = (int8_t)p[at++]; }
    else if (op == 0x81 || op == 0xc7) { at = modrm(p, avail, x, at); if (at > 0) at = imm(p, avail, x, at, isz); }
    else if ((op >= 0x84 && op <= 0x8b) || op == 0x8d || op == 0xd1 || op == 0xd3 || op == 0xff) at = modrm(p, avail, x, at);
    else if (op >= 0x90 && op <= 0x99) { }                                               /* nop/xchg/cwde/cdq */
    else if (op >= 0xb0 && op <= 0xb7) at = imm(p, avail, x, at, 1);
    else if (op >= 0xb8 && op <= 0xbf) at = imm(p, avail, x, at, isz);
    else if (op == 0xc3) { }
    else if (op == 0xe8 || op == 0xe9) {
        if (x->o16 || at + 4 > avail) return 0;
        x->rel = (int32_t)((uint32_t)p[at] | (uint32_t)p[at + 1] << 8 | (uint32_t)p[at + 2] << 16 | (uint32_t)p[at + 3] << 24);
        at += 4;
    }
    else if (op == 0xf6 || op == 0xf7) {
        at = modrm(p, avail, x, at);
        if (at > 0 && x->reg <= 1) at = imm(p, avail, x, at, op == 0xf6 ? 1 : isz);      /* test */
    }
    else if (op >= 0x180 && op <= 0x18f) {                                               /* jcc rel32 */
        if (at + 4 > avail) return 0;
        x->rel = (int32_t)((uint32_t)p[at] | (uint32_t)p[at + 1] << 8 | (uint32_t)p[at + 2] << 16 | (uint32_t)p[at + 3] << 24);
        at += 4;
    }
    else if (op == 0x1af || op == 0x1b6 || op == 0x1b7 || op == 0x1be || op == 0x1bf || (op >= 0x190 && op <= 0x19f))
        at = modrm(p, avail, x, at);
    else return 0;
    if (at <= 0) return 0;
    x->len = at;
    return at;
}

/* does the instruction change the flags (so a later jcc no longer tests the car id)? */
static int sets_flags(const X86* x) {
    int op = x->op;
    if (op < 0x40) return 1;                                                  /* ALU */
    if (op >= 0x40 && op <= 0x4f) return 1;                                   /* inc/dec */
    if (op == 0x69 || op == 0x6b || op == 0x1af) return 1;
    if (op == 0x80 || op == 0x81 || op == 0x83 || op == 0x84 || op == 0x85) return 1;
    if (op == 0xc0 || op == 0xc1 || op == 0xd1 || op == 0xd3) return 1;
    if (op == 0xf6 || op == 0xf7 || op == 0xff) return 1;
    if (op == 0xe8) return 1;                                                 /* a call may */
    return 0;
}

/* does the instruction write ebx / bx / bl / bh (the car id, Watcom's third argument)? Calls keep it:
 * Watcom callees save every register but eax. */
static int writes_ebx(const X86* x) {
    int op = x->op;
    int rm_dst = x->has_modrm && x->mod == 3 && (x->rm == 3 || (x->rm == 7 && (op == 0x88 || op == 0x8a || op == 0xc6 || op == 0x80 || op == 0xc0 || op == 0xf6)));
    int reg_dst = x->has_modrm && (x->reg == 3 || (x->reg == 7 && (op == 0x8a || op == 0x02 || op == 0x0a || op == 0x12 || op == 0x1a || op == 0x22 || op == 0x2a || op == 0x32)));
    if (op < 0x40 && (op & 7) < 4) {                 /* ALU: bit 1 = destination is reg */
        if ((op & 0x38) == 0x38) return 0;          /* cmp */
        return (op & 2) ? reg_dst : rm_dst;
    }
    if (op == 0x43 || op == 0x4b || op == 0x5b || op == 0x93 || op == 0xb3 || op == 0xb7 || op == 0xbb) return 1;
    if (op == 0x88 || op == 0x89 || op == 0xc6 || op == 0xc7) return rm_dst;
    if (op == 0x8a || op == 0x8b || op == 0x8d || op == 0x69 || op == 0x6b || op == 0x1af || (op >= 0x1b6 && op <= 0x1bf)) return reg_dst;
    if (op == 0x80 || op == 0x81 || op == 0x83) return x->reg != 7 && rm_dst;       /* not cmp */
    if (op == 0xc0 || op == 0xc1 || op == 0xd1 || op == 0xd3) return rm_dst;
    if (op == 0xf6 || op == 0xf7) return (x->reg == 2 || x->reg == 3) && rm_dst;    /* not / neg */
    if (op == 0xff) return x->reg <= 1 && rm_dst;                                   /* inc / dec */
    if (op >= 0x190 && op <= 0x19f) return rm_dst;                                  /* setcc */
    return 0;
}

/* ---- the walk ------------------------------------------------------------------------------- */

typedef struct { uint32_t addr; uint8_t off, val; } Write;

typedef struct {
    uint32_t path[MAX_STEPS];
    int npath;
    Write w[1024];
    int nw;
} Walk;

/* cc: the low nibble of a jcc opcode */
static int taken(int cc, uint16_t bx, uint16_t v) {
    int cf = bx < v, zf = bx == v, lt = (int16_t)bx < (int16_t)v;
    switch (cc) {
    case 0x2: return cf;            case 0x3: return !cf;
    case 0x4: return zf;            case 0x5: return !zf;
    case 0x6: return cf || zf;      case 0x7: return !cf && !zf;
    case 0xc: return lt;            case 0xd: return !lt;
    case 0xe: return lt || zf;      case 0xf: return !lt && !zf;
    default: return -1;
    }
}

static int walk(const PeImage* pe, uint16_t id, Walk* wk, char* err, size_t errlen) {
    uint32_t a = FN_CAR_STATS;
    int bx_is_id = 1;
    int have_cmp = 0;
    uint16_t cmpv = 0;
    wk->npath = 0; wk->nw = 0;
    for (;;) {
        if (wk->npath >= MAX_STEPS) { snprintf(err, errlen, "walk too long at %08x", a); return -1; }
        for (int i = 0; i < wk->npath; i++)
            if (wk->path[i] == a) { snprintf(err, errlen, "loop at %08x", a); return -1; }
        const uint8_t* p = pe_at(pe, a, 1);
        if (!p) { snprintf(err, errlen, "address %08x outside the image", a); return -1; }
        int avail = 16;
        while (avail > 1 && !pe_at(pe, a, (size_t)avail)) avail--;
        X86 x;
        if (!decode(p, avail, &x)) { snprintf(err, errlen, "unknown instruction at %08x (%02x %02x %02x)", a, p[0], avail > 1 ? p[1] : 0, avail > 2 ? p[2] : 0); return -1; }
        wk->path[wk->npath++] = a;
        uint32_t next = a + (uint32_t)x.len;

        /* cmp bx, imm (66 81 /7 or 66 83 /7 with rm = bx) */
        if (x.o16 && (x.op == 0x81 || x.op == 0x83) && x.mod == 3 && x.reg == 7 && x.rm == 3) {
            cmpv = x.op == 0x83 ? (uint16_t)(int16_t)(int8_t)x.imm : (uint16_t)x.imm;
            have_cmp = 1;
            a = next;
            continue;
        }
        /* constant store into the stats block: mov [ecx|edx + disp], imm */
        if ((x.op == 0xc6 || x.op == 0xc7) && x.reg == 0 && x.mod != 3 && !x.sib && (x.rm == 1 || x.rm == 2)) {
            int size = x.op == 0xc6 ? 1 : (x.o16 ? 2 : 4);
            if (x.disp < 0 || x.disp + size > 0x100) { snprintf(err, errlen, "store outside the stats block at %08x", a); return -1; }
            for (int i = 0; i < size; i++) {
                if (wk->nw >= (int)(sizeof wk->w / sizeof wk->w[0])) { snprintf(err, errlen, "too many stores"); return -1; }
                wk->w[wk->nw++] = (Write){ a, (uint8_t)(x.disp + i), (uint8_t)(x.imm >> (8 * i)) };
            }
        }
        /* the car id itself stored into the block: mov [ecx|edx + disp], bx (or bl) */
        /* (once ebx has been reused as a scratch register its stores are computed values: skipped
         *  like any other register store) */
        if ((x.op == 0x89 || x.op == 0x88) && x.mod != 3 && !x.sib && (x.rm == 1 || x.rm == 2) && x.reg == 3 && bx_is_id) {
            int size = x.op == 0x88 ? 1 : (x.o16 ? 2 : 4);
            if (size == 4) { snprintf(err, errlen, "32 bit store of the car id at %08x not understood", a); return -1; }
            if (x.disp < 0 || x.disp + size > 0x100) { snprintf(err, errlen, "store outside the stats block at %08x", a); return -1; }
            for (int i = 0; i < size; i++) {
                if (wk->nw >= (int)(sizeof wk->w / sizeof wk->w[0])) { snprintf(err, errlen, "too many stores"); return -1; }
                wk->w[wk->nw++] = (Write){ a, (uint8_t)(x.disp + i), (uint8_t)(id >> (8 * i)) };
            }
        }
        if (writes_ebx(&x)) bx_is_id = 0;
        if (x.op == 0xc3) return 0;
        if (x.op == 0xeb || x.op == 0xe9) { a = next + (uint32_t)x.rel; continue; }
        if ((x.op >= 0x70 && x.op <= 0x7f) || (x.op >= 0x180 && x.op <= 0x18f)) {
            if (have_cmp && !bx_is_id) { snprintf(err, errlen, "car id overwritten before the compare at %08x", a); return -1; }
            if (have_cmp) {
                int t = taken(x.op & 0xf, id, cmpv);
                if (t < 0) { snprintf(err, errlen, "unhandled condition at %08x", a); return -1; }
                a = t ? next + (uint32_t)x.rel : next;
            } else a = next;   /* not about the car id: both ids take the same (fall through) way */
            continue;
        }
        if (sets_flags(&x)) have_cmp = 0;
        a = next;
    }
}

static int cmp_u32(const void* a, const void* b) {
    uint32_t x = *(const uint32_t*)a, y = *(const uint32_t*)b;
    return x < y ? -1 : x > y;
}

static int on_path(const uint32_t* sorted, int n, uint32_t a) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        if (sorted[m] == a) return 1;
        if (sorted[m] < a) lo = m + 1; else hi = m - 1;
    }
    return 0;
}

int car_stats_extract(const PeImage* pe, CarStatsEntry** out, int* count, char* err, size_t errlen) {
    static const uint8_t prologue[] = { 0x51, 0x56, 0x89, 0xc6, 0x89, 0xd1 };   /* push ecx; push esi; mov esi, eax; mov ecx, edx */
    const uint8_t* f = pe_at(pe, FN_CAR_STATS, sizeof prologue);
    if (!f || memcmp(f, prologue, sizeof prologue)) {
        snprintf(err, errlen, "TM2.EXE: the car stats setup is not at %08x (another build?)", FN_CAR_STATS);
        return -1;
    }
    Walk* wk = malloc(sizeof *wk * MAX_ID);
    if (!wk) { snprintf(err, errlen, "out of memory"); return -1; }
    for (int id = 0; id < MAX_ID; id++)
        if (walk(pe, (uint16_t)id, &wk[id], err, errlen)) { free(wk); return -1; }

    /* instructions every id runs are the shared part; a store only some ids run is per car */
    uint32_t* common = malloc(sizeof(uint32_t) * MAX_STEPS);
    int ncommon = 0;
    for (int i = 0; i < wk[0].npath; i++) {
        int all = 1;
        for (int id = 1; id < MAX_ID && all; id++) {
            int hit = 0;
            for (int k = 0; k < wk[id].npath && !hit; k++) hit = wk[id].path[k] == wk[0].path[i];
            all = hit;
        }
        if (all) common[ncommon++] = wk[0].path[i];
    }
    qsort(common, (size_t)ncommon, sizeof *common, cmp_u32);

    CarStatsEntry* e = calloc(MAX_ID, sizeof *e);
    int n = 0;
    for (int id = 0; id < MAX_ID; id++) {
        int priv = 0;
        for (int i = 0; i < wk[id].nw; i++) priv += !on_path(common, ncommon, wk[id].w[i].addr);
        if (!priv) continue;
        uint8_t* b = malloc((size_t)priv * 2);
        int k = 0;
        for (int i = 0; i < wk[id].nw; i++)
            if (!on_path(common, ncommon, wk[id].w[i].addr)) { b[k++] = wk[id].w[i].off; b[k++] = wk[id].w[i].val; }
        e[n++] = (CarStatsEntry){ (uint16_t)id, b, priv };
    }
    free(common);
    free(wk);
    if (n == 0) { free(e); snprintf(err, errlen, "TM2.EXE: no per car tables found"); return -1; }
    *out = e; *count = n;
    return 0;
}

int tm2_car_stats_load(const char* exe_path) {
    FILE* fp = fopen(exe_path, "rb");
    if (!fp) return -1;
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    uint8_t* d = n > 0 ? malloc((size_t)n) : 0;
    if (!d || fread(d, 1, (size_t)n, fp) != (size_t)n) { fclose(fp); free(d); return -1; }
    fclose(fp);
    PeImage pe;
    CarStatsEntry* e;
    int count;
    char err[160];
    int r = pe_open(&pe, d, (size_t)n) ? -2 : car_stats_extract(&pe, &e, &count, err, sizeof err);
    if (r == 0) car_stats_set_table(e, count);
    else if (r == -1) fprintf(stderr, "%s\n", err);
    free(d);
    return r;
}
