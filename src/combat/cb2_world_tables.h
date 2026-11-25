/*
 * cb2_world_tables.h: the two world setup functions whose data lives as immediates in code, with
 * that data read from the player's TM2.EXE at runtime (never copied into this source).
 *
 * worldWarpsSetup, TM2.EXE 0x00453fac (PS1 decomp: worldWarpsSetup), eax = event (arena) number.
 *   Warp records, 3 x 0x28 bytes at 0x00502ea0 (see src/world/warps.h):
 *     +0x00 u8 active  +0x02 u8 link / kind byte  +0x04 i16 frame counter
 *     +0x08 i32[3] entry position  +0x14 i32[3] destination  +0x20 u32 car (handle)
 *   First, for each record: +0x00 = 0 (byte), +0x04 = 0 (word), +0x20 = 0 (dword).
 *   Then, for events 1..8 ((unsigned)(event - 1) <= 7, a jump table at 0x00453f8c), a block of
 *   constant stores into the records (+0x02, +0x08..+0x1C of records 0 and 1; event 8 also record
 *   2). Event 2 and every other value write nothing more. Fields a case does not name keep their
 *   old contents. The function returns nothing (eax is left holding a scratch value).
 *
 * worldCollapseReset, TM2.EXE 0x00454ae0 (PS1 decomp: worldCollapseReset), no arguments.
 *   Collapse section table, 35 x 0x24 bytes at 0x00502988, and the state after it at 0x00502e74
 *   (see src/world/collapse.h). First constant stores to the state (+0x00 busy byte, +0x08..+0x18;
 *   +0x10 timer starts at a negative value). Then for every section: +0x00 object = 0, +0x04 = 0,
 *   +0x08 = 0, +0x0C = 0, +0x10 count = 1, +0x18 armed = 0, +0x19 = 0, +0x1A = 0, the six edge
 *   links +0x1C..+0x21 = -1. Then the ice arena's section graph as constant stores: per section
 *   counts (+0x10) and edge links. The original writes these in register allocation order; the
 *   port applies them in that same order.
 *
 * The values come from cb2_exe_run_stores (src/assets/cb2_exe_stores.h): the extractor runs the
 * original code in a register machine and keeps its stores to fixed addresses; the register based
 * stores (the clear loops) are the code ported below and are checked against it.
 *
 * Verified against the original under emulation (tools/emu/check_cb2_world_tables.sh).
 */
#ifndef TM2_CB2_WORLD_TABLES_H
#define TM2_CB2_WORLD_TABLES_H

#include <stddef.h>
#include <stdint.h>

#include "../assets/pe.h"

#define CB2_WARPS_VA 0x00502ea0u
#define CB2_WARPS_SIZE (3 * 0x28)
#define CB2_COLLAPSE_VA 0x00502988u
#define CB2_COLLAPSE_SIZE (35 * 0x24 + 0x1c)   /* table + state up to 0x00502e90 */

typedef struct {
    uint16_t off;       /* from the block start */
    uint8_t size;       /* 1, 2 or 4 */
    uint32_t value;
} Cb2Put;

typedef struct {
    int n;
    Cb2Put put[64];
} Cb2PutList;

typedef struct {
    Cb2PutList level[8];        /* events 1..8 */
} Cb2WarpsTable;

typedef struct {
    Cb2PutList pre;             /* before the section loop (the state block) */
    int n;                      /* after it (the section graph) */
    Cb2Put put[512];
} Cb2CollapseTable;

/* Extraction from an opened TM2.EXE. 0 on success, else err gets a reason. */
int cb2_warps_table_extract(const PeImage* pe, Cb2WarpsTable* t, char* err, size_t errlen);
int cb2_collapse_table_extract(const PeImage* pe, Cb2CollapseTable* t, char* err, size_t errlen);

/* Reads the file and extracts both. 0 on success. */
int cb2_world_tables_load(const char* exe_path, Cb2WarpsTable* warps, Cb2CollapseTable* collapse,
                          char* err, size_t errlen);

/* 0x00453fac: records = the 0x78 bytes at 0x00502ea0 */
void cb2_world_warps_setup(uint8_t* records, int32_t event, const Cb2WarpsTable* t);

/* 0x00454ae0: block = the 0x508 bytes at 0x00502988 (section table + state) */
void cb2_world_collapse_reset(uint8_t* block, const Cb2CollapseTable* t);

#endif /* TM2_CB2_WORLD_TABLES_H */
