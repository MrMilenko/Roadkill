/*
 * en_reg.h: the scene draw's small registries and frame state, ported from TM2.EXE:
 *   0x004a129c en_reg_reset        constant animation list and extra model list cleared, queue off
 *   0x004a1430 en_reg_add_anim     animRegister: add a type 3 node to the constant animation list
 *                                  (100 max, no duplicates; full: printf of the string at 0x004e30b4)
 *   0x004a1488 / 0x004a1494        constant animation queue off / on (0x00a81e30)
 *   0x004a14a0 / 0x004a14c0        extra model list (20 slots, drawn by 0x004a0e20): add in the first
 *                                  free slot / remove the first match
 *   0x004a14f0 en_reg_set_lists    0x00a81e3c / 0x00a81e34 = (EAX, EDX)
 *   0x004a1314 en_reg_set_shift    scale shift into 0x004e8448, 0x004e8430, 0x004e8288, 0x004e8f88
 *   0x004a0dd4 en_reg_frame        renderFlipFrame: previous frame flag set 0x004e844c = 0x00a81c38,
 *                                  0x00a81c38 = frame parity [0x00a0364c] & 1, 0x00a81c48 = ticks * 60 / 60
 * Verified under emulation: tools/emu/check_engine_reg.sh.
 */
#ifndef TM2_EN_REG_H
#define TM2_EN_REG_H

#include <stdint.h>

typedef struct {
    uint32_t anims[100];     /* 0x00a81c9c */
    int32_t anim_count;      /* 0x00a81e2c */
    int32_t anim_queue;      /* 0x00a81e30 */
    uint32_t models[20];     /* 0x00a81c4c */
    uint32_t list_a, list_b; /* 0x00a81e3c, 0x00a81e34 */
    int32_t shift[4];        /* 0x004e8448, 0x004e8430, 0x004e8288, 0x004e8f88 */
    int32_t frame_clear;     /* 0x004e844c */
    int32_t frame_set;       /* 0x00a81c38 */
    int32_t ticks;           /* 0x00a81c48 */
    void (*print)(void* user, const char* msg);   /* 0x004a1f1e printf */
    void* user;
} EnReg;

void en_reg_reset(EnReg* r);
void en_reg_add_anim(EnReg* r, uint32_t node);
void en_reg_queue(EnReg* r, int on);
void en_reg_add_model(EnReg* r, uint32_t model);
void en_reg_remove_model(EnReg* r, uint32_t model);
void en_reg_set_lists(EnReg* r, uint32_t a, uint32_t b);
void en_reg_set_shift(EnReg* r, int32_t shift);
/* parity: [0x00a0364c] (frame_parity 0x004931f4), ticks60: frame_get_ticks60 0x004931d4 (i16) */
void en_reg_frame(EnReg* r, int32_t parity, int16_t ticks60);

#endif /* TM2_EN_REG_H */
