/*
 * mt_registry.c: the arena object registry (see mt_registry.h).
 */
#include "mt_registry.h"

#include <string.h>

static void put(const MtRegistry* r, uint32_t off, uint32_t obj) { memcpy(r->block + off, &obj, 4); }

void mt_obj_registry_add(const MtRegistry* r, uint32_t obj, int32_t id) {
    if (id > 0xf && id < 0x190) {
        uint32_t s = (uint32_t)(id >> 4) * 4u;
        if ((id - 6) % 16 == 0)
            put(r, s, obj);
        else if ((id - 7) % 16 == 0)
            put(r, 0x40 + s, obj);
    }
    uint32_t u = (uint32_t)id;
    uint8_t* o = r->resolve(r->user, obj);
    uint16_t k;
    memcpy(&k, o + 4, 2);
    int fixed = -1, indexed = -1, list = 0;
    switch (u) {
    case 0xc: fixed = 0x398; break;
    case 0x914: fixed = 0x80; break;
    case 0x915: fixed = 0x84; break;
    case 0x202: case 0x204: case 0x205: indexed = 0x88; break;
    case 0x2b1: fixed = 0xa4; break;
    case 0xc1f: fixed = 0xa8; break;
    case 0x2ac: indexed = 0xac; break;
    case 0x2b3: indexed = 0xc0; break;
    case 0x2ae: indexed = 0xd4; break;
    case 0x29d: fixed = 0xe8; break;
    case 0xfa0: indexed = 0x344; break;
    case 0xfa5: indexed = 0x358; break;
    case 0xbbb: indexed = 0x36c; break;
    case 0xbbc: fixed = 0x380; break;
    case 0xbbd: fixed = 0x384; break;
    case 0xc1d: fixed = 0x388; break;
    case 0xc80: fixed = 0x38c; break;
    case 0x1f4: case 0x1f5: case 0x1f6: case 0x1f7: case 0x1f8: case 0x1f9: case 0x1fa:
    case 0x1fb: case 0x1fc: case 0x1fd: case 0x1fe: case 0x207: case 0x208: list = 1; break;
    default: return;
    }
    if (o[0] != 9) return;
    if (fixed >= 0) {
        put(r, (uint32_t)fixed, obj);
    } else if (indexed >= 0) {
        put(r, (uint32_t)indexed + 4u * k, obj);
    } else if (list) {
        int32_t n;
        memcpy(&n, r->block + 0x39c, 4);
        if (n >= 0x32) return;
        uint16_t n16 = (uint16_t)n;
        put(r, (uint32_t)(0x1b4 + 4 * n), obj);
        memcpy(o + 4, &n16, 2);
        n++;
        memcpy(r->block + 0x39c, &n, 4);
    }
}
