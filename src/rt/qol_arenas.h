/*
 * qol_arenas.h: the runtime side of [gameplay] hidden_arenas (host/qol/qol.h, docs/QOL.md). Port
 * code, not from the original game: the bindings of the verified ports call these around them;
 * the ports themselves are unchanged. Each does nothing while the switch is off.
 */
#ifndef TM2_RT_QOL_ARENAS_H
#define TM2_RT_QOL_ARENAS_H

#include <stdint.h>

/* pickupCounterTake 0x0044eee4(i), after it: v is what the game's table answered */
uint32_t qola_counter(uint32_t i, uint32_t v);
/* netdlg_host_build 0x00467904: 1 before it, 0 after it */
void qola_host_build(int begin);

#endif /* TM2_RT_QOL_ARENAS_H */
