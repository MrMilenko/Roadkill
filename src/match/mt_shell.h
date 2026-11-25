/*
 * mt_shell.h: front end helpers in TM2.EXE 0x0045f080..0x00468774 (input configuration state,
 * unlocked cars, arena picker, game type buttons), ported from the PC machine code.
 *
 *   0x0050e5cc u8[256] shell state table        0x0050e6d4 u8[50] car availability
 *   0x00624ec7 u8 arena (what 0x00474508 game_get_event_type returns), 0x0050fa70 i32 current
 *   arena on the arena screen, 0x00624ea8 u8 game type, 0x0050fa7c / 0x0050fa80 / 0x0050fa84
 *   u32 the game type buttons (objects: +0x34 a table, its +0xd0 the select method)
 *
 * Verified against the original under emulation (tools/emu/check_match_shell.sh).
 */
#ifndef MT_SHELL_H
#define MT_SHELL_H

#include <stdint.h>

typedef struct {
    uint8_t* state_table;       /* 0x0050e5cc */
    uint8_t* cars;              /* 0x0050e6d4 */
    uint8_t* arena;             /* 0x00624ec7 */
    const int32_t* arena_cur;   /* 0x0050fa70 */
    uint8_t* game_type;         /* 0x00624ea8 */
    const uint32_t* buttons;    /* 0x0050fa7c, 0x0050fa80, 0x0050fa84 */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    uint32_t (*save_data)(void* user, uint32_t a);              /* 0x004a81e8 (0x00a82560) */
    uint32_t (*unlocked)(void* user, int32_t what);              /* 0x0044eee4 */
    void (*net_set_type)(void* user, uint32_t net, int32_t type); /* 0x004aaa74 (0x0050f9a8, type) */
    void (*button_select)(void* user, uint32_t fn, uint32_t obj, int32_t on); /* [[obj +0x34] +0xd0](obj, on) */
    void* user;
} MtShell;

/* 0x0045f080: state table all 1, then 40 entries (see .c) 0 */
void mt_inputcfg_init_state_table(const MtShell* s);
/* 0x0045f1e8: car availability cleared, car 0 available; with save data (0x004a81e8): 32 bytes
 * from its +0x24c into cars 1..32, and its words +0x21c..+0x230 unlock pairs 33/34 .. 43/44,
 * +0x218 unlocks 45..48. */
void mt_inputcfg_load_data(const MtShell* s);
/* 0x004685d4 / 0x00468650: next / previous arena from 0x0050fa70: 1..12 wrapping, 9 skipped,
 * 10 / 11 / 12 only when unlocked(0x23 / 0x24 / 0x25). Values below 9 (incl. negative ones for
 * next) are taken as they are. */
void mt_arena_next(const MtShell* s);
void mt_arena_prev(const MtShell* s);
/* 0x004686d4 / 0x00468724 / 0x00468774: game type 7 / 6 / 5 picked with button obj (the other two
 * buttons unselected, in the original's order) */
void mt_game_type_7(const MtShell* s, uint32_t obj);
void mt_game_type_6(const MtShell* s, uint32_t obj);
void mt_game_type_5(const MtShell* s, uint32_t obj);

#endif /* MT_SHELL_H */
