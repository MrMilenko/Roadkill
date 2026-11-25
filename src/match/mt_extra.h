/*
 * mt_extra.h: small leftovers in TM2.EXE 0x00462d34..0x0046ec80, ported from the PC machine code.
 * Other code is reached through one callback shape (registers eax, edx, ebx; pointers to the
 * caller's stack as host pointers in the ptr argument).
 *
 * Verified against the original under emulation (tools/emu/check_match_extra.sh).
 */
#ifndef MT_EXTRA_H
#define MT_EXTRA_H

#include <stdint.h>

typedef uint32_t (*MtExtraFn)(void* user, uint32_t eax, uint32_t edx, uint32_t ebx, void* ptr);

typedef struct {
    const int32_t* disp_960;    /* 0x0058a960 */
    int32_t* disp_950;          /* 0x0058a950 */
    int32_t* disp_954;          /* 0x0058a954 */
    int32_t* net_flag;          /* 0x0050fa24 */
    uint8_t* (*resolve)(void* user, uint32_t handle);
    MtExtraFn text_set;         /* 0x004a6f08 (eax, text) */
    MtExtraFn net_on;           /* 0x004aaa88 (0x0050f9a8, ebx 1) */
    MtExtraFn net_off;          /* 0x004aaacc (0x0050f9a8) */
    MtExtraFn list_select;      /* 0x004a3964 (0x0050ea98, obj) */
    MtExtraFn page_1;           /* 0x004622fc (obj) */
    MtExtraFn page_2;           /* 0x004627c4 (obj) */
    MtExtraFn page_4;           /* 0x00462500 (obj) */
    MtExtraFn list_cursel;      /* 0x004a54cc (widget) */
    MtExtraFn list_text;        /* 0x004a559c (widget, index, buffer ptr) */
    void* user;
} MtExtra;

/* 0x0046ec54 / 0x0046ec80: unless 0x0058a960: 0x0058a950 = 1, 0x0058a954 = 0 / -1,
 * text_set(eax, 0x004e207d / 0x004e207e). */
void mt_display_text2_off(const MtExtra* e, uint32_t eax);
void mt_display_text2_on(const MtExtra* e, uint32_t eax);
/* 0x004671e8: v == 1: net_on, 0x0050fa24 = 1; else net_off, 0x0050fa24 = 0. */
void mt_net_set_flag(const MtExtra* e, int32_t v);
/* 0x00462d34 (obj): list_select(0x0050ea98, obj), then by obj +0x58: 1 page_1, 2 page_2,
 * 4 page_4 (obj). Returns 0. */
int32_t mt_page_dispatch(const MtExtra* e, uint32_t obj);
/* 0x00469b34 (widget): the selected entry's text read into a 0x80 byte stack buffer (unused). */
void mt_list_read_selected(const MtExtra* e, uint32_t widget);

#endif /* MT_EXTRA_H */
