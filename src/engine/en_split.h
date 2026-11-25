/*
 * en_split.h: split screen primitive build, ported from TM2.EXE 0x00491018 (old name
 * display_cmd_build; the split screen counterpart of render_submit_nonplit 0x0048fd54, see
 * en_submit.h), and the ordering table link 0x004966cc (depth_sort_insert).
 * Verified under emulation: tools/emu/check_engine_split.sh.
 *
 * In split screen the polygon renderer 0x0048e9b8 calls this instead of 0x0048fd54; the records
 * are queued in the ordering table and drawn by the flush 0x0049681c. Records:
 *
 *   2 vertices, shade count 0x009f7d34 <= 1: 0x10 byte line
 *     +4 u32 color 0x009f7d54 (its top byte then overwritten by) +7 code 0x40,
 *     +8 i16 x0, +0xa y0, +0xc x1, +0xe y1 (fistp of the screen floats, low 16 bits)
 *   2 vertices, shade count > 1: 0x14 byte gouraud line
 *     +4 u32 color 0 (0xff000000 | r << 16 | g << 8 | b, each truncated from the vertex floats),
 *     +7 code 0x50, +8 x0, +0xa y0, +0xc u32 color 1, +0x10 x1, +0x12 y1
 *   else: 0x80 polygon, 0x14 byte header + 0x20 per vertex
 *     +4 u16 render flags (0x00c846d8, | 0x1000 when the shade count > 1), +6 u8 count,
 *     +7 0x80 (| 2 when semi transparent, 0x009f7d38), +8 i32 texture page (page ref >> 16,
 *     textured only), +0xc u32 0x009f7d7c (blend mode from the polygon record)
 *     vertex: +0 sx, +4 sy (raw, negative values first clamped to 0 in the vertex itself),
 *     +8 z, +0xc w (1/z), +0x10 color (0x3c000000 with render flag 2; else the flat color, or the
 *     vertex color with alpha 0xff / 0x80 when gouraud), +0x14 0, +0x18 / +0x1c u, v scaled to
 *     0..1 (textured only: u * 1 / (w * 4096); pages with +0x10 set and +0x2c clear get the half
 *     texel inset ((w - 1) / w) / (w * 4096) and + 0.5 / w)
 * Textured with a shade count > 0 first sets the flat color 0x009f7d54 to 0xffffffff
 * (0xc0ffffff when semi transparent).
 */
#ifndef TM2_EN_SPLIT_H
#define TM2_EN_SPLIT_H

#include <stdint.h>

#include "en_vertex.h"
#include "en_submit.h"

typedef struct {
    const EnClipBuf* clip;   /* 0x004e8290 (the vertices' sx / sy may be clamped in place) */
    uint32_t shade;          /* 0x009f7d34 shading vertex count (polygon record byte 1), unsigned */
    uint32_t color;          /* 0x009f7d54 (may be rewritten, see above) */
    int32_t semi;            /* 0x009f7d38 semi transparency */
    uint32_t blend;          /* 0x009f7d7c */
    uint16_t render_flags;   /* 0x00c846d8 (gets 0x1000 when shade > 1) */
    uint32_t page_ref;       /* 0x009f7e30 (high 16 bits, signed: texture page) */
    int32_t depth;           /* 0x009f7d58 */
    int32_t depth_fixed;     /* 0x009f7d48 */
    int32_t depth_bias;      /* 0x009f7d50 */
    /* texture page table 0x00a30c48, 0x30 bytes per page (+4 width, +8 height, +0x10 and
     * +0x2c flags read here); pages outside [0, tex_count) read as zeros */
    const uint8_t* tex_table;
    int32_t tex_count;

    /* primitive memory: records are written at prim + (cursor - prim_addr) */
    uint8_t* prim;
    uint32_t prim_addr;
    uint32_t cursor;         /* 0x009f7d78 (advanced) */
    uint32_t end;            /* 0x009f7cd4 end of the 0x80 record (written by that path) */
    uint32_t rec;            /* 0x009f7cd0 (0x40), 0x009f7cf0 (0x50), 0x009f7cd8 (0x80) */

    /* 0x004966cc with the slot ot_base + 4 * bucket (en_ot_link does what it does) */
    void (*ot_insert)(void* user, uint32_t slot, uint32_t rec);
    uint32_t ot_base;        /* *param_1: the ordering table */
    void* user;
} EnSplitEnv;

void en_split_prim(EnSplitEnv* e);

/* Entry point for the polygon renderer port: the same build with EnSubmitEnv inputs.
 * Read from EnSubmitEnv: clip, color, depth, depth_fixed, depth_bias, render_flags, page_ref,
 * prim, prim_addr, cursor; written back: color, render_flags, cursor, rec. ot_insert is called
 * with (bucket, record) like en_submit_prim, but in split screen the original's insert is
 * 0x004966cc (en_ot_link on the slot *param_1 + 4 * bucket), not 0x00496680.
 * Globals not in EnSubmitEnv are in EnSplitExtra. */
typedef struct {
    uint32_t shade;          /* 0x009f7d34 */
    int32_t semi;            /* 0x009f7d38 */
    uint32_t blend;          /* 0x009f7d7c */
    const uint8_t* tex_table;   /* 0x00a30c48 */
    int32_t tex_count;
    uint32_t end;            /* out: 0x009f7cd4 (0x80 records only; not EnSubmitEnv's end 0x009f7cc8) */
} EnSplitExtra;

void en_split_build(EnSubmitEnv* s, EnSplitExtra* x);

/* 0x004966cc: *rec = *slot; *slot = rec_addr (push onto the bucket's list) */
void en_ot_link(uint32_t* slot, uint32_t* rec, uint32_t rec_addr);

#endif /* TM2_EN_SPLIT_H */
