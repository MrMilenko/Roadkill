/*
 * sh_misc.h: small shell helpers ported from TM2.EXE: the per player input / pad state reset
 * (0x00473160 / 0x00473168), two menu command callbacks (0x00485880, 0x004858a0) and the audio
 * settings defaults (0x00486b44). Game memory through ShEnv (sh_mem.h), by game address.
 *
 * Player state 0x00622d24: 8 blocks of 0x40c bytes: +0 u16, +2 u16, +4 u16, +6 u16, +8 / +0xc /
 * +0x10 u32, +0x14 u16 (top two bits kept), +0x16 u8 (0x7f), +0x17..+0x3f3 bytes.
 * 0x00624d84 u32, 0x00624d88 u8, 0x00624d8c u8[8], 0x00624d94 u32[8] (frame parity per player).
 *
 * Verified against the original under emulation (tools/emu/check_shell_misc.sh).
 */
#ifndef TM2_SH_MISC_H
#define TM2_SH_MISC_H

#include <stdint.h>
#include "sh_mem.h"

/* 0x00473168: with full (al != 0) every player block cleared (+0x16 = 0x7f) and the 8 flag bytes;
 * always: 0x00624d84 = 0, block 0 +0x16 = 0x7f, 8 frame_parity() values, 0x00624d88 = 0.
 * (Paired with PS1 humanCarSlideStep: wrong.) */
void sh_players_reset_ex(const ShEnv* e, int full);
/* 0x00473160: sh_players_reset_ex(1) (falls into 0x00473168 with al = 1; paired with PS1
 * humanCarsSetCount: wrong). */
void sh_players_reset(const ShEnv* e);
/* 0x00485880: cmd +0xC (target) +0x48 = cmd +0x10. */
void sh_cmd_set_value(const ShEnv* e, uint32_t cmd);
/* 0x004858a0: target = cmd +0xC: target +0x48 = (v + n - 1) % n with v = +0x48, n = +0xB8
 * (C division, as idiv); returns the quotient (left in eax). Step back with wrap. n = 0 faults
 * in the original (divide error); the port returns 0 and leaves the value. */
int32_t sh_cmd_step_back(const ShEnv* e, uint32_t cmd);
/* 0x00486b44: audio settings defaults: +0x78 = 0xe666, +0x7C = 0x9999, +0x80 = +0x84 = 0, copied
 * to +0x64 / +0x68 / +0x6C / +0x70 and +0x50 / +0x54 / +0x58 / +0x5C. */
void sh_audio_defaults(const ShEnv* e, uint32_t cfg);

/* The outgoing network message buffer: player block 0 +0x16 (0x00622d3a), fill count 0x00624d84
 * (signed compares). Each record starts with 0x2a and a byte tag. On overflow net_overflow is
 * called and nothing is written. */
/* 0x00473534: end mark 0x7f at the fill point, net_send(0x22, 0x00622d24, count + 0x17); then
 * when reset (al) != 0, sh_players_reset_ex(0). */
void sh_net_flush(const ShEnv* e, uint8_t reset);
/* 0x00473568: {0x2a, a, b, 0} (count < 0x3db) */
void sh_net_msg_ab(const ShEnv* e, uint8_t a, uint8_t b);
/* 0x004735b4: {0x2a, a, -, -, u32 v} (count < 0x3d6; bytes 2, 3 left as they are) */
void sh_net_msg_a32(const ShEnv* e, uint8_t a, uint32_t v);
/* 0x004735fc: {0x2a, a, b, c} (count < 0x3da) */
void sh_net_msg_abc(const ShEnv* e, uint8_t a, uint8_t b, uint8_t c);
/* 0x00473650: {0x2a, a, u16 h, u32 v} (count < 0x3d6) */
void sh_net_msg_a16_32(const ShEnv* e, uint8_t a, uint16_t h, uint32_t v);

#endif /* TM2_SH_MISC_H */
