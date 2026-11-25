/*
 * mt2_icfg_keys.h: the per frame key readers of the input configuration screen's "press a key"
 * mode, ported from the PC machine code of TM2.EXE 0x004605b0 (keyboard, old name
 * inputcfg_process_input) and 0x00460b04 (device 4 / joystick).
 *
 * Globals (emulator addresses, reached through resolve):
 *   0x0050e5c4 i32 keyboard keys taken this round, 0x0050e5c9 u8 copy of the shown code,
 *   0x0050e5ca u8 next code, 0x0050e5cb u8 shown code, 0x0050e5cc u8[256] keyboard code usable,
 *   0x0050e6d4 u8[50] device 4 code usable; widgets 0x0050e428 (accept), 0x0050e42c (cancel),
 *   0x0050e43c (status text), 0x0050e440 (code text); 0x004e7a48 + 8 row the row's key id;
 *   names 0x004e755c (keyboard) / 0x004e797c (device 4); 0x004e7554 / 0x004e7558 sound sequence
 *   counters and the sound object 0x00626010 as in mt2_icfg_dlg.h.
 *
 * Verified against the original under emulation (tools/emu/check_mt2_icfg_keys.sh).
 */
#ifndef MT2_ICFG_KEYS_H
#define MT2_ICFG_KEYS_H

#include <stdint.h>

typedef struct {
    uint8_t* (*resolve)(void* user, uint32_t addr);
    /* a call of original code (Watcom registers, stack arguments); returns eax */
    uint32_t (*call)(void* user, uint32_t fn, uint32_t eax, uint32_t edx, uint32_t ebx, uint32_t ecx,
                     const uint32_t* stack, int nstack);
    /* the two calls that take the original's 256 byte stack buffer: 0x004a559c (eax widget,
     * edx 0, ebx buffer: reads the widget's text into it) and 0x004a91b0 (eax buffer, edx text:
     * compare; the eax argument here is 0); returns eax */
    uint32_t (*call_buf)(void* user, uint32_t fn, uint32_t eax, uint32_t edx, char* buf);
    void* user;
} Mt2IcfgKeys;

/* 0x004605b0: keyboard key reader for control row `row`. */
void mt2_icfg_read_keyboard(const Mt2IcfgKeys* m, int32_t row);
/* 0x00460b04: device 4 code reader for control row `row`. */
void mt2_icfg_read_dev4(const Mt2IcfgKeys* m, int32_t row);

#endif /* MT2_ICFG_KEYS_H */
