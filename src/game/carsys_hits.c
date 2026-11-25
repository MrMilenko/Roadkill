/*
 * carsys_hits.c: car knock back, skid smoke, spin out by id (see carsys_hits.h).
 */
#include "carsys_hits.h"

#include <string.h>

#include "../math/tm2math.h"
#include "carsys_misc.h"

static int32_t rd32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }
static int16_t rd16(const uint8_t* p) { int16_t v; memcpy(&v, p, 2); return v; }
static void wr16(uint8_t* p, int16_t v) { memcpy(p, &v, 2); }
static void wr32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }

void carsys_knock_back(uint8_t* car, int player, const int32_t* pos, int32_t amount) {
    uint8_t* contact = car + (player ? 0x21C : 0xCC);
    const uint8_t* stats = car + (player ? 0x138 : 0x4EC);
    if ((rd32(stats + 0x78) >> 16) > 0x32) amount /= 2;
    memcpy(contact + 0x364, pos, 12);
    contact[5] = 0;
    contact[4] = 0;
    contact[3] = 0;
    wr16(contact + 0x12, (int16_t)amount);
    if (amount < 0x28) wr16(contact + 6, 0x78);
    else if (amount > 800) wr16(contact + 6, 300);
    else wr16(contact + 6, (int16_t)(300 - (800 - amount) / 800));  /* always 300, as the original */
}

void carsys_skid_smoke(uint8_t* car, int player, const CarsysHitOps* ops) {
    uint8_t* ctl = car + (player ? 0x9A : 0x68);
    const uint8_t* stats = car + (player ? 0x138 : 0x4EC);
    const uint8_t* body = car + (player ? 0x6C4 : 0x708);
    uint8_t* probes = car + (player ? 0x58C : 0x5D0);
    if (ctl[0x0B] == 0 && (ctl[0x01] != 0 || (player && ctl[0x1C] != 0))) {
        int32_t half = rd32(stats + 0xC4) / 2;
        int32_t speed = rd32(body + 8) >> 16;
        if (speed < 0) speed = -speed;
        if (speed > half) {
            for (int k = 0; k < 4; k++)
                ops->smoke(ops->user, ops->jitter(ops->user, probes + ((k & 1) ? 0x12C : 0xE4), 0x555));
            ctl[0x18] = 1;
        }
        return;
    }
    ctl[0x18] = 0;
}

void carsys_spin_out_by_id(int16_t id, int16_t angle, uint8_t back, const CarsysHitOps* ops) {
    const CarRecords* r = ops->rec;
    uint8_t* obj = carsys_object_from_owner(r, id);
    if (!obj) return;
    uint8_t human;
    int32_t index;
    if (!carsys_find_by_object(r, obj, &human, &index)) return;
    uint8_t* car = human ? carsys_human_get(r, index) : carsys_ai_get(r, index);
    uint8_t* ctl = car + (human ? 0x9A : 0x68);
    uint8_t* probes = car + (human ? 0x58C : 0x5D0);
    uint8_t* body = car + (human ? 0x6C4 : 0x708);
    uint8_t* contact = car + (human ? 0x21C : 0xCC);
    uint8_t* stats = car + (human ? 0x138 : 0x4EC);
    ctl[0x15] = 0;
    ctl[0x1D] = 0;
    int32_t speed = (int32_t)tm2_frame_ticks60() * 0x532 / 100 * 32;
    if (back) {
        ops->spin_out(ops->user, car, human, 2);
        wr16(body + 0x0A, (int16_t)(speed / 3));
        int32_t h = angle + 0x800;
        tm2_angle_wrap32(&h);
        wr32(body + 0x40, h);
    } else {
        ops->spin_out(ops->user, car, human, 1);
        wr16(body + 0x0A, (int16_t)speed);
        wr32(body + 0x40, angle);
    }
    wr16(contact + 0x0A, 0);
    wr16(contact + 0x06, 0xB4);
    ctl[0x19] = 0;
    ctl[0x01] = 1;
    carsys_body_spin_clear(body);
    ops->shake(ops->user, car, human);
    if (!back && (rd32(stats + 0x78) >> 16) < 0x23) {
        ctl[0x12] = 1;
        int32_t heading = rd32(body + 0x40);
        wr16(body + 0x02, (int16_t)heading);
        int32_t d = heading - rd32(body + 0x34);
        tm2_angle_wrap32(&d);
        if (d < 0) {
            wr16(body + 0x12, 1);
            wr16(body + 0x04, (int16_t)(heading + 0x400));
        } else {
            wr16(body + 0x12, -1);
            wr16(body + 0x04, (int16_t)(heading - 0x400));
        }
        wr16(stats + 0x20, 0);
    }
    for (int k = 0; k < 4; k++) wr16(probes + 0x32 + 0x48 * k, 0);
}

void carsys_hit_pass(const CarRecords* rec, const uint8_t* active, const uint8_t* summary) {
    uint8_t human;
    uint8_t* car = carsys_from_slot(rec, rd16(active + 0x14), &human);
    if (!car) return;
    uint8_t* base = car + (human ? 0x21C : 0xCC);
    const uint8_t* body = car + (human ? 0x6C4 : 0x708);
    int32_t d = (int32_t)((uint32_t)rd32(body + 0xF4) - (uint32_t)(int32_t)rd16(summary + 0x0A));
    if (rd16(body + 0x1A) < 0) d = (int32_t)((uint32_t)d + 0x800u);
    tm2_angle_wrap32(&d);
    int32_t bounce = (int32_t)(0x800u - 2u * (uint32_t)d);
    tm2_angle_wrap32(&bounce);
    int32_t rel = (int32_t)((uint32_t)rd32(body + 0x34) - (uint32_t)bounce);
    tm2_angle_wrap32(&rel);
    int part = rel > 0x400 ? 1 : rel < -0x400 ? 0 : rel > 0 ? 2 : 3;
    uint8_t* dst = base + 0x19C + 0x24 * part;
    memcpy(dst, summary, 0x24);
    dst[1] = 1;
    dst[3] = (uint8_t)part;
    int32_t floor = (int32_t)tm2_frame_ticks60() * 0x2F8 / 100 * 32;
    int32_t speed = rd16(body + 0x0A);
    wr16(dst + 6, (int16_t)(floor < speed ? speed : floor));
    wr16(dst + 0x0C, (int16_t)bounce);
}

void carsys_slam_carry_target(int16_t id, const uint8_t* stats, const uint8_t* body, uint8_t lifting,
                              uint8_t side, const CarsysHitOps* ops) {
    const CarRecords* r = ops->rec;
    uint8_t human;
    int32_t index;
    if (!carsys_find_by_object(r, carsys_object_from_owner(r, id), &human, &index)) return;
    int32_t v[3] = {0, rd16(stats + 0x4E) + 0x78, 0};
    if (lifting) {
        int32_t lift = tm2_sin(0x38E - *ops->slam_arm) * 120 / 0xFFF;
        v[1] = side ? v[1] - lift : v[1] + lift;
    }
    int32_t m[9], w[3];
    memcpy(m, body + 0x50, sizeof m);
    tm2_mat_vec_mul_row(m, v, w);
    uint8_t* car = human ? carsys_human_get(r, index) : carsys_ai_get(r, index);
    uint8_t* tb = car + (human ? 0x6C4 : 0x708);
    uint8_t* probes = car + (human ? 0x58C : 0x5D0);
    int32_t rate = lifting ? 0x14 : 0x0D;
    wr32(tb + 0x20, tm2_angle_step(rd32(tb + 0x20), (int32_t)((uint32_t)rd32(body + 0x20) + (uint32_t)w[0]), rate));
    wr32(tb + 0x24, tm2_angle_step(rd32(tb + 0x24), (int32_t)((uint32_t)rd32(body + 0x24) + (uint32_t)w[1]), rate));
    int32_t d = (int32_t)((uint32_t)rd32(tb + 0x34) - (uint32_t)rd32(body + 0x34));
    tm2_angle_wrap32(&d);
    int32_t aim = (int32_t)((uint32_t)rd32(body + 0x34) + (d < 0 ? (uint32_t)-0x400 : 0x400u));
    tm2_angle_wrap32(&aim);
    wr32(tb + 0x34, tm2_angle_interp(rd32(tb + 0x34), aim, 9));
    if (!lifting) return;
    int32_t a = 0x38E - *ops->slam_arm;
    int32_t c = tm2_sin(a) * 120 / 0xFFF;
    int32_t c2 = c + (int32_t)((uint32_t)(rd16(stats + 0x48) / 2) * (uint32_t)tm2_sin(a)) / 0xFFF;
    int near = d < 0 ? 0x32 : 0x7A, far = d < 0 ? 0x7A : 0x32;
    wr16(probes + near, (int16_t)c);
    wr16(probes + near + 0x90, (int16_t)c);
    wr16(probes + far, (int16_t)c2);
    wr16(probes + far + 0x90, (int16_t)c2);
}
