/*
 * qol_cars.h: the runtime side of [gameplay] unlock_hidden_cars and play_dark_tooth
 * (host/qol/qol.h, docs/QOL.md). Port code, not from the original game: the bindings of the
 * verified ports call these around them; the ports themselves are unchanged. Each does nothing
 * while its switch is off.
 */
#ifndef TM2_RT_QOL_CARS_H
#define TM2_RT_QOL_CARS_H

#include <stdint.h>

/* screen_car_select 0x0047baa0, before it: unlock_hidden_cars sets the two unlock flags */
void qolc_car_select_enter(void);
/* level_files_load 0x004740a8 (name), after it: play_dark_tooth gives the car select's wheel a
 * 15th slot, Dark Tooth's model (with Sweet Tooth's driver picture) */
void qolc_scene_loaded(uint32_t name);
/* bg_tim_load 0x0046f784 (name): play_dark_tooth: the car info page of car 14 is Sweet Tooth's */
uint32_t qolc_info_tim(uint32_t name);
/* level_load_assets 0x004753a4: before (1) / after (0) */
void qolc_level_load(int begin);
/* dpc_load_relocate 0x004502b0, after it: play_dark_tooth shrinks a human's Dark Tooth model */
void qolc_model_loaded(uint32_t buf);
/* car stats setup 0x0043846c, after it: play_dark_tooth shrinks a human's Dark Tooth body */
void qolc_stats_done(uint32_t stats, uint16_t car_id);
/* car_player_update 0x00415f00, before it: play_dark_tooth keeps a human Dark Tooth's probe
 * step height (car +0x594, 0xA0 from 0x0043a754 at every spawn) at a quarter */
void qolc_player_frame(uint32_t car);
/* screenEnding 0x00485290: play_dark_tooth gives a Dark Tooth winner Sweet Tooth's ending (the
 * original's ending table has no entry for car 14); before (1) / after (0) */
void qolc_ending(int before);
/* save_record_load 0x0046ecb0, after it: a save made as Dark Tooth loads as Sweet Tooth while
 * play_dark_tooth is off */
void qolc_save_loaded(void);

#endif /* TM2_RT_QOL_CARS_H */
