/*
 * sh2_flow.h: front end flow pieces of TM2.EXE 0x00472ec0..0x00485398 (the shell loop, the
 * attract demo, the challenge result and three page message screens, the car model / slot
 * setup of a session, the level file loading, the network player name list and audio glue).
 *
 * Every system not ported yet is reached through Sh2Env.sh.call with the original's address
 * (see sh_mem.h: n arguments = eax, edx, ebx, ecx, then the stack words in order). Indirect
 * calls go to the function pointer read from game memory, as the original does. A local buffer of
 * the function is passed as SH_LOCAL(k) and its host bytes are in Sh2Env.local[k] while the call
 * runs (so a caller can see what the original would have written there). Text and formats are
 * TM2.EXE data addresses passed on, never kept as text here.
 *
 * Game state 0x00624ea8 (see sh_names.h): +0 u8 game type, +1 u8 human count, +2 u8 opponent
 * count, +3 u8, +4.. human cars, +0x0d.. opponent cars, +0x16.. human modes, +0x1f u8 event,
 * +0x20, +0x21 u8 boss fight, +0x22, +0x24 u32, +0x28 u8 full version, +0x29.. +0x32, +0x33.
 *
 * Verified against the original under emulation (tools/emu/check_sh2_flow.sh).
 */
#ifndef TM2_SH2_FLOW_H
#define TM2_SH2_FLOW_H

#include <stdint.h>
#include "sh_mem.h"

typedef struct {
    ShEnv sh;
    uint8_t* local[4];      /* host bytes of SH_LOCAL(k) while a call runs */
} Sh2Env;

/* 0x00472ec0: 0x00c85040 = 0x8ca, then 0x004b90d0() (tail jump). */
void sh2_sound_init(const Sh2Env* e);
/* 0x0047312c: when 0x004b9150() (the sound device's state, nonzero = on) is set, 0x004b9268(h)
 * (edx = h too). */
void sh2_sound_stop_if_on(const Sh2Env* e, uint32_t h);
/* 0x0047314c: tail jump to 0x004b9270 (the sound manager update) with eax; returns its eax. */
uint32_t sh2_sound_update(const Sh2Env* e, uint32_t iface);
/* 0x00484260: draws overlay id with the display object's first method ([*0x00626010 + 0x1c][0],
 * (obj, id, 0, 0, 0x63, n, 0), n = the counter 0x009f6c18, incremented first), then the sound
 * update on the display object. */
void sh2_overlay_draw(const Sh2Env* e, uint32_t id);
/* 0x00485338: the shell loop: from the title (0x004780fc) every screen returns the next one
 * (called with eax = the previous screen, edx = the title); at 0 input is released (0x00496184)
 * and MessageBoxA(0x004a8f34(), 0x004e2b02, 0x004e2af2, 0x134) asks to quit: IDYES (6) ends it. */
void sh2_shell_loop(const Sh2Env* e);
/* 0x004740a8: the level files: two paths from name ("%s..." formats 0x004e2209 / 0x004e2213 with
 * 0x004e2205 / 0x004e220f), the DPC base set, both files read, the resource registered, the TPC
 * texture page registered and handed to 0x00497390, the resource reloaded, the weapons reset. */
void sh2_level_files_load(const Sh2Env* e, uint32_t name);
/* 0x00474ee4 (PS1 screenChallengeResult): music stopped and volume 0, "YOU WIN" (0x004e2531) or
 * "YOU LOSE" (0x004e2539) centered, the overlay, "WINS LOSSES" (0x004e2542) and the counts
 * ("%d %d" 0x004e2550 into 0x009f6920) below. */
void sh2_challenge_result(const Sh2Env* e, uint32_t a0, uint8_t win, int16_t wins, int16_t losses);
/* 0x0047506c (ret 0x24): three pages: page k (after half a second and the button checks for
 * k > 0) shows texts 0..k (centered font print, skipped when the text is 0). t[] = the 9 stack
 * words: text 0 = (edx, ecx, t[0], ebx), text 1 = (t[1], t[3], t[4], t[2]), text 2 =
 * (t[5], t[7], t[8], t[6]) as (font, x, y, text). */
void sh2_message_pages(const Sh2Env* e, uint32_t a0, uint32_t font0, uint32_t text0, uint32_t x0, const uint32_t t[9]);
/* 0x00475af8: session cars: slots set up (0x004576a8 with the signed top byte of +0x1c), every
 * human car (count +1, car ids +5..) its record's index (+0x894) and model (0x0045803c), every
 * opponent (count +2, ids +0x0d..) its model (0x00458340); model = 0x20 + 0x10 * id for 0..11,
 * 0x10 for 12, 0xe0 / 0xf0 for 13 / 14, else -1. Network game: sync and wait for message 0x11. */
void sh2_session_cars(const Sh2Env* e, uint32_t gs);
/* 0x00477b90: the attract demo: game state saved, a random one player match (rand() % 13 car,
 * four opponents, the event from 0x004e8020) run by the session runner 0x00475d34, the state
 * restored, the demo event advanced (1..8). Returns its argument. */
uint32_t sh2_attract_demo(const Sh2Env* e, uint32_t next);
/* 0x0047b934: network player names (game type >= 4 and item +0x0c set): for every player its
 * name (cut to 8 characters + "..." past 11) at x 0x1c2 and its car's name at x 0x226, rows from
 * y 0x28 by 0x14, through font_sprite_print_xy with *list as the last argument. Returns the eax
 * the original leaves (see .c). */
uint32_t sh2_net_player_names(const Sh2Env* e, uint32_t item, uint32_t list);

#endif /* TM2_SH2_FLOW_H */
