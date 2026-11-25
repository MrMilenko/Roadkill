/*
 * cb_invis.h: the invisibility special, TM2.EXE 0x0044bbe4 (PS1 weaponInvisibilityUpdate).
 *
 * For every human car j (0x0045afb4) with u8 +0x84C set (object 0x0045ae24(j), car 0x0045afbc(j);
 * t = i16 +0x87C, type = i16 +0x90):
 *   t == 360: carBodyObjectHide(type) (0x00453c38); 25 < t <= 335: object +8 = 0 (invisible);
 *   t <= 25 with object +8 == 0: carBodyObjectShow(type) (0x00453c00), object +8 = 1;
 *   t -= ticks60 (0x004931d4, 16 bit); then while fading (t >= 335 or t <= 25): sparkle: p =
 *   math_jitter_pos(object + 0xE8, 0x333, 0x333, 0x333) (0x0044a40c), sound 0x16 at the object
 *   (prio 99; split screen locates with view -1, the pan is used either way), fxSparkBlue(p, 1)
 *   (0x0045075c); t <= 0: +0x84C = 0.
 *
 * Verified against the original under emulation (tools/emu/check_combat_invis.sh).
 */
#ifndef TM2_CB_INVIS_H
#define TM2_CB_INVIS_H

#include <stdint.h>

#include "weapons/wmem.h"

typedef struct {
    const WMem* mem;
    int32_t (*human_count)(void* user);                       /* 0x0045afb4 */
    uint32_t (*human_object)(void* user, int32_t i);          /* 0x0045ae24 */
    uint32_t (*human_get)(void* user, int32_t i);             /* 0x0045afbc */
    void (*body_hide)(void* user, int32_t type);              /* 0x00453c38 */
    void (*body_show)(void* user, int32_t type);              /* 0x00453c00 */
    int32_t (*ticks60)(void* user);                           /* 0x004931d4 */
    uint32_t (*jitter)(void* user, uint32_t pos, int32_t dx, int32_t dy, int32_t dz);  /* 0x0044a40c */
    int32_t (*split)(void* user);                             /* 0x0046cc54 */
    void (*locate)(void* user, uint32_t pos, int32_t view);   /* 0x0046f974 / 0x0046f980 */
    int32_t (*pan)(void* user);                               /* 0x0046f990 */
    int32_t (*volume)(void* user);                            /* 0x0046f988 */
    void (*play)(void* user, int32_t id, int32_t vol, int32_t pan, int32_t prio);      /* 0x0046f8b0 */
    void (*spark_blue)(void* user, uint32_t pos, int32_t flag);                        /* 0x0045075c */
    void* user;
} CbInvisEnv;

void cb_invisibility_update(const CbInvisEnv* e);

#endif /* TM2_CB_INVIS_H */
