/*
 * sh_names.h: game state accessors, level / car display names and the menu text callbacks,
 * ported from TM2.EXE 0x00474500..0x00474e0c, 0x0047770c, 0x00477b2c..0x00477b74.
 *
 * The game state block 0x00624ea8 (reached through ShEnv by game address):
 *   +0x00 u8 game type (5, 6, 7: the modes where a car taken by another player is shown as such)
 *   +0x17 u8[] game mode per index (0x00624ebf)      +0x1F u8 event type (0x00624ec7)
 * Unlock flags 0x009f6c1c u8 (car 12, Sweet Tooth), 0x009f6c1d u8 (car 13, Minion).
 *
 * Names: the original returns pointers to text in TM2.EXE's data. The port returns the same
 * game addresses (picked by code); the text itself is read from the player's TM2.EXE with
 * sh_exe_text, never kept in the source.
 *
 * Verified against the original under emulation (tools/emu/check_shell_misc.sh).
 */
#ifndef TM2_SH_NAMES_H
#define TM2_SH_NAMES_H

#include <stdint.h>
#include "sh_mem.h"
#include "../assets/pe.h"

#define SH_GAME_STATE 0x00624ea8u

/* 0x00474500 / 0x00474508 / 0x00474510 (only al is meaningful in the first two) */
uint8_t sh_game_type(const ShEnv* e);
uint8_t sh_game_event_type(const ShEnv* e);
uint8_t sh_game_mode(const ShEnv* e, int32_t i);
/* 0x00474b8c: "<city> - <arena>" text address of level 1..12 (9 and anything else: the secret
 * level text). The original takes the game state in eax and ignores it. */
uint32_t sh_level_display_name(uint8_t level);
/* 0x00474c1c: the short city name of level 1..12 ("Unknown Level" text otherwise) */
uint32_t sh_level_short_name(uint8_t level);
/* 0x00474d68: car 0..14 name text ("UNKNOWN CAR" otherwise) */
uint32_t sh_car_display_name(uint8_t car);
/* 0x00474e0c: opponents of a tournament level (1..8; 0 for 9..12 and anything else) */
uint8_t sh_tournament_opponents(uint8_t level);
/* 0x0047770c: 0 for car 12 / 13 while its unlock flag is clear, else 1 */
int32_t sh_car_unlocked(const ShEnv* e, int32_t car);
/* 0x00477b2c: the car select text: "Car Selected" in game types 5..7 when car_taken(car) != -1,
 * else the car's name (sh_car_display_name of the low byte) */
uint32_t sh_menu_car_name_text(const ShEnv* e, int32_t car);
/* 0x00477b64: the "Getting Random Car" text */
uint32_t sh_menu_random_car_text(void);
/* 0x00477b74: sh_level_display_name(level + 1) (the original's C returns void, its eax is the name) */
uint32_t sh_menu_level_name_text(uint8_t level);

/* the zero ended text at a TM2.EXE address, or NULL */
const char* sh_exe_text(const PeImage* pe, uint32_t addr);

#endif /* TM2_SH_NAMES_H */
