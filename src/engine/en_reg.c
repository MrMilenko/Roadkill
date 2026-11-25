/*
 * en_reg.c: scene draw registries and frame state (see en_reg.h).
 */
#include "en_reg.h"

#include <string.h>

void en_reg_reset(EnReg* r) {
    r->anim_queue = 0;
    r->anim_count = 0;
    memset(r->anims, 0, sizeof r->anims);
    memset(r->models, 0, sizeof r->models);
}

void en_reg_add_anim(EnReg* r, uint32_t node) {
    if (node == 0) return;
    if (r->anim_count >= 100) {
        if (r->print) r->print(r->user, "Can't add: Limit of constant animations is execeeded \n");
        return;
    }
    for (int32_t i = 0; i < r->anim_count; i++)
        if (r->anims[i] == node) return;
    r->anims[r->anim_count++] = node;
}

void en_reg_queue(EnReg* r, int on) { r->anim_queue = on ? 1 : 0; }

void en_reg_add_model(EnReg* r, uint32_t m) {
    for (int i = 0; i < 20; i++)
        if (r->models[i] == 0) { r->models[i] = m; return; }
}

void en_reg_remove_model(EnReg* r, uint32_t m) {
    for (int i = 0; i < 20; i++)
        if (r->models[i] == m) { r->models[i] = 0; return; }
}

void en_reg_set_lists(EnReg* r, uint32_t a, uint32_t b) { r->list_b = b; r->list_a = a; }

void en_reg_set_shift(EnReg* r, int32_t s) { for (int i = 0; i < 4; i++) r->shift[i] = s; }

void en_reg_frame(EnReg* r, int32_t parity, int16_t ticks60) {
    r->frame_clear = r->frame_set;
    r->frame_set = parity & 1;
    r->ticks = (int32_t)ticks60 * 60 / 60;
}
