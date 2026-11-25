/*
 * mt_misc.c: small shell / car helpers (see mt_misc.h).
 */
#include "mt_misc.h"

#include <string.h>

int32_t mt_human_car_leave(const MtMisc* m, int32_t i) {
    int32_t z = 0;
    memcpy(m->resolve(m->user, m->human_objects[i]) + 8, &z, 4);
    uint8_t* car = m->human + (uint32_t)i * 0x898u;
    memcpy(car + 0x1d4, &z, 4);
    int16_t type;
    memcpy(&type, car + 0x90, 2);
    m->car_type_release(m->user, (uint32_t)(int32_t)type, 0, 0, 0);
    int32_t last = (int32_t)((uint32_t)*m->human_count - 1u);
    m->flags_568[i] = 0;
    return i < last ? last : i;
}

void mt_key_defaults(const MtMisc* m) {
    for (int e = 0; e < 51; e++) {
        const uint8_t* k = m->key_defaults + 5 * e;
        m->key_set_pad(m->user, k[0], k[1], 0, 0);
        m->key_set_pad(m->user, k[0], k[4], 1, 0);
        m->key_set_2278(m->user, k[0], k[2], 0, 0);
        m->key_set_27bc(m->user, k[0], k[3], 0, 0);
    }
}

uint32_t mt_shell_lists_init(const MtMisc* m) {
    m->list_reset(m->user, 0x004e7548u, 0, 0, 0);
    uint32_t r = m->list_create(m->user, 0x0050e444u, 1, 2, 3);
    uint32_t v = 0x004e50d8u;
    memcpy(m->resolve(m->user, r) + 0x30, &v, 4);
    *m->list_mode = 1;
    r = m->list_create(m->user, 0x0050e4f8u, 1, 2, 3);
    memcpy(m->resolve(m->user, r) + 0x30, &v, 4);
    *m->list_mode = 2;
    m->list_462168(m->user, 0x0050e708u, 0, 0, 0);
    *m->list_mode = 3;
    return m->list_4a3620(m->user, 0x0050ea98u, 0, 0, 0);
}
