/*
 * mt_extra.c: small leftovers (see mt_extra.h).
 */
#include "mt_extra.h"

#include <string.h>

void mt_display_text2_off(const MtExtra* e, uint32_t eax) {
    if (*e->disp_960) return;
    *e->disp_954 = 0;
    *e->disp_950 = 1;
    e->text_set(e->user, eax, 0x004e207du, 0, 0);
}

void mt_display_text2_on(const MtExtra* e, uint32_t eax) {
    if (*e->disp_960) return;
    *e->disp_950 = 1;
    *e->disp_954 = -1;
    e->text_set(e->user, eax, 0x004e207eu, 0, 0);
}

void mt_net_set_flag(const MtExtra* e, int32_t v) {
    if (v == 1) {
        e->net_on(e->user, 0x0050f9a8u, 0, 1, 0);
        *e->net_flag = 1;
    } else {
        e->net_off(e->user, 0x0050f9a8u, 0, 0, 0);
        *e->net_flag = 0;
    }
}

int32_t mt_page_dispatch(const MtExtra* e, uint32_t obj) {
    e->list_select(e->user, 0x0050ea98u, obj, 0, 0);
    uint32_t mode;
    memcpy(&mode, e->resolve(e->user, obj) + 0x58, 4);
    if (mode == 1) e->page_1(e->user, obj, 0, 0, 0);
    else if (mode == 2) e->page_2(e->user, obj, 0, 0, 0);
    else if (mode == 4) e->page_4(e->user, obj, 0, 0, 0);
    return 0;
}

void mt_list_read_selected(const MtExtra* e, uint32_t widget) {
    uint8_t buf[0x80];
    uint32_t i = e->list_cursel(e->user, widget, 0, 0, 0);
    e->list_text(e->user, widget, i, 0, buf);
}
