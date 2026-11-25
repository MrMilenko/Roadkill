/*
 * tpc.h: TPC texture packs ("TCPMC"), ported from TM2.EXE.
 *
 * tpc_register() is the game's registration routine at 0x00496fd8 (old Ghidra name
 * file_get_resource_ptr, wrong). It fills the texture page table (0x00a30c48, 0x30 bytes per page,
 * 512 pages) from a loaded TPC buffer, and may convert palettes in the buffer from 555 to 565.
 * Verified field for field against the original under emulation (tools/emu/check_tpc.sh).
 *
 * Pointers are 32 bit values addr_base + offset into the TPC buffer (addr_base 0 = plain offsets).
 *
 * File: int32 header { 'TCPM', 'C', id, page_count }, then page_count chunks:
 *   +0x00 chunk payload size in bytes (next chunk at +4 + size)
 *   +0x08 flags: bits 0..1 depth, 0x08 has own palette, 0x10, 0x20 shared, 0x40, 0x80
 *   +0x0C without palette: page index (in this file) whose palette to use
 *         with palette: byte offset (low 2 bits ignored) from +0x18 to the size word + pixels
 *   +0x18 palette (256 x 16 bit) when it has one; otherwise the size word, then pixels
 * Size word: width = (v * 2) & 0xFFFF, height = v >> 16.
 */
#ifndef TM2_TPC_H
#define TM2_TPC_H

#include <stddef.h>
#include <stdint.h>

#define TPC_MAX_PAGES 512

/* One texture page, same field order as the original's 0x30 byte table entry. */
typedef struct {
    uint32_t pixels;     /* +0x00 */
    uint32_t width;      /* +0x04 */
    uint32_t height;     /* +0x08 */
    uint32_t clut;       /* +0x0C palette pointer, 0 = none */
    uint32_t flag10;     /* +0x10 (flags & 0x10) != 0, meaning not verified */
    uint32_t clut_moved; /* +0x14 set when palette ownership moved to the linked page */
    uint32_t owner;      /* +0x18 1 unless flags & 0x20 */
    uint32_t has_clut;   /* +0x1C page carries its own palette (flags & 0x08) */
    uint32_t depth;      /* +0x20 flags & 3; set to 9 after 565 palette conversion */
    uint32_t link;       /* +0x24 page index (global) of the palette source */
    uint32_t flag40;     /* +0x28 flags & 0x40 */
    uint32_t flag80;     /* +0x2C flags & 0x80 */
} TpcPage;

typedef struct {
    int32_t page_count;    /* 0x00a36c48 */
    int32_t shared_count;  /* 0x00a36c4c */
    int32_t counter_a;     /* 0x00a36c50 (only reset here) */
    int32_t next_page;     /* 0x00a36c54 first free page index */
    TpcPage pages[TPC_MAX_PAGES];
} TpcTable;

/* Registers one TPC buffer. append = 0 resets the table first (the original's param_2 == 0, which
 * also calls FUN_004b270c to release surfaces: the port leaves that to the caller).
 * convert_565: what the original decides with screen_is_split() == 0 && FUN_00496284() == 1;
 * converts every own palette in buf from 555 to 565 in place.
 * Returns the file id (header[2]) when append == 0, else the first page index used; -1 for a bad
 * header, -2 if the table would overflow, -3 for a chunk outside the buffer. */
int tpc_register(TpcTable* t, uint8_t* buf, size_t size, uint32_t addr_base, int append, int convert_565);

#endif /* TM2_TPC_H */

/* Texture page to RGBA8 (port side, the original converts pages to the video card's formats).
 * Every disc page is 8 bit indexed with a 256 entry 15 bit palette, width = row bytes.
 * Color: PC RGB555 (bits 10..14 red, 5..9 green, 0..4 blue) scaled by 8. The PC files are not
 * PS1 BGR: tpc_register's 555 -> 565 step only fits red on top, and the Direct3D upload
 * 0x004bff08 writes bits 10..14 into byte 0 (peRed) of its PALETTEENTRY.
 * Alpha: 0 for color 0x0000, else 255. CAVEAT: this is the PS1 rule, consistent with the disc
 * (palette index 0 is never 0x0000, about a third of texels map to 0x0000 at other indices) but
 * not found in the PC code. Bit 15 (PS1 semi transparency) is reported through stp_mask.
 * buf/size/addr_base as given to tpc_register (palettes must still be 555, convert_565 = 0).
 * out holds width * height * 4 bytes; stp_mask (optional) width * height bytes.
 * Returns 0, or -1 for an unsupported depth or data outside the buffer. */
int tpc_page_rgba(const TpcPage* p, const uint8_t* buf, size_t size, uint32_t addr_base,
                  uint8_t* out, uint8_t* stp_mask);
