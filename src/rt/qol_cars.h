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
/* after it: play_dark_tooth turns a Sweet Tooth pick (car 12) into Dark Tooth (car 14) */
void qolc_car_select_leave(void);
/* shellSetUnlockFlagInit 0x0047770c: play_dark_tooth lets the wheel stop on the Sweet Tooth slot */
int qolc_car_unlocked(int32_t car, int original);
/* menuCarNameText 0x00477b2c: play_dark_tooth labels the Sweet Tooth slot "Dark Tooth" */
uint32_t qolc_car_name_text(int32_t car, uint32_t original);
/* dpc_load_relocate 0x004502b0, after it: play_dark_tooth shrinks the human's Dark Tooth model */
void qolc_model_loaded(uint32_t buf);
/* car stats setup 0x0043846c, after it: play_dark_tooth shrinks the human's Dark Tooth body */
void qolc_stats_done(uint32_t stats, uint16_t car_id);
/* car_player_update 0x00415f00, before it: play_dark_tooth keeps the human's Dark Tooth's probe
 * step height (car +0x594, 0xA0 from 0x0043a754 at every spawn) at a quarter */
void qolc_player_frame(uint32_t car);
/* screenEnding 0x00485290: play_dark_tooth gives a Dark Tooth winner Sweet Tooth's ending (the
 * original's ending table has no entry for car 14); before (1) / after (0) */
void qolc_ending(int before);

#endif /* TM2_RT_QOL_CARS_H */
