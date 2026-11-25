/*
 * en_ot.h: the ordering table between the scene engine and the renderer, ported from TM2.EXE
 *
 *   0x00496680 en_ot_insert     add a record to a bucket (software: append at the bucket's tail;
 *                               hardware: push in front, as 0x004966cc)
 *   0x004967f4 en_add_prim      PC AddPrim: software: lock, command_dispatch 0x004ad874, unlock;
 *                               hardware: 0x004963b4
 *   0x004963b4 en_prim_queue    hardware: the 3D device path 0x004b67d0(0x00c84840, prim);
 *                               else lock, dispatch, unlock
 *   0x0049681c en_ot_flush      software: lock, dispatch every record of buckets 0..0xffe in order,
 *                               unlock; hardware: 0x00496380
 *   0x00496380 en_ot_flush_hw   hardware: 0x004b6780(0x00c84840) (the device's own flush); else
 *                               0x0049681c
 * Verified under emulation: tools/emu/check_engine_ot.sh.
 *
 * "hardware" is [0x00c85008] != 0 (read by 0x004b5f60; 0x004958ac, old name screen_is_split,
 * is the same test). It is not split screen: it selects the 3D device renderer (0x004b67d0 takes
 * the 0x80 records of 0x00491018 and feeds a COM device, vtable +0x5c ...) instead of the
 * software PlayStation GPU (command_dispatch).
 *
 * Ordering table (PS1 style): slot i at ot + 4 * i holds the first record of bucket i; each
 * record's first dword links to the next; a bucket's last record links to slot i + 1 (an empty
 * slot holds the address of the next slot). Software inserts also keep a tail per bucket:
 * descriptor d = [0x00a2dbb0]: [d] = ot, d + 0x4004 + 4 * i = address of the bucket's last link.
 */
#ifndef TM2_EN_OT_H
#define TM2_EN_OT_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t* mem;             /* memory holding the table, its descriptor and the records */
    uint32_t base;            /* address of mem[0] */
    size_t size;
    uint32_t desc;            /* [0x00a2dbb0] */
    int32_t hw;               /* [0x00c85008] */
    void (*lock)(void* user);                 /* 0x004961b8 / 0x004b4418(0x00c84840, 0, 0, 0, 0) */
    void (*unlock)(void* user);               /* 0x004961d4 / 0x004b44e4(0x00c84840, 0, 0) */
    void (*dispatch)(void* user, uint32_t prim);   /* 0x004ad874 command_dispatch */
    void (*hw_prim)(void* user, uint32_t prim);    /* 0x004b67d0 */
    void (*hw_flush)(void* user);                  /* 0x004b6780 */
    void* user;
} EnOtEnv;

void en_ot_insert(EnOtEnv* e, uint32_t slot, uint32_t rec);
void en_add_prim(EnOtEnv* e, uint32_t prim);
void en_prim_queue(EnOtEnv* e, uint32_t prim);
void en_ot_flush(EnOtEnv* e, uint32_t ot);
void en_ot_flush_hw(EnOtEnv* e, uint32_t ot);

#endif /* TM2_EN_OT_H */
