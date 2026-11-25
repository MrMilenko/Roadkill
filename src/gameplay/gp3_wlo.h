/*
 * gp3_wlo.h: worldLoadObject, TM2.EXE 0x0044f20c..0x0045029b (PS1 decomp pair: worldLoadObject in
 * world_load_object.c), ported from the PC machine code.
 *
 * Called for every object record of a level's object list: eax = object type, edx = record
 * (a handle here: an address in the game's memory, read and written through `resolve`), ebx = a
 * third value only handed on (the level / collision context). A compare tree on the type (no jump
 * table: 128 jbe / 94 jb, unsigned, after one signed test) picks what is done with the record:
 *
 *   type >= 9000 (signed): 0x00469b60(rec). Negative types (huge unsigned) do nothing.
 *   "entity" types (0x10 0x20 0x30 0x40 0x50 0x60 0x70 0x80 0x90 0xa0 0xb0 0xc0 0xd0 0xe0, and
 *     0xf0): e = entity_alloc 0x00492d90(), entity_list_insert 0x00492c4c(e), 0x00492de4(e, rec),
 *     0x00457428(e, type); 0xa0 also 0x00447dbc(rec, 0xa0); last record 0x0050001c = rec (not
 *     for 0xf0).
 *   1: scene root 0x00492c44() gets the record (0x00492de4), once: flag 0x00500014.
 *   2: entity (not inserted) + 0x00494c3c(e), e+0x0c = 0.  3: entity, e+0 = 0xffe, e+0x0c = 0.
 *   A: 0x004706c4(rec)  B: 0x00453c5c(type, rec)  C: 0x0046ff40(rec, type)  D: 0x00453f50(type,
 *     rec)  E: 0x00470964(type, last record)  F: 0x00499b9c(rec, 2): the small types 0x12..0xf8
 *     cycle through these in blocks of 16 (see gp3_wlo.c).
 *   0x191 0x193 0x194 0x196 0x19b 0x19d 0x1a1..0x1a3 0x1ae: rec+0 u8 == 1 / 4: unless vehicle
 *     mode 0x0045c338() == 4 and type 0x1a2 (then rec+0x14 / +0x12 u8 = 0), rec+0x16 / +0x10 i16 =
 *     counter 0x00500020, 0x00449888(ctx, type, counter), counter++, 0x00470770(rec, type).
 *   0x1f4..0x1fe, 0x207, 0x208: rec+4 u16 = collision counter 0x00500024 (old value kept),
 *     C, 0x00470bc0(type, rec+4 u16, ctx), counter++, then 0x1fc: 0x004724c4(rec), 0x1fd:
 *     0x0047274c(), 0x1fe: 0x00472a14(rec, old), 0x1fa / 0x1fb: entity + 0x00455ec0(e, 0 / 1).
 *   0x202 0x204 0x205: C, 0x004712c8(type, i16 rec+4 sign extended, rec).
 *   0x28a..0x299, 0x29e..0x2b5, 0x4ec..0x50d, 0x8a0..0x916, 0xbbb..0xc1f, 0xc81..0xc84,
 *     0xdaf / 0xdb0, 0xfa0..0xfa9, 5000, 5001, 5100: the single calls listed in gp3_wlo.c.
 *   Everything else: nothing.
 *
 * Every call to other code goes through `call` with the original's register arguments (handles
 * as values); entity_alloc (no arguments) returns a handle that `resolve` maps for the two
 * entity field writes. Argument counts follow the callees' register use (checked for
 * entity_alloc, which ignores eax, and 0x0046ff40, which ignores ebx).
 *
 * Verified against the original under emulation (tools/emu/check_gp3_wlo.sh).
 */
#ifndef GP3_WLO_H
#define GP3_WLO_H

#include <stdint.h>

typedef struct {
    uint32_t* scene_root_done;    /* 0x00500014 */
    uint32_t* last_record;        /* 0x0050001c */
    int32_t* object_counter;      /* 0x00500020 */
    int32_t* collision_counter;   /* 0x00500024 */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    /* a call to TM2.EXE function `addr` with its register arguments (unused ones 0) */
    uint32_t (*call)(void* user, uint32_t addr, uint32_t a, uint32_t b, uint32_t c);
    void* user;
} Gp3WloEnv;

void gp3_world_load_object(const Gp3WloEnv* env, int32_t type, uint32_t rec, uint32_t ctx);

#endif /* GP3_WLO_H */
