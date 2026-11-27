/*
 * host/qol/qol.h: the opt-in changes (docs/QOL.md) as the host and the game ports ask for them.
 * Port code, not from the original game; host/settings/settings.h has the file behind them.
 * Each answer is read once and kept for the run. One file per switch:
 *   crash_fixes.c  [gameplay] fix_crashes      music_loop.c  [gameplay] loop_music
 *   controller.c   [input] controller          volume.c      [audio] *_volume
 *   hidden_cars.c  [gameplay] unlock_hidden_cars, play_dark_tooth
 *   hidden_arenas.c [gameplay] hidden_arenas
 */
#ifndef TM2_HOST_QOL_QOL_H
#define TM2_HOST_QOL_QOL_H

/* [gameplay] fix_crashes (default 0): guard the original's own crash / hang bugs. A game port
 * may only test it in a small, marked branch that names the original's behaviour. */
int qol_fix_crashes(void);
/* logs (once per id) that a fix_crashes guard changed what the original would have done */
void qol_fixed(const char* id);

/* [gameplay] loop_music (default 0): a CD track that ends starts again at once */
int qol_loop_music(void);

/* [gameplay] unlock_hidden_cars (default 0): Sweet Tooth and Minion on the car select as if
 * their cheat codes had been typed (src/rt/qol_cars.c) */
int qol_unlock_hidden_cars(void);
/* [gameplay] play_dark_tooth (default 0): Dark Tooth (car 14) on his own car select slot, drawn
 * and sized as a car (src/rt/qol_cars.c); part of the network compatibility word */
int qol_play_dark_tooth(void);
/* [gameplay] hidden_arenas (default 0): the network host screen's arena arrows also reach
 * Suicide Swamp, Cyburbia and the TM1 rooftop as if their debug codes had been typed
 * (src/rt/qol_arenas.c) */
int qol_hidden_arenas(void);

/* [input] controller (default 1) and a game controller connected: the game's saved
 * InputJoystickStatus reads as on (host/win32_reg.c) */
int qol_controller_default(void);

/* [audio] master_volume x effects_volume / music_volume as a gain 0..256 (256: as the original),
 * times the game's own mixer levels (host/audio/mixer.h: speakers x wave / speakers x CD) */
int qol_effects_gain(void);
int qol_music_gain(void);

#endif /* TM2_HOST_QOL_QOL_H */
