/*
 * gp3_wlo.c: worldLoadObject, TM2.EXE 0x0044f20c (see gp3_wlo.h).
 *
 * The original is a compare tree of about 220 unsigned tests; read as a table, the types
 * 0x10..0xff form 15 blocks of 16 with the same pattern by low nibble (0 entity, 2 / 3 / 4 / 5 /
 * 6 / 7 / 8 = A / B / C / D / C / E / F), with a few per block exceptions. The port is written
 * that way; the emulator diff runs every type 0..9100 and negative ones.
 */
#include "gp3_wlo.h"

#include <string.h>

#define CALL(addr, a, b, c) e->call(e->user, (addr), (uint32_t)(a), (uint32_t)(b), (uint32_t)(c))

static uint16_t rd16(const uint8_t* p) { uint16_t v; memcpy(&v, p, 2); return v; }
static void wr16(uint8_t* p, uint16_t v) { memcpy(p, &v, 2); }
static void wr32(uint8_t* p, uint32_t v) { memcpy(p, &v, 4); }

/* entity_alloc, entity_list_insert (unless !insert), attach the record */
static uint32_t entity(const Gp3WloEnv* e, uint32_t rec, int insert) {
    uint32_t ent = CALL(0x00492d90, 0, 0, 0);
    if (insert) CALL(0x00492c4c, ent, 0, 0);
    CALL(0x00492de4, ent, rec, 0);
    return ent;
}

/* types 0x191 .. 0x1ae: the record kind byte picks the counter field */
static void counted(const Gp3WloEnv* e, uint32_t t, uint32_t rec, uint32_t ctx) {
    uint8_t* r = e->resolve(e->user, rec);
    uint32_t cnt_off, clr_off;
    if (r[0] == 1) { cnt_off = 0x16; clr_off = 0x14; }
    else if (r[0] == 4) { cnt_off = 0x10; clr_off = 0x12; }
    else return;
    if (CALL(0x0045c338, 0, 0, 0) == 4 && t == 0x1a2) {
        r = e->resolve(e->user, rec);
        r[clr_off] = 0;
        return;
    }
    int32_t cnt = *e->object_counter;
    r = e->resolve(e->user, rec);
    wr16(r + cnt_off, (uint16_t)cnt);
    CALL(0x00449888, ctx, t, cnt);
    *e->object_counter = (int32_t)((uint32_t)*e->object_counter + 1u);
    CALL(0x00470770, rec, t, 0);
}

/* types 0x1f4..0x1fe, 0x207, 0x208: collision objects */
static void collision(const Gp3WloEnv* e, uint32_t t, uint32_t rec, uint32_t ctx) {
    uint8_t* r = e->resolve(e->user, rec);
    uint32_t old = rd16(r + 4);
    wr16(r + 4, (uint16_t)*e->collision_counter);
    CALL(0x0046ff40, rec, t, 0);
    r = e->resolve(e->user, rec);
    CALL(0x00470bc0, t, rd16(r + 4), ctx);
    *e->collision_counter = (int32_t)((uint32_t)*e->collision_counter + 1u);
    if (t == 0x1fc) CALL(0x004724c4, rec, 0, 0);
    if (t == 0x1fd) CALL(0x0047274c, 0, 0, 0);
    if (t == 0x1fe) CALL(0x00472a14, rec, old, 0);
    if (t == 0x1fa) CALL(0x00455ec0, entity(e, rec, 1), 0, 0);
    if (t == 0x1fb) CALL(0x00455ec0, entity(e, rec, 1), 1, 0);
}

/* types 0x10..0xff */
static void small(const Gp3WloEnv* e, uint32_t t, uint32_t rec) {
    uint32_t lo = t & 0xf, hi = t & 0xf0;
    if (lo == 0) {
        uint32_t ent = entity(e, rec, 1);
        CALL(0x00457428, ent, t, 0);
        if (t == 0xa0) CALL(0x00447dbc, rec, t, 0);
        if (t != 0xf0) *e->last_record = rec;
        return;
    }
    if (lo == 2) {
        switch (hi) {
        case 0x20: CALL(0x00453400, rec, 0, 0); return;
        case 0xb0: CALL(0x004534a0, rec, 0, 0); return;
        case 0xc0: CALL(0x00453450, rec, 0, 0); return;
        case 0xe0: CALL(0x00453428, rec, 0, 0); return;
        case 0xf0: CALL(0x00453478, rec, 0, 0); return;
        default: CALL(0x004706c4, rec, 0, 0); return;
        }
    }
    if (t == 0xc9) { CALL(0x00453e60, rec, 0, 0); return; }
    if (t == 0xca) { CALL(0x00453e90, rec, 0, 0); return; }
    if (t == 0xf9) { CALL(0x00447dbc, rec, t, 0); *e->last_record = rec; return; }
    switch (lo) {
    case 3: CALL(0x00453c5c, t, rec, 0); return;
    case 4: case 6: CALL(0x0046ff40, rec, t, 0); return;
    case 5: CALL(0x00453f50, t, rec, 0); return;
    case 7: CALL(0x00470964, t, *e->last_record, 0); return;
    case 8: CALL(0x00499b9c, rec, 2, 0); return;
    default: return;
    }
}

void gp3_world_load_object(const Gp3WloEnv* e, int32_t type, uint32_t rec, uint32_t ctx) {
    uint32_t t = (uint32_t)type;
    if (type >= 9000) { CALL(0x00469b60, rec, 0, 0); return; }
    if (t < 0x10) {
        uint32_t ent;
        uint8_t* p;
        switch (t) {
        case 1:
            if (*e->scene_root_done) return;
            CALL(0x00492de4, CALL(0x00492c44, 0, 0, 0), rec, 0);
            *e->scene_root_done = 1;
            return;
        case 2:
            ent = entity(e, rec, 0);
            CALL(0x00494c3c, ent, 0, 0);
            p = e->resolve(e->user, ent);
            wr32(p + 0x0c, 0);
            return;
        case 3:
            ent = entity(e, rec, 1);
            p = e->resolve(e->user, ent);
            wr32(p, 0xffe);
            wr32(p + 0x0c, 0);
            return;
        case 0xc: CALL(0x0046ff40, rec, t, 0); return;
        default: return;
        }
    }
    if (t < 0x100) { small(e, t, rec); return; }
    switch (t) {
    case 0x191: case 0x193: case 0x194: case 0x196: case 0x19b: case 0x19d:
    case 0x1a1: case 0x1a2: case 0x1a3: case 0x1ae:
        counted(e, t, rec, ctx);
        return;
    case 0x1f4: case 0x1f5: case 0x1f6: case 0x1f7: case 0x1f8: case 0x1f9: case 0x1fa:
    case 0x1fb: case 0x1fc: case 0x1fd: case 0x1fe: case 0x207: case 0x208:
        collision(e, t, rec, ctx);
        return;
    case 0x202: case 0x204: case 0x205: {
        CALL(0x0046ff40, rec, t, 0);
        uint8_t* r = e->resolve(e->user, rec);
        CALL(0x004712c8, t, (int32_t)(int16_t)rd16(r + 4), rec);
        return;
    }
    case 0x28a: CALL(0x00451498, rec, 0, 0); return;
    case 0x28c: CALL(0x00451574, rec, 0, 0); return;
    case 0x28d: CALL(0x0045154c, rec, 0, 0); return;
    case 0x28e: CALL(0x004514ac, rec, 0, 0); return;
    case 0x290: CALL(0x00451538, rec, 0, 0); return;
    case 0x292: CALL(0x00451594, rec, 0, 0); return;
    case 0x294: CALL(0x004514fc, rec, 0, 0); return;
    case 0x295: CALL(0x00451510, rec, 0, 0); return;
    case 0x296: CALL(0x00451524, rec, 0, 0); return;
    case 0x297: CALL(0x004514c0, rec, 0, 0); return;
    case 0x298: CALL(0x004514d4, rec, 0, 0); return;
    case 0x299: CALL(0x00451560, rec, 0, 0); return;
    case 0x29d:
        CALL(0x004573d4, entity(e, rec, 1), 0, 0);
        CALL(0x0046ff40, rec, t, 0);
        return;
    case 0x29e: case 0x29f: case 0x2a0: case 0x2a1:
        CALL(0x00451604, t, rec, 0);
        return;
    case 0x2a9:
        for (int i = 0; i < 5; i++) CALL(0x004573e4, entity(e, rec, 1), i, 0);
        return;
    case 0x2ab: CALL(0x00457118, rec, 0, 0); return;
    case 0x2ad: CALL(0x004572f4, rec, 0, 0); return;
    case 0x2af: CALL(0x004514e8, rec, 0, 0); return;
    case 0x2b0: CALL(0x004570a0, 0, 0, 0); return;
    case 0x2b2: CALL(0x0045721c, rec, 0, 0); return;
    case 0x2b4: CALL(0x00470938, rec, 0, 0); return;
    case 0x2ac: case 0x2ae: case 0x2b1: case 0x2b3:
        CALL(0x0046ff40, rec, t, 0);
        CALL(0x004a14c0, rec, 0, 0);
        CALL(0x004a14a0, rec, 0, 0);
        return;
    case 0x2b5: CALL(0x00454aa4, rec, 0, 0); return;
    case 0x4ec: CALL(0x00435934, rec, 0, 0); return;
    case 0x4f6: CALL(0x00435c50, 0, 0, 0); return;
    case 0x50a: case 0x50b: case 0x50c: case 0x50d:
        CALL(0x00436ff4, t, rec, 0);
        return;
    case 0x899: case 0x89a: case 0x8a0: case 0x8fd: case 0x911: case 0x912: case 0x916:
        CALL(0x00412670, rec, t, 0);
        return;
    case 0x8a1: case 0x8a2: case 0x8a4: case 0x8a6: case 0x8a9: case 0x913:
        CALL(0x00447dbc, rec, t, 0);
        return;
    case 0x915:
        CALL(0x00447dbc, rec, t, 0);
        CALL(0x0046ff40, rec, t, 0);
        return;
    case 0x914: case 0xbbc: case 0xbbd: case 0xc1d: case 0xc80: case 0xfa0: case 0xfa5:
        CALL(0x0046ff40, rec, t, 0);
        return;
    case 0xbbb: {
        CALL(0x0046ff40, rec, 0xbbb, 0);
        uint8_t* r = e->resolve(e->user, rec);
        CALL(0x00411dc8, ctx, rd16(r + 4), 0);
        return;
    }
    case 0xbbe: CALL(0x004119e4, rec, ctx, 0); return;
    case 0xc1c: CALL(0x00454fb8, rec, 0, 0); return;
    case 0xc1f:
        CALL(0x004570a4, entity(e, rec, 1), 0, 0);
        CALL(0x0046ff40, rec, 0xc1f, 0);
        CALL(0x004a14c0, rec, 0, 0);
        CALL(0x004a14a0, rec, 0, 0);
        return;
    case 0xc81: CALL(0x00412564, rec, 0, 0); return;
    case 0xc82: CALL(0x004125a8, rec, 0, 0); return;
    case 0xc83: CALL(0x004125a8, rec, 1, 0); return;
    case 0xc84: CALL(0x004125a8, rec, 2, 0); return;
    case 0xdaf: case 0xdb0:
        entity(e, rec, 1);
        CALL(0x00472b70, 0, 0, 0);
        return;
    case 0xfa1: case 0xfa2: case 0xfa3: case 0xfa4:
        CALL(0x004107f4, rec, ctx, t - 0xfa0);
        return;
    case 0xfa6: case 0xfa7: case 0xfa8: case 0xfa9:
        CALL(0x004112a4, rec, ctx, t - 0xfa5);
        return;
    case 5000: CALL(0x00499b9c, rec, 0, 0); return;
    case 5001: CALL(0x00499b9c, rec, 3, 0); return;
    case 0x13ec: {
        CALL(0x00499b9c, rec, 1, 0);
        uint8_t* r = e->resolve(e->user, rec);
        if (r[0] == 0) CALL(0x00411de8, ctx, rd16(r + 0x14), 0);
        return;
    }
    default: return;
    }
}
