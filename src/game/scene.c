/*
 * scene.c: the front end's scene engine (see scene.h).
 */
#include "scene.h"

static int32_t i32(const uint8_t* b, int o) { return (int32_t)(b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24); }
static void w32(uint8_t* b, int o, uint32_t v) { for (int k = 0; k < 4; k++) b[o + k] = (uint8_t)(v >> (8 * k)); }

uint8_t* scene_image_init(uint8_t* rec, const char* name, uint32_t value) {
    /* as the original: copy a byte, stop on zero, copy the next, stop on zero, two at a time */
    uint8_t* d = rec;
    const uint8_t* s = (const uint8_t*)name;
    for (;;) {
        uint8_t c = s[0];
        d[0] = c;
        if (!c) break;
        c = s[1];
        d[1] = c;
        s += 2;
        d += 2;
        if (!c) break;
    }
    rec[0x130] = 0;
    w32(rec, 0x12c, value);
    return rec;
}

static int32_t list_add(uint8_t* sc, int count_off, int max, uint32_t obj) {
    int32_t n = i32(sc, count_off);
    if (n > max - 1) return -1;
    w32(sc, count_off + 4 + 4 * n, obj);
    /* the original reads the count again after the store (a negative count makes the store land
     * on the count itself) */
    n = i32(sc, count_off);
    w32(sc, count_off, (uint32_t)n + 1);
    return n;
}

int32_t scene_add_aux(uint8_t* sc, uint32_t obj) { return list_add(sc, 0x50, 8, obj); }
int32_t scene_add_decor(uint8_t* sc, uint32_t obj) { return list_add(sc, 0x74, 16, obj); }
int32_t scene_add_item(uint8_t* sc, uint32_t obj) { return list_add(sc, 0xb8, 32, obj); }

int32_t scene_add_action(uint8_t* sc, uint8_t event, uint32_t source, uint8_t type, uint32_t command) {
    int32_t n = i32(sc, 0x13c);
    if (n >= 0x96) return -1;
    int o = 0x140 + 0x10 * n;
    sc[o] = event;
    w32(sc, o + 4, source);
    sc[o + 8] = type;
    w32(sc, o + 0xc, command);
    n = i32(sc, 0x13c);                 /* read again, as list_add */
    w32(sc, 0x13c, (uint32_t)n + 1);
    return n;
}

int32_t scene_post_event(uint8_t* sc, uint8_t event, uint32_t source) {
    int32_t n = i32(sc, 0xacc);
    if (n > 0x95) return -1;
    sc[0xad0 + 8 * n] = event;
    w32(sc, 0xad4 + 8 * n, source);
    n = i32(sc, 0xacc);                 /* read again, as list_add */
    w32(sc, 0xacc, (uint32_t)n + 1);
    return n;
}

void scene_request_redraw(uint8_t* sc, const int32_t rect[4]) {
    w32(sc, 0x2c, 1);
    if (rect) {
        for (int k = 0; k < 4; k++) w32(sc, 0x30 + 4 * k, (uint32_t)rect[k]);
        return;
    }
    w32(sc, 0x38, 0x280);
    w32(sc, 0x3c, 0x1e0);
    w32(sc, 0x34, 0);
    w32(sc, 0x30, 0);
}

void scene_choice_set(uint8_t* node, uint8_t state) {
    if (node && node[0] == 9) node[7] = state;
}

int8_t scene_choice_get(const uint8_t* node) {
    if (!node || node[0] != 9) return -1;
    return (int8_t)node[7];
}

void scene_dispatch_events(uint8_t* sc, void (*run_once)(void* user, uint32_t command), void* user) {
    for (int32_t e = 0; e < i32(sc, 0xacc); e++) {
        uint8_t ev = sc[0xad0 + 8 * e];
        uint32_t src = (uint32_t)i32(sc, 0xad4 + 8 * e);
        for (int32_t a = 0; a < i32(sc, 0x13c); a++) {
            int o = 0x140 + 0x10 * a;
            uint32_t asrc = (uint32_t)i32(sc, o + 4), cmd = (uint32_t)i32(sc, o + 0xc);
            if (sc[o] != ev || (asrc != src && asrc != 0)) continue;
            uint8_t type = sc[o + 8];
            if (type == 0) {
                int32_t n = i32(sc, 0xaa0);
                if (n < 10) {
                    w32(sc, 0xaa4 + 4 * n, cmd);
                    w32(sc, 0xaa0, (uint32_t)(n + 1));
                }
            } else if (type == 1) {
                int32_t n = i32(sc, 0xaa0), at = -1;
                for (int32_t k = 0; k < n; k++)
                    if ((uint32_t)i32(sc, 0xaa4 + 4 * k) == cmd) { at = k; break; }
                if (at != -1) {
                    /* the original shifts from the found slot up to the count, reading one past it */
                    for (int32_t k = at; k < n; k++) w32(sc, 0xaa4 + 4 * k, (uint32_t)i32(sc, 0xaa8 + 4 * k));
                    w32(sc, 0xaa0, (uint32_t)(n - 1));
                }
            } else if (type == 2) {
                if (run_once) run_once(user, cmd);
            }
        }
    }
    w32(sc, 0xacc, 0);
}
