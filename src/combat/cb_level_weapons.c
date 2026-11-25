/* cb_level_weapons.c: see cb_level_weapons.h */
#include "combat/cb_level_weapons.h"

static uint32_t rd32(const CbLevelWeaponsEnv* e, uint32_t a) { return w_u32(wmem_ptr(e->mem, a, 4), 0); }
static void wr32(const CbLevelWeaponsEnv* e, uint32_t a, uint32_t v) { w_s32(wmem_ptr(e->mem, a, 4), 0, v); }

/* base + 4 * ((p - file) / 4), C division */
static uint32_t rebase(const CbLevelWeaponsEnv* e, uint32_t p) {
    int32_t d = (int32_t)(p - rd32(e, 0x0050002c));
    return rd32(e, 0x00500028) + 4u * (uint32_t)(d / 4);
}

void cb_level_init_weapons(const CbLevelWeaponsEnv* e) {
    uint32_t base = e->dpc_base(e->user);
    wr32(e, 0x00500028, base);
    wr32(e, 0x0050002c, rd32(e, base + 0x0C));
    wr32(e, 0x00500018, rd32(e, base + 0x10));
    e->fn_4a1314(e->user, rd32(e, base + 0x10));
    uint32_t list = rebase(e, rd32(e, base + 0x14));
    wr32(e, 0x00500020, 0);
    wr32(e, 0x00500024, 0);
    uint32_t n = rd32(e, list);
    list += 4;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t p = rebase(e, rd32(e, list));
        wr32(e, list, p);
        if (p == 0)
            return;
        e->relocate(e->user, p, rd32(e, 0x0050002c), 0);
        list += 4;
        e->walk(e->user, p, rd32(e, 0x00500018), 0x0044f20c);
    }
}
