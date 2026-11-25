/*
 * sh3_screen.h: shared helpers of the sh3_* front end screen ports (TM2.EXE 0x00478000..
 * 0x00481000: the AI opponent select, car select, level select and menu screens).
 *
 * These screens build their whole scene (image record, objects, commands, the cross pointers
 * between them) in their own stack frame and hand pointers into it to the scene engine. The port
 * keeps that frame as game memory: `fb` is the game address of the original's frame base (its
 * esp after the prologue) and every local is written at fb + the original's offset, so the scene
 * engine (ported in src/game/scene.c and src/match/mt_scene.c) sees the same bytes. Objects are
 * 0x54 byte records (i16 id, +4 node, +8 vtable, ...), commands smaller records with a vtable
 * at +0 or +8; the layout comments name each by its frame offset.
 *
 * Every function the screens call goes through ShEnv.call with the original's address (see
 * sh_mem.h: n arguments = eax, edx, ebx, ecx, then the stack words in order).
 */
#ifndef TM2_SH3_SCREEN_H
#define TM2_SH3_SCREEN_H

#include <stdint.h>
#include "sh_mem.h"

static inline uint32_t sh3_call(const ShEnv* e, uint32_t fn, int n, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    uint32_t a[5] = {a0, a1, a2, a3, a4};
    return e->call ? e->call(e->user, fn, n, a) : 0;
}

#define SH3_SCENE 0x0050fad8u      /* the scene block (src/game/scene.h) */

/* the scene action row of a screen's table: event, source (frame offset, 0 = any), type,
 * command (frame offset), added in order with scene_add_action 0x0046a18c */
typedef struct {
    uint8_t event;
    uint16_t src;
    uint8_t type;
    uint16_t cmd;
} Sh3Action;

static inline void sh3_add_actions(const ShEnv* e, uint32_t fb, const Sh3Action* t, int n) {
    for (int i = 0; i < n; i++)
        sh3_call(e, 0x0046a18cu, 5, SH3_SCENE, t[i].event, t[i].src ? fb + t[i].src : 0, t[i].type, fb + t[i].cmd);
}

#endif /* TM2_SH3_SCREEN_H */
