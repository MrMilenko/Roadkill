/*
 * sh4_session.h: the session runner, TM2.EXE 0x00475d34 (PS1 decomp: sessionRun; Ghidra's old
 * name match_run). Called with the game state (0x00624ea8 or a copy, see sh2_flow.h for its
 * fields) and returns the last in-game loop result (signed byte).
 *
 * Setup: input drained (button 7), game mode (0x0045c328), network player count, AI difficulty
 * (+0x28), input / render modes, the level (0x004753a4) and car models (0x00475af8), display
 * mode. Then one round per pass until the "done" flag: music cue for the level (+0x1f), the round
 * (rtMainLoop 0x0046c6fc, result r), music stopped and the CD volume set from 0, then by game type
 * (+0): 0 free play; 1 / 2 tournament (r 1: a life lost, at 0 lives (+0x17) GAME OVER; r 2: quit;
 * else next level, with the minion and Dark Tooth boss stages and the final message); 3 challenge
 * (r 1 lost, 3 won: the result screen with the counts, r 2 quit); 4 and others (r 2 quit, 3 / 4
 * end of round waits); 5..7 network (r 2 quit, 8: the scores table and the next level / round).
 * Messages go through hud_show_message 0x0047506c and wait at least 2 s for a button (0x12, 0x11,
 * 0x16, 0x14) or at most 10 s (2 s, 4..6 s in places). The end restores the display and input.
 *
 * Locals live in the original's frame (fb = its ebp - 0x50, see sh3_screen.h): +4 the game state,
 * +8 done, +0x0c volume temporary, +0x18 the exit loop counter (never initialized: ORIGINAL BUG,
 * the stack's value is used), +0x3c a 3 byte text buffer (from TM2.EXE 0x004e8010), +0x40 i16
 * challenge wins, +0x44 i16 challenge losses, +0x48 u8 r, plus the scores table temporaries.
 *
 * Verified against the original under emulation (tools/emu/check_sh4_session.sh).
 */
#ifndef TM2_SH4_SESSION_H
#define TM2_SH4_SESSION_H

#include "sh3_screen.h"

int32_t sh4_session_run(const ShEnv* e, uint32_t fb, uint32_t gs);

#endif /* TM2_SH4_SESSION_H */
