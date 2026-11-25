/*
 * mt2_netui.h: the network setup screen (provider list, player name / callsign, buttons), ported
 * from the TM2.EXE machine code: 0x004653d4 (build), 0x004658f0 (free), 0x00465cd4 (list
 * selection), 0x00465a3c / 0x00465b7c (the two connect buttons). PC only (no PS1 counterpart).
 *
 * Widgets (handles): 0x0050f9f0 provider list, 0x0050f9f4 name, 0x0050f9f8 callsign,
 * 0x0050f9fc / 0x0050fa00 / 0x0050fa04 buttons. Widget +0x34: vtable (+0 free, +0xb0 / +0xb4 /
 * +0xb8 colors, +0xcc / +0xd4 list callbacks, +0xe0 click callback). Other globals: 0x0050f9dc
 * session handle (+0x40 u32 icon flag), 0x0050f9d4 provider enumeration, 0x0050f9e4 / 0x0050f9e8
 * screen result, 0x0050f9ec selected provider (+4 its name), 0x004e7ae8 / 0x004e7aec default name
 * / callsign (string handles).
 *
 * Every callee goes through one dispatcher, `call(user, addr, args, n)`: addr is the original's
 * function address (for a vtable call: the function pointer read from the vtable; for Win32 calls
 * through the import table: the import slot, 0x00c90318 lstrcmpA, 0x00c90524 MessageBoxA);
 * args = eax, edx, ebx, ecx, then the stack arguments in order. Strings and function addresses
 * of the original are passed as their TM2.EXE addresses (constants, no game text here); local
 * buffers as host pointers (text 0x80 bytes, registry key object 4 bytes, size word 4 bytes).
 * Callees: 0x004ac4b0 reg_open(&key, 0), 0x004ac528 reg_open_subkey(&key, root, 0),
 * 0x004ac5e4 reg_read_data(&key, name, buf, &size), 0x004ac638 reg_write(&key, name, buf),
 * 0x004ac590 reg_flush(&key), 0x004a8f34 widget_mgr_begin, 0x004a8f48 widget_mgr_alloc(size),
 * 0x004a5290 / 0x004a5f40 / 0x004a490c constructors (panel / sprite / text button),
 * 0x004a4bdc, 0x004a4de0 colors, 0x004a70ac nav links, 0x004a8e6c game_loop_run,
 * 0x004a6ee4 set property, 0x004a6e0c, 0x004a6f80 set focus, 0x004a6f08 set label,
 * 0x004a6ef4 get text(w, buf, 0x80), 0x004a5630, 0x004a54b4, 0x004a54cc, 0x004a5394,
 * 0x004a559c item text(w, i, buf), 0x004a6dd8, 0x004a6e2c, 0x004ace88, 0x004988b4 / 0x004988d0
 * set name / callsign(session, buf), 0x004aa5b8 (net), 0x004ace8c list providers(enum, list),
 * 0x00496184 release input, 0x00493748 input poll, 0x004936ac input check(key, 0), 0x0049ecf4
 * key down(scan code), 0x004a5470 find item(list, 0, text), 0x004a56f8 select item(list, i), 0x004988c8 / 0x004988e0 name / callsign, 0x00499884
 * register window, 0x004a91b0 strcmp, 0x004aa524 select provider(net, provider).
 *
 * Verified against the original under emulation (tools/emu/check_mt2_netui.sh).
 */
#ifndef MT2_NETUI_H
#define MT2_NETUI_H

#include <stdint.h>

typedef struct {
    const uint32_t* session;    /* 0x0050f9dc */
    const uint32_t* enumr;      /* 0x0050f9d4 */
    uint32_t* result;           /* 0x0050f9e4 */
    uint32_t* result2;          /* 0x0050f9e8 */
    uint32_t* provider;         /* 0x0050f9ec */
    uint32_t* w;                /* 0x0050f9f0..0x0050fa04: the six widgets */
    const uint32_t* defaults;   /* 0x004e7ae8, 0x004e7aec */
    int32_t* first;             /* 0x004e7ae4: providers not listed yet */
    uint32_t* bdfd20;           /* 0x00bdfd20..0x00bdfd3b: +0 / +4 / +0x14 / +0x18 cleared on entry */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    uint32_t (*call)(void* user, uint32_t addr, const uintptr_t* args, int n);
    void* user;
} Mt2NetUi;

/* 0x004653d4: build the screen: the six widgets (alloc 0x7c / 0x68 / 0x68 / 0x1cc x3; the name /
 * callsign fields start with the session's strings unless they are empty (lstrcmpA with the
 * empty strings 0x004e1406 / 0x004e1407), then the defaults), colors, callbacks (0x00465a3c,
 * 0x00465b7c, 0x00465cbc, 0x00465cd4), the stored name and callsign from the registry
 * (NetPlayerName 0x004e1408, NetPlayerCallsign 0x004e1416 under 0x80000002), nav links, one
 * game loop, property 5 on all, focus on button 0x0050f9fc if 0x004a6e0c says so, else 0x0050fa04.
 * The original never checks an alloc result before the vtable calls (a 0 widget crashes it). */
void mt2_netui_build(const Mt2NetUi* u);
/* 0x004658f0: 0x004a5630(list), then every non zero widget freed (vtable +0, 2). */
void mt2_netui_free(const Mt2NetUi* u);
/* 0x00465cd4 (list widget): with a selection inside the provider count: both connect buttons
 * enabled and redrawn, 0x0050f9ec = the selected provider, its text fetched. */
void mt2_netui_select(const Mt2NetUi* u, uint32_t list);
/* 0x00465a3c (result 2, icon 0x004e14e1) / 0x00465b7c (result 3, icon 0x004e14e9): store result,
 * copy the name and callsign into the session, register the window, take the selected provider
 * (a message box when it is the serial one, 0x004e1428), select it and save its name
 * (NetProviderName 0x004e14d1). The registry object is flushed only when a provider is set. */
void mt2_netui_connect(const Mt2NetUi* u, uint32_t result2, uint32_t icon);

/* 0x00465110: the network setup screen: builds it (0x004653d4), then until 0x0050f9e4 == 1:
 * buttons off while the list is empty; the first time (0x004e7ae4) list the providers (none: a
 * message box, result 1 / 0) and select the stored NetProviderName (0x004e13d6); input poll;
 * input 0x14 / 0x15 and keys 0x2e / 0x24 / 0x31 set the result (0x0050f9e8 = 0 / 1, 0 / 2 / 3).
 * Result 0 / 1: 0x004aa5b8(net); else the name and callsign go to the registry (NetPlayerName
 * 0x004e13e6, NetPlayerCallsign 0x004e13f4). Frees the screen, returns 0x0050f9e8. The ports of
 * 0x004653d4 / 0x00465cd4 / 0x004658f0 above are called directly. */
uint32_t mt2_netui_run(const Mt2NetUi* u);

#endif /* MT2_NETUI_H */
