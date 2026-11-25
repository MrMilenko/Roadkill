/*
 * cb2_exe_stores.h: runs a straight-line "constant setter" function of the player's TM2.EXE in a
 * tiny x86 register machine and lists the memory stores it makes.
 *
 * Some level setup functions keep their per level data as immediates inside code (mov reg, imm /
 * mov [abs], reg ...), e.g. worldWarpsSetup 0x00453fac and worldCollapseReset 0x00454ae0. The
 * port does not carry those values in its source: it reads them from the player's executable with
 * this walker (rule 4 of docs/PORTING_GUIDE.md).
 *
 * The machine knows only the instructions those functions use (mov / xor / add / sub / cmp / lea /
 * inc / dec / push / pop on registers, byte / word / dword stores, jcc / jmp, the cs: jump table
 * jump, ret). Registers start unknown unless given; reading an unknown register byte, reading
 * memory other than a jump table in the image, or meeting any other instruction is an error, never
 * a guess (another TM2.EXE build fails cleanly).
 */
#ifndef TM2_CB2_EXE_STORES_H
#define TM2_CB2_EXE_STORES_H

#include <stddef.h>
#include <stdint.h>

#include "pe.h"

enum { CB2_EAX, CB2_ECX, CB2_EDX, CB2_EBX, CB2_ESP, CB2_EBP, CB2_ESI, CB2_EDI };

typedef struct {
    uint32_t pc;        /* address of the store instruction */
    uint32_t va;        /* address written */
    uint8_t size;       /* 1, 2 or 4 */
    uint8_t absolute;   /* 1: [disp32] / moffs form (a fixed address); 0: register based */
    uint32_t value;     /* low size bytes are meaningful */
} Cb2Store;

typedef struct {
    int n;
    Cb2Store s[1024];
} Cb2StoreList;

/* Runs the function at va until its final ret. init_known: bit i set = register i starts as
 * init[i]. 0 on success; on failure err gets a reason. */
int cb2_exe_run_stores(const PeImage* pe, uint32_t va, const uint32_t init[8], unsigned init_known,
                       Cb2StoreList* out, char* err, size_t errlen);

#endif /* TM2_CB2_EXE_STORES_H */
