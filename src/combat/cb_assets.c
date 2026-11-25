/* cb_assets.c: see cb_assets.h */
#include "combat/cb_assets.h"

static uint8_t* gp(const CbAssetsEnv* e, uint32_t a, uint32_t n) { return wmem_ptr(e->mem, a, n); }
static void wr32(const CbAssetsEnv* e, uint32_t a, uint32_t v) { w_s32(gp(e, a, 4), 0, v); }
static void wr16(const CbAssetsEnv* e, uint32_t a, uint32_t v) { w_s16(gp(e, a, 2), 0, v); }

static uint32_t make(const CbAssetsEnv* e, uint32_t id) {
    uint32_t o = e->entity_alloc(e->user, id);
    return o;
}

/* n objects into table: each alloc gets the previous object's address (ORIGINAL BUG) */
static uint32_t pool(const CbAssetsEnv* e, uint32_t table, int n, uint32_t id, uint32_t asset, int smoke) {
    uint32_t arg = id, o = 0;
    for (int k = 0; k < n; k++) {
        o = make(e, arg);
        wr32(e, table + 4 * k, o);
        e->entity_list_insert(e->user, o);
        wr32(e, o + 0x15C, asset);
        wr32(e, o + 8, 0);
        if (smoke) {
            wr32(e, o + 0x158, 0x8A2);
            wr32(e, o + 0x0C, 1);
            wr32(e, o + 0xF4, 1);
            wr16(e, o + 0xF8, 0);
            wr16(e, o + 0xFA, 0);
            wr32(e, 0x004ffab4, asset);
            wr16(e, o + 0xFC, 0x28);
        }
        arg = o;
    }
    return o;
}

uint32_t cb_weapon_asset_bind(const CbAssetsEnv* e, uint32_t asset, uint32_t id) {
    uint32_t o;
    switch (id) {
    case 0xA0:
        o = make(e, id);
        wr32(e, 0x004ffe1c, o);
        e->entity_list_insert(e->user, o);
        wr32(e, o + 8, 0);
        wr32(e, o + 0x0C, 0);
        wr32(e, o + 0x15C, asset);
        return o;
    case 0xF9:
        o = make(e, id);
        wr32(e, 0x004ffed4, o);
        e->entity_list_insert(e->user, o);
        wr32(e, o + 8, 1);
        wr32(e, o + 0x15C, asset);
        return o;
    case 0x8A1: return pool(e, 0x004ffb54, 2, id, asset, 0);
    case 0x8A2: return pool(e, 0x004ff8a8, 10, id, asset, 1);
    case 0x8A4: return pool(e, 0x004ffaf8, 23, id, asset, 0);
    case 0x8A9: return pool(e, 0x004ffab8, 4, id, asset, 0);
    case 0x8A6:
        wr32(e, 0x004ffab0, asset);
        return id;
    case 0x915:
        o = make(e, id);
        wr32(e, 0x004ffed0, o);
        e->entity_list_insert(e->user, o);
        wr32(e, 0x004ffe9c, 0);
        wr32(e, 0x004ffea4, 0);
        wr32(e, 0x004ffea8, 0);
        wr32(e, 0x004ffeac, (uint32_t)-40);
        wr32(e, o + 8, 0);
        wr32(e, 0x004ffeb0, 0);
        wr32(e, o + 0x158, 0x8A2);
        wr32(e, 0x004ffeb4, (uint32_t)-40);
        wr32(e, o + 0x0C, 1);
        wr32(e, 0x004ffeb8, 0);
        wr32(e, o + 0xF4, 1);
        wr32(e, 0x004ffebc, 0);
        wr16(e, o + 0xF8, 0);
        wr32(e, 0x004ffec4, 0);
        wr16(e, o + 0xFA, 0);
        wr32(e, 0x004ffec8, 0);
        wr32(e, o + 0x15C, asset);
        wr16(e, o + 0xFC, 0x50);
        wr32(e, 0x004ffea0, 0x28);
        wr32(e, 0x004ffec0, 0x28);
        return o;
    default:
        return id;
    }
}

uint32_t cb_warp_objects_create(const CbAssetsEnv* e, uint32_t asset) {
    uint32_t arg = asset, o = 0;
    for (int k = 0; k < 3; k++) {
        o = make(e, arg);
        wr32(e, 0x00502ec4 + 0x28 * k, o);
        e->entity_list_insert(e->user, o);
        wr32(e, o + 0x15C, asset);
        wr32(e, o + 8, 0);
        arg = o;
    }
    return o;
}
