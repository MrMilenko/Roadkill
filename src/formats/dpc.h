/*
 * dpc.h: DPC model files ("DCPMC"), ported from TM2.EXE.
 *
 * A DPC file is a PlayStation memory image: every internal pointer is a PS1 address. The game
 * loads the file and rewrites those pointers by walking the model node tree:
 *   FUN_004502b0  header check + root list loop (level code at 0x0044f130 has a copy of the loop)
 *   FUN_0049eee0  node tree walk, relocates pointers per node type, marks done nodes with 0xFE
 *   FUN_0049f440  optional texture page shift on type 0 polygon lists
 * Verified byte for byte against the original under emulation (tools/emu/check_dpc.sh).
 *
 * The port keeps pointers as 32 bit values: addr_base + offset into the buffer. Use addr_base 0 to
 * get plain offsets (what the port needs on 64 bit hosts); the test passes the emulator's buffer
 * address so the result is identical to the original's memory.
 *
 * Header (int32 little endian):
 *   [0] 'DCPM'  [1] 'C'  [2] ?  [3] PS1 address the file was saved at (base)
 *   [4] count passed to the original's allocator FUN_004a1314 (meaning not verified)
 *   [5] PS1 address of the root list: { count, node ptr * count }
 */
#ifndef TM2_DPC_H
#define TM2_DPC_H

#include <stddef.h>
#include <stdint.h>

#define DPC_DONE_MARK 0xFE   /* written to node byte +1 once relocated */

typedef struct {
    uint32_t base;           /* header[3] */
    uint32_t alloc_count;    /* header[4] */
    uint32_t root_list;      /* header[5], PS1 address */
} DpcHeader;

/* Returns 0 if buf holds a DPC header (original calls game_shutdown_dispatch otherwise). */
int dpc_read_header(const uint8_t* buf, size_t size, DpcHeader* out);

/* PS1 pointer -> addr_base + offset, exactly as the original: (ptr - base) / 4 * 4 with signed
 * division (truncates toward zero). */
uint32_t dpc_reloc(uint32_t ps1_ptr, uint32_t base, uint32_t addr_base);

/* Relocates the whole file in place (FUN_004502b0 + FUN_0049eee0 + FUN_0049f440).
 * tex_shift: added to each type 0 polygon texture page (low 9 bits); 0 = none.
 * Returns number of root nodes, or negative on error (bad header, pointer outside buffer,
 * node stack overflow). */
int dpc_relocate(uint8_t* buf, size_t size, uint32_t addr_base, int16_t tex_shift);

/* The two steps on their own (the runtime binds the original's addresses to them):
 *   FUN_0049eee0  dpc_relocate_tree: the node tree at root (a relocated pointer) of a file saved at
 *                 PS1 address base, with texture page shift tex_shift (FUN_0049f440 on type 0 nodes)
 *   FUN_0049f440  dpc_shift_tex_pages: the polygons of the type 0 node at node
 * buf / size hold the memory at addr_base (pointers are addr_base + offset). Return 0, or negative
 * for data outside the buffer / no memory. */
int dpc_relocate_tree(uint8_t* buf, size_t size, uint32_t addr_base, uint32_t base, uint32_t root, int16_t tex_shift);
int dpc_shift_tex_pages(uint8_t* buf, size_t size, uint32_t addr_base, uint32_t node, int16_t tex_shift);

#endif /* TM2_DPC_H */
