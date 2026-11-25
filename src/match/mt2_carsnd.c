/*
 * mt2_carsnd.c: carSoundUpdate, the car pair vector table and the AI cars' respawn (see
 * mt2_carsnd.h).
 */
#include "mt2_carsnd.h"

#include <stdio.h>
#include <string.h>

#include "../assets/pe.h"

static int32_t rd32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static int16_t rd16(const uint8_t* p) { int16_t v; memcpy(&v, p, 2); return v; }
static void wr32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }
static void wr16(uint8_t* p, int16_t v) { memcpy(p, &v, 2); }

/* ---- 0x0045c1c4 ---- */

uint32_t mt2_car_pair_offset(int32_t car, int32_t other) {
    return (uint32_t)car * 0xb4u + (uint32_t)other * 0xcu;
}

uint8_t* mt2_car_pair_vec(uint8_t* table_50d0d8, int32_t car, int32_t other) {
    return table_50d0d8 + (int32_t)mt2_car_pair_offset(car, other);
}

/* ---- 0x0045b5f0 carSoundUpdate ---- */

/* the engine sound of a car type (a switch in the original) */
static int32_t engine_sound(int16_t type) {
    switch (type) {
    case 0x10: return 0x3c;
    case 0x20: return 0x24;
    case 0x30: return 0x26;
    case 0x40: return 0x28;
    case 0x50: return 0x2a;
    case 0x60: return 0x2c;
    case 0x70: return 0x2e;
    case 0x80: return 0x30;
    case 0x90: return 0x32;
    case 0xa0: return 0x34;
    case 0xb0: return 0x36;
    case 0xc0: return 0x38;
    case 0xd0: return 0x3a;
    case 0xe0: return 0x41;
    default: return -1;
    }
}

void mt2_car_sound_update(const Mt2CarSnd* e, uint8_t* car, uint8_t second) {
    const MtAudio* a = e->audio;
    uint8_t *fl, *eng, *stats, *horn;
    int16_t type;
    int32_t id;
    if (second) {
        fl = car + 0x9a; eng = car + 0x6c4; stats = car + 0x138; horn = car + 0x21c;
        type = rd16(car + 0x90); id = rd16(car);
    } else {
        fl = car + 0x68; eng = car + 0x708; stats = car + 0x4ec; horn = car + 0xcc;
        type = rd16(car + 0x30); id = rd32(car + 2) >> 16;
    }
    /* 0x0046cc54 then 0x0046f974 (music_call(obj, 0)) or 0x0046f980 (music_call(obj, -1)) */
    if (*e->flag_520e90 == 0) e->music_call(e->user, eng + 0x20, 0);
    else e->music_call(e->user, eng + 0x20, 0xffffffffu);
    uint32_t pan = 0, vol = 0;
    if (*e->player_mode != 4) {
        pan = mt_get_c8500c(a);
        vol = mt_get_c85058(a);
    }
    if (fl[2] && !fl[0x22] && !fl[0x21] && !fl[0xb]) mt_sound_play(a, 1, pan, vol, 99);
    if (fl[0x13]) {
        mt_sound_play(a, 7, pan, vol, 99);
        fl[0x13] = 0;
    }
    if (second) {
        if (fl[0xe]) mt_sound_play_variant(a, 0x15, pan, vol, 99, (uint32_t)id);
        else mt_sound_stop_instance(a, 0x15, (uint32_t)id);
        if (id == 0) {
            if ((fl[0x1a] || fl[0x22]) && !fl[0xe]) mt_sound_play(a, 0x20, pan, vol, 99);
            else mt_sound_stop(a, 0x20);
        }
    }
    /* volume: |speed| * 100 / top speed (idiv), through 0x0046f830 (min 99; its other four
     * arguments, stats bytes and the engine speed, are ignored) */
    int32_t sp = rd32(eng + 8) >> 16;
    if (sp < 0) sp = -sp;
    int32_t level = mt_clamp_99((sp * 100) / rd32(stats + 0xb4));
    int32_t snd = engine_sound(type);
    if (snd != -1) {
        if (fl[0] == 0) mt_sound_play(a, (uint32_t)snd, pan, vol, (uint32_t)level);
        else mt_sound_stop(a, (uint32_t)snd);
    }
    if (rd16(horn + 0xe) != 0 && fl[0] == 0) mt_sound_play(a, 3, pan, vol, (uint32_t)(rd32(horn + 0xc) >> 16));
}

/* ---- 0x0045c56c ai_cars_respawn ---- */

#define CARS 0x0050760cu
#define CAR_SIZE 0x880u

void mt2_ai_cars_respawn(const Mt2Respawn* e) {
    if (++*e->tries > 4) {
        e->schedule_event(e->user, 1, 0, 0);
        *e->tries = 0;
        *e->flag_502f9c = 0;
        return;
    }
    e->freeze_reset_all(e->user, 0, 0, 0);
    e->debris_clear(e->user, 0, 0, 0);
    e->start_nodes_clear(e->user, 0, 0, 0);
    for (int32_t i = 0; i < *e->car_count; i++) {
        uint8_t* car = e->cars + i * CAR_SIZE;
        e->ai_car_start(e->user, e->car_addr(e->user, car), (uint32_t)(int32_t)rd16(car + 0x30), 1);
        wr32(e->resolve(e->user, e->car_objects[i]) + 8, 1);
        car[0x83e] = 100;
        uint32_t r = e->rand(e->user, 0, 0, 0);
        wr32(car + 0x58c, (int32_t)((uint32_t)rd32(car + 0x58c) - (r & 0x3f)));
    }
    int32_t s = (int32_t)e->rand(e->user, 0, 0, 0) % 4;
    if (e->used[s]) {
        /* the next free set after s (wrapping), at most 4 steps; s may be negative (signed
         * remainder), then it counts up through the bytes before the table */
        int32_t k = 0;
        do {
            if (++s > 3) s = 0;
        } while (e->used[s] && ++k < 4);
    }
    *e->v502fb4 = 3;
    *e->v502fb8 = 3;
    *e->v502fbc = 1;
    e->used[s] = 1;
    *e->v502fc0 = 1;
    *e->start_flag = 0;
    uint8_t ev = (uint8_t)(e->event_type(e->user, 0, 0, 0) - 1);
    if (ev <= 7) {
        /* start sets outside 0..3 (negative s) take the default case: set 0 */
        const Mt2RespawnSet* t = &e->table->set[ev][(uint32_t)s > 3 ? 0 : s];
        for (int k = 0; k < 4; k++) {
            wr16(e->cars + k * CAR_SIZE + 0x52c, t->node52c[k]);
            wr32(e->cars + k * CAR_SIZE + 0x5bc, t->node5bc[k]);
        }
        if (e->rand(e->user, 0, 0, 0) & 1) {
            for (int k = 0; k < 3; k++) e->start[k] = t->start[k];
            *e->start_flag = t->start_flag;
        }
    }
    for (int32_t i = 0; i < *e->car_count; i++)
        e->ai_place(e->user, e->car_addr(e->user, e->cars + i * CAR_SIZE), 0, 0);
}

/* ---- the table loader: a register machine over the original's case blocks ---- */

enum { R_EAX, R_ECX, R_EDX, R_EBX, R_ESP, R_EBP, R_ESI, R_EDI };

typedef struct {
    uint32_t r[8];
    unsigned known;
} Regs;

#define END_VA 0x0045c740u    /* the common tail after the switch */
#define RAND_VA 0x004899c2u
#define OUTER_VA 0x0045c6a0u  /* and eax, 0xff; jmp cs:[eax * 4 + 0x0045c54c] */

static int fail(char* err, size_t n, const char* msg, uint32_t va) {
    if (err && n) snprintf(err, n, "%s at 0x%08x", msg, va);
    return -1;
}

/* store into the set; phase 0: before rand (car fields), 1: after (start point) */
static int store(Mt2RespawnSet* s, unsigned* seen, int phase, uint32_t va, int size, uint32_t v) {
    if (phase == 0) {
        uint32_t off = va - CARS;
        if (va < CARS || off >= 4 * CAR_SIZE) return -1;
        unsigned k = off / CAR_SIZE, f = off % CAR_SIZE;
        if (f == 0x52c && size == 2) { s->node52c[k] = (int16_t)v; *seen |= 1u << k; return 0; }
        if (f == 0x5bc && size == 4) { s->node5bc[k] = (int32_t)v; *seen |= 0x10u << k; return 0; }
        return -1;
    }
    if (size != 4) return -1;
    if (va >= 0x005030bcu && va <= 0x005030c4u && (va & 3) == 0) {
        s->start[(va - 0x005030bcu) / 4] = (int32_t)v;
        *seen |= 0x100u << ((va - 0x005030bcu) / 4);
        return 0;
    }
    if (va == 0x00502fd8u) { s->start_flag = (int32_t)v; *seen |= 0x800u; return 0; }
    return -1;
}

static int run_case(const PeImage* pe, Regs g, Mt2RespawnSet* s, char* err, size_t n) {
    uint32_t pc = OUTER_VA;
    int phase = 0, tested = 0, steps = 0;
    unsigned seen = 0;
    while (pc != END_VA) {
        if (++steps > 200) return fail(err, n, "too many steps", pc);
        const uint8_t* p = pe_at(pe, pc, 16);
        if (!p) return fail(err, n, "outside the image", pc);
        int o16 = 0;
        if (p[0] == 0x66) { o16 = 1; p++; }
        uint8_t op = p[0];
        uint32_t len = (uint32_t)o16;
        if (op >= 0xb8 && op <= 0xbf && !o16) {                      /* mov r32, imm32 */
            memcpy(&g.r[op - 0xb8], p + 1, 4); g.known |= 1u << (op - 0xb8); len += 5;
        } else if ((op == 0x89 || op == 0x8b || op == 0x31 || op == 0x33 || op == 0x39 || op == 0x3b) &&
                   (p[1] & 0xc0) == 0xc0 && !o16) {                   /* reg, reg */
            int rm = p[1] & 7, rg = (p[1] >> 3) & 7;
            int dst = (op & 2) ? rg : rm, src = (op & 2) ? rm : rg;
            if (op == 0x31 || op == 0x33) {
                if (dst != src) return fail(err, n, "xor of two registers", pc);
                g.r[dst] = 0; g.known |= 1u << dst;
            } else if (op == 0x89 || op == 0x8b) {
                if (!(g.known >> src & 1)) return fail(err, n, "unknown register", pc);
                g.r[dst] = g.r[src]; g.known |= 1u << dst;
            } else {                                                   /* cmp dst, src then ja */
                if (!(g.known >> src & 1) || !(g.known >> dst & 1)) return fail(err, n, "unknown register", pc);
                const uint8_t* q = p + 2;
                uint32_t tgt, l2;
                if (q[0] == 0x77) { tgt = pc + 2 + 2 + (uint32_t)(int8_t)q[1]; l2 = 2; }
                else if (q[0] == 0x0f && q[1] == 0x87) { int32_t d; memcpy(&d, q + 2, 4); tgt = pc + 2 + 6 + (uint32_t)d; l2 = 6; }
                else return fail(err, n, "cmp not followed by ja", pc);
                pc = g.r[dst] > g.r[src] ? tgt : pc + 2 + l2;
                continue;
            }
            len += 2;
        } else if (op == 0x89 && (p[1] & 0xc7) == 0x05) {             /* mov [disp32], r */
            int rg = (p[1] >> 3) & 7;
            uint32_t va;
            memcpy(&va, p + 2, 4);
            if (!(g.known >> rg & 1)) return fail(err, n, "store of an unknown register", pc);
            if (store(s, &seen, phase, va, o16 ? 2 : 4, g.r[rg]) != 0) return fail(err, n, "unexpected store", pc);
            len += 6;
        } else if (op == 0xa3) {                                       /* mov [moffs32], eax / ax */
            uint32_t va;
            memcpy(&va, p + 1, 4);
            if (!(g.known & 1u)) return fail(err, n, "store of an unknown register", pc);
            if (store(s, &seen, phase, va, o16 ? 2 : 4, g.r[R_EAX]) != 0) return fail(err, n, "unexpected store", pc);
            len += 5;
        } else if (op == 0x25 && !o16) {                              /* and eax, imm32 */
            uint32_t v; memcpy(&v, p + 1, 4); g.r[R_EAX] &= v; len += 5;
        } else if (op == 0x2e && p[1] == 0xff && p[2] == 0x24 && (p[3] & 0xc7) == 0x85) { /* jmp cs:[r*4+d] */
            int rg = (p[3] >> 3) & 7;
            uint32_t tbl;
            memcpy(&tbl, p + 4, 4);
            if (!(g.known >> rg & 1) || g.r[rg] > 7) return fail(err, n, "jump table index", pc);
            const uint8_t* t = pe_at(pe, tbl + 4 * g.r[rg], 4);
            if (!t) return fail(err, n, "jump table outside the image", pc);
            memcpy(&pc, t, 4);
            continue;
        } else if (op == 0xe8 && !o16) {                               /* call rand */
            int32_t d; memcpy(&d, p + 1, 4);
            if (pc + 5 + (uint32_t)d != RAND_VA || phase) return fail(err, n, "unexpected call", pc);
            phase = 1; g.known &= ~1u; len += 5;
        } else if (op == 0xa8 && p[1] == 1 && phase == 1 && !tested) { /* test al, 1 */
            tested = 1; len += 2;
        } else if (op == 0x0f && p[1] == 0x84 && tested == 1) {        /* je END (rand even): follow odd */
            int32_t d; memcpy(&d, p + 2, 4);
            if (pc + 6 + (uint32_t)d != END_VA) return fail(err, n, "je elsewhere", pc);
            tested = 2; len += 6;
        } else if (op == 0x74 && tested == 1) {
            if (pc + 2 + (uint32_t)(int8_t)p[1] != END_VA) return fail(err, n, "je elsewhere", pc);
            tested = 2; len += 2;
        } else if (op == 0xe9 || op == 0xeb) {                         /* jmp */
            int32_t d;
            if (op == 0xe9) { memcpy(&d, p + 1, 4); pc += 5 + (uint32_t)d; }
            else { d = (int8_t)p[1]; pc += 2 + (uint32_t)d; }
            continue;
        } else {
            return fail(err, n, "unknown instruction", pc);
        }
        pc += len;
    }
    if (seen != 0xfffu || tested != 2) return fail(err, n, "incomplete case", pc);
    return 0;
}

int mt2_respawn_table_load(Mt2RespawnTable* t, const uint8_t* exe, size_t size, char* err, size_t errlen) {
    PeImage pe;
    if (pe_open(&pe, exe, size) != 0) return fail(err, errlen, "not a PE image", 0);
    memset(t, 0, sizeof *t);
    for (uint32_t ev = 0; ev < 8; ev++)
        for (uint32_t s = 0; s < 4; s++) {
            /* registers at 0x0045c6a0: eax = event - 1, ecx = 0, edx = set, ebp = 1, esi = 3 */
            Regs g = {{0}, 0};
            g.r[R_EAX] = ev; g.r[R_ECX] = 0; g.r[R_EDX] = s; g.r[R_EBP] = 1; g.r[R_ESI] = 3;
            g.known = 1u << R_EAX | 1u << R_ECX | 1u << R_EDX | 1u << R_EBP | 1u << R_ESI;
            if (run_case(&pe, g, &t->set[ev][s], err, errlen) != 0) return -1;
        }
    return 0;
}
