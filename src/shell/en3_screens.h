/*
 * en3_screens.h: front end screens of TM2.EXE 0x00480000..0x00490000 that build their scene from
 * locals (PS1 decomp: screenOptions ...). Runs in the En2Env of src/engine/en2_env.h: memory by
 * game address, every call outside the function by the original's address.
 *
 * A screen keeps its scene objects and commands as locals of one big stack frame and hands their
 * addresses to the scene engine (src/game/scene.h), which keeps the pointers while sceneRun runs.
 * The port keeps that frame at En2Env.scratch, laid out at the original's offsets (F + offset
 * below), so the scene engine gets the same records at the same relative places.
 *
 * Records (field layouts from the writes and the scene engine's ports, src/match/mt_scmd.h):
 *   scene object (item 0x54 bytes, vtable 0x004e5798; decor 0x50 bytes, vtable 0x004e57a0):
 *     +0x00 u16 text / model id   +0x04 u32 choice node (0)   +0x08 u32 vtable
 *     +0x0c u16, +0x0e u16, +0x10 i16 flags   +0x14 i32 1   +0x18 i32 0
 *     +0x1c i32[3] position   +0x28 i32[3] angles   +0x34 i32[4] hot spot (x0, y0, x1, y1)
 *     +0x44 i32 selectable   +0x4c i32 0   +0x50 u32 screen function of the item (items only)
 *   command (+0x00 i32 duration, +0x04 i32 frames left, +0x08 u32 vtable, +0x0c u32 target):
 *     0x004e577c move (scene_cmd_move 0x0046a760): +0x10 i32[3] step
 *     0x004e5790 choice (scene_cmd_choice 0x0046ab34): +0x10 u8 mode, +0x14 i32 value,
 *                +0x18 u32 validator
 *     0x004e575c wait for input (0x0046ace8): +0x10 i32 player, +0x18 i32
 *     0x004e5794 (run 0x00485af0): +0x10 i32 text id, +0x14 / +0x18 i32 0, +0x1c u32 font
 *                (the value of 0x00626010)
 *     0x004e5760 (run 0x0046ac4c): +0x10 u8;  0x004e5770 (run 0x00485900): +0x10 i32
 *     0x004e576c (run 0x004858d0) / 0x004e5768 (run 0x004858a0, menuCmdStepBack): target only
 *
 * Verified against the original under emulation (tools/emu/check_en3_screens.sh).
 */
#ifndef TM2_EN3_SCREENS_H
#define TM2_EN3_SCREENS_H

#include <stdint.h>
#include "../engine/en2_env.h"

#define EN3_SCENE 0x0050fad8u   /* the shell's scene block */

/* 0x004824c8 screenOptions: the options menu (input configuration 0x00484230, audio 0x00484250,
 * history 0x0048429c, two on / off choices 0x00624ed0 / 0x00624ed1, back to the title 0x004780fc).
 * Straight line code: the scene built in a 0x8a0 byte frame, sceneRun, then 0x009f6cd4 = (the
 * item under the cursor is not the input configuration item) and the two choices stored back.
 * Returns sceneRun's result (the next screen function). */
uint32_t en3_screen_options(En2Env* e);

/* 0x004813e8: the opponent line up shown before a match (game modes 0..2, else straight on):
 * background "<0x004e2a1f><arena><0x004e2a17>" by sprintf, the opponents' models (0x2329 + car,
 * up to 9, placed from the table at 0x004e808c by count, each turned by rand() % 10 - 5 degrees),
 * each moved in after the previous one, then a timed wait. Returns 0x00485100 (screenMatchEnd).
 * More than 9 opponents would read past the original's 9 entry table: not supported. */
uint32_t en3_screen_lineup(En2Env* e);

/* 0x0048429c screenHistory: the history menu (the history movie 0x00484fbc, the history text
 * 0x00485040; back to the options 0x004824c8). Straight line code, a 0x574 byte frame.
 * Returns sceneRun's result. */
uint32_t en3_screen_history(En2Env* e);

/* 0x00480c8c screenWorldGlobe: the globe turning from the previous level to the next one
 * (game state 0x00624ea8, level 0x00624ec7). Shown only when 0x00624ed1 is set and level <= 8,
 * else returns 0x004813e8 (the line up) at once. Uses x87 (the marker distance, chopped).
 * Returns sceneRun's result. */
uint32_t en3_screen_world_globe(En2Env* e);

/* 0x00486f18 audio_config_screen (eax = the audio configuration object, sh2_audio.h): the
 * dialog: 6 slider labels and the OK button (label widgets 0x004a4a24), the sound card combo box
 * filled from waveOutGetDevCapsA (a WAVEOUTCAPSA at frame +0x12c), 3 sliders (5 when the mixer
 * answered, cfg +0x1120) with their keyboard navigation links, the volumes applied (mode 2),
 * the dialog run (0x004877e4), every control freed, the overlay 0x1b. Returns 1. */
uint32_t en3_audio_config_screen(En2Env* e, uint32_t cfg);

#endif
