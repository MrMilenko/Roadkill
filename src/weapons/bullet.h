/*
 * bullet.h: machine gun bullets, ported from TM2.EXE (PS1 decomp: bulletSpawn, bulletsMove in
 * src/semantic/weapon_bullets.c).
 *
 *   0x00412dc4 bullet_spawn   takes a free record: velocity = dir * 200 / max(|dir|) (max at least 1)
 *   0x00412fb8 bullets_move   moves every live bullet and tests 8 points along this frame's step
 *
 * Bullet record (40 of 0x38 bytes at 0x004f0910):
 *   +0x00 i32[3] pos   +0x0C i32[3] last pos   +0x18 i32[3] velocity   +0x24 i32 owner
 *   +0x28 i32 frames left (> 0 = live; 15, or 20 for owners >= 0x32 other than 1)
 *   +0x2C i32 damage   +0x30 i32 what was hit   +0x34 u8 flag   +0x35 u8 = 0
 * Move: last = pos, frames - 1, pos += velocity, then for k = 7 .. 0 the point pos - (v * k >> 3):
 * entity hit test 0x004126b4(point, owner, owner < 0x32 ? 10 : 5, -1) (own hits ignored), and for
 * k != 1 without an entity hit the level test 0x0048df8c(owner, -1, point, &hit); collision records
 * of the owner's own id negated, 0x29B, 0x29C, 0xCE5, 0x191..0x1B8 and 0xDAC..0xDB5 are ignored
 * (their +0x0A is cleared). On a hit: spark 0x00450718 at the previous point (the hit point when
 * the hit is 1), smoke 0x00450fd4 on rand() & 1, ricochet 0x004513c8 unless the hit is 9, the bullet
 * dies, and on another rand() & 1 the hit response 0x00415d1c(hit, record +0x0A, record +0x0C, damage,
 * velocity, owner).
 * ORIGINAL BUG: the collision record pointer is kept across bullets: a bullet that hits an entity at
 * k = 7 before any level test passes the last record of an earlier bullet to the hit response.
 *
 * Verified against the original under emulation (tools/emu/check_weapons_bullet.sh).
 */
#ifndef TM2_BULLET_H
#define TM2_BULLET_H

#include <stdint.h>

#include "weapons/wmem.h"

#define BULLET_COUNT 40
#define BULLET_SIZE 0x38

enum { BULLET_FX_SPARK = 0x00450718, BULLET_FX_SMOKE = 0x00450fd4, BULLET_FX_RICOCHET = 0x004513c8 };

typedef struct {
    const WMem* mem;          /* the collision record and spawn arguments */
    uint8_t* bullets;         /* 0x004f0910 */
    void* user;
    int32_t (*hit_test)(void* user, const int32_t pos[3], int32_t owner, int32_t radius, int32_t type);
    uint32_t (*level_hit)(void* user, int32_t owner, uint32_t model, const int32_t pos[3], int32_t* hit);
    void (*fx)(void* user, uint32_t fn, const int32_t pos[3]);
    void (*apply_hit)(void* user, int32_t hit, int32_t rtype, int32_t rc, int32_t dmg, const int32_t vel[3],
                      int32_t owner);
} BulletEnv;

void bullet_spawn(BulletEnv* env, int32_t owner, uint8_t flag, uint32_t dir, uint32_t pos, int32_t damage);
void bullets_move(BulletEnv* env);

#endif /* TM2_BULLET_H */
