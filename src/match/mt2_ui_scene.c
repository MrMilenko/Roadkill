/*
 * mt2_ui_scene.c: 0x00469b60 and 0x00469d14. See mt2_ui_scene.h.
 */
#include "mt2_ui_scene.h"

#include <string.h>

static uint32_t rd(const uint8_t* p, uint32_t o) { uint32_t v; memcpy(&v, p + o, 4); return v; }
static uint16_t rd16(const uint8_t* p, uint32_t o) { uint16_t v; memcpy(&v, p + o, 2); return v; }
static void wr(uint8_t* p, uint32_t o, uint32_t v) { memcpy(p + o, &v, 4); }
static void wr16(uint8_t* p, uint32_t o, uint16_t v) { memcpy(p + o, &v, 2); }

#define R(h) s->resolve(s->user, (h))

static int send_list(const Mt2UiScene* s, const int32_t* count, const uint32_t* list, uint16_t id,
                     uint32_t msg, int found)
{
    int32_t i;
    for (i = 0; i < *count; i++) {
        uint8_t* o = R(list[i]);
        if (rd16(o, 0) == id) {
            s->call_fn(s->user, rd(R(rd(o, 8)), 0), list[i], msg);
            found = 1;
        }
    }
    return found;
}

void mt2_ui_scene_dispatch(const Mt2UiScene* s, uint32_t msg)
{
    uint16_t id = rd16(R(msg), 2);
    int found = 0;
    found = send_list(s, s->list_a_count, s->list_a, id, msg, found);
    found = send_list(s, s->list_b_count, s->list_b, id, msg, found);
    found = send_list(s, s->list_c_count, s->list_c, id, msg, found);
    if (!found && s->fallback[1] != 0)
        s->call_fn(s->user, s->fallback[1], s->fallback[0], msg);
}

void mt2_ui_scene_model_item(const Mt2UiScene* s, uint32_t item, uint32_t target)
{
    uint8_t* it = R(item);
    uint32_t e;
    if (rd(it, 4) == 0 || rd16(it, 0xc) == 0)
        return;
    if (rd(it, 0x18) == 0) {
        wr(it, 0x18, s->call(s->user, 0x00492d90, item, 0, 0));
        e = s->call(s->user, 0x00495864, 0, 0, 0);
        wr(R(rd(it, 0x18)), 4, e);
        s->call(s->user, 0x00492de4, rd(it, 0x18), rd(it, 4), 0);
    }
    wr(R(rd(it, 0x18)), 0xe8, rd(it, 0x1c));
    wr(R(rd(it, 0x18)), 0xec, rd(it, 0x20));
    wr(R(rd(it, 0x18)), 0xf0, rd(it, 0x24));
    wr16(R(rd(it, 0x18)), 0xe0, rd16(it, 0x2c));
    wr16(R(rd(it, 0x18)), 0xe2, rd16(it, 0x30));
    wr16(R(rd(it, 0x18)), 0xe4, rd16(it, 0x28));
    e = rd(it, 0x18);
    s->call(s->user, 0x0048c360, e + 0xe0, e + 0xb0, 0);
    if (rd16(it, 0xe) != 0) {
        s->call(s->user, 0x004948b4, 0, 0, 0);
    } else {
        int32_t v = (int16_t)rd16(it, 0x10), d, dist;
        if (v != -1)
            wr(R(rd(it, 0x18)), 0, (uint32_t)v);
        d = (int32_t)rd(it, 0x14);
        if (d < 0) {
            int32_t a = (int32_t)(0u - (uint32_t)d);  /* INT_MIN stays INT_MIN */
            dist = 8 / a;
        } else {
            dist = (int32_t)((uint32_t)d * 8u);
        }
        s->call(s->user, 0x00494ec4, 0, (uint32_t)dist, 1);
    }
    s->call(s->user, 0x004a10fc, target, rd(it, 0x18), 0);
}
