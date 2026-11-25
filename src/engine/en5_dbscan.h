/*
 * en5_dbscan.h: dbscanHierarchy, TM2.EXE 0x004a15d4 (PS1 dbscanHierarchy): the DPC model node
 * walk. Environment: En2Env (en2_env.h); scratch must hold 0x100 bytes (the walk's locals passed
 * by address live at scratch + their original stack offsets). Verified under emulation:
 * tools/emu/check_en5_dbscan.sh.
 */
#ifndef TM2_EN5_DBSCAN_H
#define TM2_EN5_DBSCAN_H
#include "en2_env.h"

void en5_dbscan_hierarchy(En2Env* e, uint32_t root, uint32_t lvl0, uint32_t cb);

#endif
