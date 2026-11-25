/*
 * sh2_events.h: match event effects of TM2.EXE 0x0047259c..0x00473532 (the scripted explosions
 * of a destroyed landmark / boss, the special event, the two player target events) and the car
 * state snapshot of 0x00473254.
 *
 * An event record (0x00622ab0, 0x00622ada, and two of 0x2c bytes at 0x00622af4):
 *   +0 u8 active, +1 u8 done (0x00622ada: +2 active, +3 phase), then i16 camera object id,
 *   i16 frame counter, i16 explosion rate (counts down, floor 2 / 5), i16 the big explosion's
 *   frame, u32 debris model node, +0x10 position (3 x i32), +0x1c direction, +0x24 spin.
 * Each frame: counter + 1; on frame 1 a big explosion; with chance 1 / rate a small one near the
 * position (math_jitter_pos 0x0044a40c), alternating kinds by rand(); on the trigger frame a
 * screen flash, a big explosion, a debris burst (0x0048a738) and the camera object's animation
 * step (0x00470320); some frames later the music cue (0x0046f974 / 0x0046f980) and sound 4.
 * All callees are reached through Sh2Env.sh.call (verified ports exist in src/weapons,
 * src/match, src/engine with their own environments).
 *
 * Verified against the original under emulation (tools/emu/check_sh2_events.sh).
 */
#ifndef TM2_SH2_EVENTS_H
#define TM2_SH2_EVENTS_H

#include <stdint.h>
#include "sh2_flow.h"

/* 0x0047259c: the event at 0x00622ab0 (event type 1): small explosions 12 x 12 x 41, rate floor
 * 2; trigger: debris (stack 0, 0, 0x28, 0x78, 0), camera step 0x1fc; trigger + 60: done. */
void sh2_event_destroyed_fx(const Sh2Env* e);
/* 0x004727a8: the event at 0x00622ada (event type 5), phases 0 and 1: small explosions 20 x 20 x
 * 10; trigger in phase 0: object (0x00470b14) with more than 2 parts gets the debris burst with
 * the direction read from TM2.EXE 0x004e7ff4 (8 bytes, a local copy), camera step 0x207 and the
 * event restarted for phase 1 (counter 0, rate 0x14, trigger 0x50, position 0x3ff8 / 0x4d50 /
 * 0x15e0); in phase 1 camera step 0x1fd and phase 2. Music and sound on every trigger. */
void sh2_event_special_fx(const Sh2Env* e);
/* 0x00472b74 (PS1 targetPlayerEventsUpdate): the two records at 0x00622af4: in game modes 2 / 4
 * (0x0045c338) an active record ends at once (flash, explosion, camera step 0x1fe, music, sound);
 * else the scripted explosions (82 x 82 x 48, rate floor 5, kinds 0x00450984 / 0x00450a10), the
 * trigger, and the end at trigger + 30. The end clears 0x004fdc79 (record 0) or 0x004fdc77. */
void sh2_target_player_events(const Sh2Env* e);
/* 0x00473254: when on, the car's state into the snapshot at 0x00622d24: +0 i16 = car +0x6f8,
 * +2 = +0x1d8, +4 i16 = (i16 car +0x6ce) * 100 / (i32 car +0x1ec) / fps, +6 i16 = (i16 +0x6c4)
 * * 100 / (i16 +0x19c) / fps (fps = 0x004931d4, 16 bit divisions), +8..+0x10 = +0x6e4..+0x6ec,
 * +0x14 u16 bits 0..13 = bit 0 of car bytes +0xcc +0xcd +0xce +0xcf +0xd2 +0xd9..+0xdf, +0x154,
 * +0xb4 (bits 14, 15 kept). */
void sh2_car_net_state(const Sh2Env* e, uint32_t car, uint8_t on);
/* 0x004713f0 (PS1 targetActorPlace): a target actor's start place by the event type
 * (0x00474508): actor +4 i16 target type, +6 i16 instance, +0x0c..+0x14 i32 position, +0x48 its
 * model node (node +0xe0..+0xe4 i16 rotation zeroed, +0xe8..+0xf0 i32 position). Per event type
 * a table in TM2.EXE (read from game memory, never kept here): either a fixed row or a row whose
 * x and y get rand() / 0x199 added; for target types 0x202 / 0x205 in event types 1, 3, 6, 8
 * the row (24 bytes) also has the actor's own position. Then actor +0 = the row, +1 = 1,
 * +2 = 0, +8 u16 = 0 (event types 2 and 4: row 0, nothing placed; 7, 9, 10 and others: nothing).
 * The node pointer is read again for every store, as the original does. */
void sh2_target_actor_place(const Sh2Env* e, uint32_t actor);

#endif /* TM2_SH2_EVENTS_H */
