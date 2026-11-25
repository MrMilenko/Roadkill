/*
 * sh_screen.h: the small front end screens and screen helpers of TM2.EXE 0x004751a4..0x004868c0
 * (movie skip checks, movie player, display init, the network / password screens, opponent
 * picking, menu command hooks). A screen function returns the next screen function (its TM2.EXE
 * address: 0x004780fc title, 0x00478edc single player menu, 0x00480c8c world globe, 0x0047a03c
 * save screen, 0x0047a18c network, 0x0047a1f8 / 0x0047a274 / 0x0047a308 network sub screens,
 * 0x004824c8 options); the shell loop (0x00485338) calls it next.
 *
 * Systems not ported yet are reached through ShEnv.call with the original's address (see
 * sh_mem.h), the background / scene file names are TM2.EXE data addresses passed on (never kept
 * as text here). The scene event post is the verified scene_post_event (src/game/scene.c).
 * Game state 0x00624ea8 (sh_names.h): +0x02 u8 opponent count, +0x0d u8[] opponent cars
 * (0x00624eb5), +0x21 u8 boss fight flag (0x00624ec9). 0x009f6c1c u8: Sweet Tooth unlocked.
 *
 * Verified against the original under emulation (tools/emu/check_shell_screen.sh).
 */
#ifndef TM2_SH_SCREEN_H
#define TM2_SH_SCREEN_H

#include <stdint.h>
#include "sh_mem.h"

#define SH_SCREEN_TITLE 0x004780fcu
#define SH_SCREEN_SINGLE 0x00478edcu
#define SH_SCREEN_GLOBE 0x00480c8cu
#define SH_SCREEN_SAVE 0x0047a03cu
#define SH_SCREEN_NETWORK 0x0047a18cu
#define SH_SCREEN_OPTIONS 0x004824c8u

/* 0x004751a4 (PS1 movieSkipCheckDelayed): poll input; after frame 0x23, 1 when button 0x13 or
 * 0x11 is ready. */
int32_t sh_movie_skip_check(const ShEnv* e, int32_t frame);
/* 0x004777f4: poll input (arg passed on), 1 when button 0x13 or 0x11 is ready. */
int32_t sh_skip_check(const ShEnv* e, uint32_t arg);
/* 0x004751dc (ret 4, the stack word unused): play movie name (path kind 6) with the audio
 * device, then the grey screen 0x004e2559 (kind 5) blitted and the user volume reapplied.
 * Returns the track handle. Decompiled C of the second call is wrong (see .c). */
int32_t sh_play_movie(const ShEnv* e, uint32_t name);
/* 0x004775a8: the display / audio subsystems set up once (flag 0x00624db4). Returns 0. */
int32_t sh_display_init_once(const ShEnv* e);
/* 0x004775f8: clears 0x00624db8..0x00624e17 (8 x 12 bytes). */
void sh_clear_624db8(const ShEnv* e);
/* 0x00480c08 (PS1 pair match_tournament_start): opponents picked (sh_pick_opponents); then the
 * save screen when event type > 1 in game type 1, else the world globe. */
uint32_t sh_match_tournament_start(const ShEnv* e);
/* the opponent pick shared by 0x00480c08 and 0x0047a0a4: boss fight: opponent 0 = car 14; else
 * with Sweet Tooth unlocked for the pick, every opponent rand() % 13 until car_is_selectable
 * (0x0047766c), the flag restored. */
void sh_pick_opponents(const ShEnv* e);
/* 0x0047a03c / 0x0047a0a4 / 0x0047a18c / 0x0047a1f8 / 0x0047a274: background + modal screens. */
uint32_t sh_screen_save(const ShEnv* e);
uint32_t sh_screen_password(const ShEnv* e);
uint32_t sh_screen_network(const ShEnv* e);
uint32_t sh_screen_network_a(const ShEnv* e);
uint32_t sh_screen_network_b(const ShEnv* e);
/* 0x0047dfec: menu command hook: when 0x004632f4(0x10) posts event 2 from cmd to its scene (+0xC). */
void sh_cmd_post_when_ready(const ShEnv* e, uint32_t cmd);
/* 0x00477aa0: menu item hook: base update (0x0046ace8), then for buttons k = 0..2 (int8 cmd
 * +0x24 + k) pressed (0x0044eee4): cmd +0x27 = k and event 0x12 + k posted from the scene's
 * current item (scene = cmd +0xC, item +0xBC[+0x48]). */
void sh_cmd_button_events(const ShEnv* e, uint32_t cmd);
/* 0x00484230: input config dialog (0x0045f2dc): 0 -> back, 1 -> next, else options. */
uint32_t sh_screen_input_cfg(const ShEnv* e, uint32_t back, uint32_t next);
/* 0x00484250: audio config dialog (0x00486f18 on 0x00624edc), then options. */
uint32_t sh_screen_audio_cfg(const ShEnv* e);
/* 0x00485398: render_configure(0x009f6c20, 0, 0, 0). */
void sh_render_reset(const ShEnv* e);
/* 0x00486870: audio config cfg: cfg +0 = 0x004a8f34(), the registry key (cfg +0x112C,
 * HKEY_LOCAL_MACHINE) opened (0x004ac528), the settings read (0x004868cc), "UserVolume"
 * (0x004e2cf8, 0x14 bytes) into cfg +0x38 (0x004ac65c), volume applied (0x00486bb4(cfg, 1)). */
void sh_audio_init_user_volume(const ShEnv* e, uint32_t cfg);
/* 0x00473f60 (PS1 roundStartMusic): level setup step 0x00457c80(level), then the music track
 * through the audio interface (0x00626014, vtable +8, method +4): 10 in game type 0, 11 for the
 * Dark Tooth music flag (0x00621181), levels 10 / 11 / 12 -> 8 / 2 / 6, else level + 1. The music
 * volume (0x0046f9dc, at least the minimum 0x00c8507c) is kept in 0x00c85098 and set on the
 * mixer (0x004b8f48(0x00c850bc, v | v << 16), v = vol * 0xffff / 100 capped at 0xffff; from
 * the unclamped volume, 32 bit wrap as the original). */
void sh_round_start_music(const ShEnv* e, uint8_t level);
/* 0x004868c0: audio_apply_volume(cfg, 0). */
void sh_audio_commit(const ShEnv* e, uint32_t cfg);

/* 0x00474154 timer_wait: poll input until min_us (unsigned difference of 0x00493038 timer
 * readings) has passed, then 1 as soon as button 0x12, 0x11, 0x16 or 0x14 is ready; 0 once
 * max_us (when >= 0) has passed. input_poll gets what the original leaves in eax (the start
 * reading, the last reading, or the last button check's result). */
int32_t sh_timer_wait(const ShEnv* e, uint32_t min_us, int32_t max_us);
/* 0x00484fbc (PS1 screenHistoryMovie): fade, the intro movie (0x004e2a8f), fade back; returns
 * the history screen 0x0048429c. */
uint32_t sh_screen_history_movie(const ShEnv* e);
/* 0x00485040 (PS1 screenCredits): credit pages 1..7 ("%s%02d.TIM" 0x004e2ab0 with 0x004e2aa0)
 * shown up to 10 s each; a button ends it. Returns next when that is the history screen
 * (0x0048429c), else the title. */
uint32_t sh_screen_credits(const ShEnv* e, uint32_t next);
/* 0x00485100 (PS1 pair screenScrollText: wrong, this is the match end flow): run the session
 * (0x00475d34); single player / tournament (game types 1, 2): result 1 or 2 back to the title
 * (event 1); otherwise the next event (opponent count from sh_tournament_opponents minus the
 * humans, 0x00480c08), the stop screen (event 4 without the full version flag 0x00624ed0), the
 * ending (0x00485290, game type 1 after event 8) or the co-op win screen. Network games (5..7)
 * go back to the network screen. */
uint32_t sh_screen_match_end(const ShEnv* e);
/* 0x00485290 (PS1 screenEnding): every human's ending movie (TM2.EXE table 0x004e81d0, 12 bytes
 * per car, first word the file name) and, except for car 4, the "you won" movie 0x004e2ae0;
 * then the credits screen 0x00485040. */
uint32_t sh_screen_ending(const ShEnv* e);

#endif /* TM2_SH_SCREEN_H */
