/*
 * gp_nullsubs.h: empty functions of the gameplay range (a single RET in TM2.EXE).
 *
 *   0x00422e04 gp_car_bind_human_slot_stub   (PS1 pair carBindHumanSlotStub)
 *   0x0042a018 gp_car_bind_ai_slot_stub      (PS1 pair carBindAiSlotStub)
 *   0x00435c50 gp_hud_nullsub                (also the shared RET of 0x00435c54's epilogue)
 *
 * Verified against the original under emulation (tools/emu/check_gameplay_nullsubs.sh): no write,
 * all registers but none changed, one instruction.
 */
#ifndef TM2_GP_NULLSUBS_H
#define TM2_GP_NULLSUBS_H

void gp_car_bind_human_slot_stub(void);
void gp_car_bind_ai_slot_stub(void);
void gp_hud_nullsub(void);

#endif /* TM2_GP_NULLSUBS_H */
