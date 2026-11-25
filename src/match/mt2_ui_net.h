/*
 * mt2_ui_net.h: network session dialog pieces in TM2.EXE 0x004670fc..0x00469a8c (the host and
 * join screens built on the shell's Win32 widgets), ported from the PC machine code.
 *
 * Globals (one block 0x0050f9a8..0x0050fb00, handed over as g; handles = the original's addresses):
 *   0x0050f9a8 the network session object (passed by address), 0x0050f9d8 u32 session record,
 *   0x0050f9dc u32 player record (its name at +0x44), 0x0050fa44 u32 player list view.
 *   Host screen: widgets 0x0050fa74 name, 0x0050fa78, 0x0050fa7c / 0x0050fa80 / 0x0050fa84 the
 *   three game modes, 0x0050fa88, 0x0050fa8c, 0x0050fa94, 0x0050fa98, 0x0050fa9c, picture
 *   0x0050fa90 (method table at +0x30).
 *   Join screen: 0x0050faa0 i32 done, 0x0050faa4 i32 result, 0x0050faa8 / 0x0050faac u32 chosen
 *   session, widgets 0x0050fab4, 0x0050fab8 (session list), 0x0050fabc, 0x0050fac4, 0x0050fac8,
 *   0x0050facc, 0x0050fad0, picture 0x0050fac0 (+0x30).
 *   0x00624ec7 u8 arena (tournament) id, 0x00624ea8 u8 game type.
 * Widgets: +0x34 method table (entry 0 with 2 destroys); pictures: +0x30 method table
 * (+0x0c free, +0x1c move (x, y, 0)).
 *
 * Every call goes through one callback: call(user, fn, n, args): fn is the original callee's
 * address (direct calls, and for imports the import's address in the table below) or, for calls
 * through a method table, the table entry's value; args are the register arguments in the
 * order eax, edx, ebx, ecx and then the stack arguments; buffers of the port are passed as
 * host pointers.
 *   imports: IsIconic 0x00c90500, ShowWindow 0x00c90584, InvalidateRect 0x00c904f8 (the
 *   addresses of their import table slots)
 *
 * Verified against the original under emulation (tools/emu/check_mt2_ui_net.sh).
 */
#ifndef MT2_UI_NET_H
#define MT2_UI_NET_H

#include <stddef.h>
#include <stdint.h>

typedef uint32_t (*Mt2UiCallFn)(void* user, uint32_t fn, int n, const uintptr_t* args);

typedef struct {
    uint8_t* g;              /* 0x0050f9a8 .. 0x0050fb00 */
    uint8_t* game_type;      /* 0x00624ea8 */
    uint8_t* arena;          /* 0x00624ec7 */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    Mt2UiCallFn call;
    void* user;
} Mt2UiNet;

/* 0x004670fc (text a, text b): a row in the player list 0x0050fa44: i = count (0x004a63d8),
 * insert (0x004a64c4: list, a, i, 0), column 1 = b, column 2 = 0x004e168b, 0x004a625c(list, i, 0) */
void mt2_ui_net_list_add(const Mt2UiNet* n, uint32_t a, uint32_t b);
/* 0x00467160: the same with column 2 = player record 0x0050f9dc + 0x44 (0x004988e0 inlined:
 * it is "add eax, 0x44; ret") */
void mt2_ui_net_list_add_player(const Mt2UiNet* n, uint32_t a, uint32_t b);
/* 0x00468850: when the main window is minimized: restore it (ShowWindow 9), reset the renderer
 * (0x00485398), move the picture, 0x00495b14, repaint the join widgets; then 0x0050faa4 = 0,
 * 0x0050faa0 = 1, 0x004abd38(session record). 0x00468988: the same without the last part. */
void mt2_ui_net_join_restore_cancel(const Mt2UiNet* n);
void mt2_ui_net_join_restore(const Mt2UiNet* n);
/* 0x004692bc: the join screen's widgets destroyed (list navigation reset first), picture freed */
void mt2_ui_net_join_destroy(const Mt2UiNet* n);
/* 0x004699c4: join button: name from widget 0x0050fab4 to the session record, join
 * 0x004aa7dc(0x0050f9a8, chosen); joined: arena / game type from the session record; failed:
 * result 0, done 1 */
void mt2_ui_net_join_go(const Mt2UiNet* n);
/* 0x00469a8c (item): session list selection changed; ends with 0x004693cc (the info panel) */
void mt2_ui_net_join_select(const Mt2UiNet* n, uint32_t item);
/* 0x004693cc: the join screen's info panel 0x0050fac4 (rows 0..4: name, arena, mode, max
 * players, players) for the session chosen in list 0x0050fab8, and its arena picture: a row is
 * rewritten (0x004a5430 clear, 0x004a55f4 set) only when its text (0x004a559c) differs
 * (0x004a91b0); no valid choice: row 0 "None Selected", picture back to entry 0. Session record:
 * +0x14 i32 max players, +0x18 i32 players, +0x20 name, +0x1a0 i32 mode (7 endurance,
 * 6 challenge, 5 tournament), +0x1a4 i32 arena (its low byte, signed, names the arena:
 * 0x00474c1c(0x00624ea8, id)). 0x0050fab0 i32 arena shown. Pictures: SHELLDB3 table 0x004e7b64
 * (13 entries) through file_build_path 0x00499408(out, name, 1), the picture reloaded (+0x0c,
 * +0x00 (pic, path, 0), +0x1c (pic, 0x148, 0x52, 0)), SetRect (0x00c90574) (0x148, 0x52, 0x276,
 * 0x136) invalidated (0x004a6e2c(0x00bdfcd0, rect, 1)).
 * Strings the original passes by address are passed as host pointers (resolve of the address);
 * "Max Players: " 0x004e1924 / "Num Players: " 0x004e1932 + 0x004b27b8 (itoa, base 10). */
void mt2_ui_net_join_info(const Mt2UiNet* n);
/* 0x004684ec: host screen's start button: empty session name (widget 0x0050fa74): MessageBoxA
 * (0x00c90524) "Session must have a name..." 0x004e171f / "No Session Name" 0x004e170f, 0x30;
 * else 0x0050fa68 = 1, 0x0050fa6c = 4, name and widget 0x0050fa78's text to the session record,
 * host (0x004aa690); game type from the record, arena = 0x0050fa70; host failed: 0x004aa638,
 * 0x0050fa6c = 0 */
void mt2_ui_net_host_start(const Mt2UiNet* n);
/* 0x00468aa4: the join screen: builds it (0x00468db8), registers 0x00468850 / 0x00468988 as the
 * session record's handlers (+0x254 / +0x258 setters 0x004ab72c / 0x004ab734), enumerates
 * sessions into list 0x0050fab8, then runs frames until 0x0050faa0 = 1 (key 0x14: cancel,
 * 0x15: join; handlers set it too): list selection, info panel (0x004693cc), name, restore when
 * minimized. Cancelled (result 0 or 1): handler cleared, 0x004aa5b8. Widgets destroyed
 * (0x004692bc); returns 0x0050faa4. */
uint32_t mt2_ui_net_join_run(const Mt2UiNet* n);
/* 0x00468db8: builds the join screen: picture 0x0050fac0 (0x004a8f48 alloc 0xb4,
 * 0x004a2e60, +0x30 = table 0x004e50d8, SHELLDB3 entry 0 loaded and moved), the name field
 * 0x0050fab4 (sprite 0x004a5f40), the session list 0x0050fab8 and two panels (0x004a5290),
 * three buttons (0x004a490c: join, 0xb6, 0xb7), colors, keys (0x004a4bdc), click handlers
 * (method +0xe0: 0x004699c4, 0x00469a60, 0x00469a74; list +0xcc / +0xd4: 0x00469a8c, panel:
 * 0x00469b34), navigation links (0x004a70ac), 0x004a8e6c, property 5 on all, focus, join button
 * disabled. ORIGINAL BUG: an allocation failure (0) is stored and then used (crash). */
void mt2_ui_net_join_build(const Mt2UiNet* n);
/* 0x00467904: builds the host screen: name field 0x0050fa74 (text: the session record's name
 * (0x004ac040) unless it is empty (lstrcmpA 0x00c90318 with 0x004e16e7, the empty string:
 * DECOMPILER TRAP, Ghidra shows it as "NetSessionName" + 0xf): then the default 0x004e7afc), field 0x0050fa78, mode buttons 0x0050fa7c..0x0050fa84, arena arrows
 * 0x0050fa88 / 0x0050fa8c, arena picture 0x0050fa90 (SHELLDB3 SC table 0x004e7b20 by the signed
 * arena byte; arenas 10..12 need unlocks 0x1f and 0x23 / 0x24 / 0x25 (0x0044eee4), anything else
 * above 9 becomes 1), 0x0050fa70 = arena, buttons 0x0050fa94..0x0050fa9c; mode 7 set
 * (0x004aaa74), then the registry's NetSessionName (0x004e16e8) / NetGameMode (0x004e16f7, kept
 * when 5, 6 or 7: game type too); the current mode's button checked (+0xd0); navigation, keys,
 * handlers (+0xe0: 0x004686d4, 0x00468724, 0x00468774, 0x004684ec, 0x004684d8, 0x004684c0,
 * 0x004685d4, 0x00468650), property 5, focus on 0x0050fa94. A negative arena byte (<= 9, kept)
 * indexes before the table (original behavior; not exercised). */
void mt2_ui_net_host_build(const Mt2UiNet* n);
/* 0x00467740: the host screen: builds it (0x00467904), then runs frames until 0x0050fa68 = 1
 * (key 0x14: cancel (0x0050fa6c = 0), 0x15: 0x0050fa6c = 1; the start button 0x004684ec sets 4):
 * the arena picture follows the arena byte (InvalidateRect(0, 0, 0)), the arena and both
 * fields go to the session record. Result 0 / 1: 0x004aa638; else the name is saved as
 * "NetSessionName" (0x004e16d8, 0x004ac638). Screen closed (0x004682c8); returns 0x0050fa6c.
 * The arena byte indexes the picture table 0x004e7b20 unchecked (0..12 are valid). */
uint32_t mt2_ui_net_host_run(const Mt2UiNet* n);
/* 0x004682c8: host screen closed: game mode saved under "NetGameMode" (registry key
 * HKEY_LOCAL_MACHINE: 0x80000002), host widgets destroyed, picture freed */
void mt2_ui_net_host_destroy(const Mt2UiNet* n);

#endif /* MT2_UI_NET_H */
