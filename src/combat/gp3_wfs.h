/*
 * gp3_wfs.h: weaponFireSpecial, TM2.EXE 0x004401fc (PS1 pair: weaponFireSpecial,
 * src/semantic/weapon_basic.c). eax = car block, dl = 1 for a human car (0: AI car).
 * Fires the car's selected weapon: the per car special (weapon slot 0, by car type) or one of
 * the pickup weapons (slots 1..7).
 *
 * Car block (human / AI):
 *   +0x00 i16 id (human)            +0x02 i16 index (human)     +0x04 i16 id (AI)  +0x06 i16 index (AI)
 *   +0x01 i8  AI kind (1: sets +0x44C)   +0x32 u8 AI "audible" flag   +0x44C u8 (AI only)
 *   +0x90 / +0x30 u16 car type (0x10..0xF0)
 *   stats  +0x138 / +0x4EC: i16 +0x44, +0xD4, +0xD6, +0xD8, +0xDC (launch offsets), +0xA0 (block)
 *   body   +0x6C4 / +0x708: +0x20 i32[3] position, +0x2C / +0x30 / +0x34 i32 angles, +0x50 i32[9]
 *   input  +0xCC / +0x40: +0x1B u8 fire latched, +0x1E u8 fire armed
 *   flags  +0x9A / +0x68: +0x00 u8, +0x09 u8 fired, +0x0D u8 blocked
 *   weapon state +0x848 / +0x834 (ws):
 *     +0x04 u8  +0x05 u8 rear fire  +0x06 u8 (slot 3 toggle)  +0x0D i8 selected slot
 *     +0x0E i16[8] ammo per slot (+0x0E special)  +0x22 i16 pickup total  +0x26 i16 cooldown
 *     +0x48 u32 object (type 0xC0 special)
 *   human only: +0x1D8, +0x1EC, +0x22C, +0x6E4, +0x6F0; AI only: +0x58C, +0x5A0, +0xDC, +0x728,
 *   +0x734 (published to the type 0x60 special's globals 0x004ffe34..0x004ffe68).
 *
 * Globals: 0x004fef54..0x004fef64 (target car, centre), the homing link table 0x004ff7f8 (5 x
 * 0x14: +0 i16 owner, +2 i16, +4 i16 timer, +6 i16, +8 i16 target id, +0xC u32 from position,
 * +0x10 u32 target position), the beam table 0x004ff7b8 (2 x 0x20), the mine objects 0x004ff8a8
 * (10 pointers) and records 0x004ff8d0 (10 x 0x30), 0x004ffab0 / 0x004ffab4, the specials' state
 * 0x004ffe1c..0x004ffe7c, 0x004ffecc, 0x004ffed0 (object), 0x004ffed8 (AI cooldown). The constant
 * vector at 0x004e6a44 is read from the player's TM2.EXE (through the memory map).
 *
 * Port model: every game pointer is a 32 bit game address; Gp3WfsEnv.reg maps address ranges to
 * host memory (car blocks, globals, the EXE constants, and `scratch`, 0x40 bytes the port uses
 * for the locals it hands to callees). Every call the original makes to code not ported here
 * goes through Gp3WfsEnv.call with the original's address and its Watcom registers (eax, edx,
 * ebx, ecx) and stack arguments; the result is the callee's eax. Math (0x0048cc14 M^T v,
 * 0x0048b624 sin, 0x0048b690 cos, 0x0048b71c atan2) runs in the port (src/math).
 *
 * ORIGINAL BUG kept: slot 6 (mines) copies 8 bytes of angles from the stack into object +0xE0;
 * +0xE6 is an uninitialized stack word (Gp3WfsEnv.stale_e6 stands in for it).
 * ORIGINAL BUG (harmless): 0x00453b10 clobbers ebx; the caller reloads it.
 *
 * Verified against the original under emulation (tools/emu/check_gp3_wfs.sh).
 */
#ifndef TM2_GP3_WFS_H
#define TM2_GP3_WFS_H

#include <stdint.h>

typedef struct {
    uint32_t base, size;
    uint8_t* host;
} Gp3WfsRegion;

typedef struct {
    Gp3WfsRegion reg[8];
    int nreg;
    uint32_t scratch;          /* game address of 0x40 bytes of port locals (mapped in reg) */
    uint16_t stale_e6;         /* the stack word the original copies to mine object +0xE6 */
    uint32_t (*call)(void* user, uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx,
                     uint32_t s0, uint32_t s1);
    void* user;
} Gp3WfsEnv;

/* 0x004401fc */
void gp3_weapon_fire_special(const Gp3WfsEnv* env, uint32_t car, uint8_t human);

#endif /* TM2_GP3_WFS_H */
