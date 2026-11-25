/*
 * mt_inputcfg.c: input configuration screen main loop and widget teardown (see mt_inputcfg.h).
 */
#include "mt_inputcfg.h"

#include <string.h>

#define W(addr) (c->widgets[((addr) - 0x0050e100u) / 4u])

static void destroy(const MtInputCfg* c, uint32_t w) {
    if (!w) return;
    uint32_t t, fn;
    memcpy(&t, c->resolve(c->user, w) + 0x34, 4);
    memcpy(&fn, c->resolve(c->user, t), 4);
    c->widget_call(c->user, fn, w, 2);
}

void mt_inputcfg_destroy_widgets(const MtInputCfg* c) {
    static const uint32_t ws[6] = {0x0050e108u, 0x0050e10cu, 0x0050e414u, 0x0050e41cu, 0x0050e420u, 0x0050e418u};
    for (int k = 0; k < 6; k++) destroy(c, W(ws[k]));
    for (uint32_t off = 0; off != 0x100; off += 0x10) {
        destroy(c, W(0x0050e120u + off));
        destroy(c, W(0x0050e220u + off));
    }
    c->list_free(c->user, 0x0050e4f8u);
}

void mt_inputcfg_destroy_car_panel(const MtInputCfg* c) {
    for (uint32_t a = 0x0050e428u; a <= 0x0050e440u; a += 4) destroy(c, W(a));
    c->list_free(c->user, 0x0050e444u);
    static const int32_t rect[4] = {0xa0, 0x4d, 0x1df, 0x1ab};
    c->set_rect(c->user, rect);
    c->invalidate(c->user, 0, 0, 1);
}

int32_t mt_inputcfg_main(const MtInputCfg* c) {
    c->screen_begin(c->user);
    c->create_widgets(c->user);
    mt_inputcfg_init_state_table(c->shell);
    memset(c->slots, 0, 8);
    for (int k = 0; k < 5; k++) c->slots[k] = 1;
    int32_t n = (uint8_t)c->num_players(c->user);
    if (n > 0) c->slots[5] = 1;
    if (n > 1) c->slots[6] = 1;
    if (n > 2) c->slots[7] = 1;
    mt_inputcfg_load_data(c->shell);
    *c->exit_flag = 0;
    *c->result = 0;
    do {
        c->input_poll(c->user);
        if ((uint8_t)c->key_state(c->user, 0x14, 0)) *c->exit_flag = 1;
        if ((uint8_t)c->key_state(c->user, 0x15, 0)) *c->exit_flag = 1;
    } while (*c->exit_flag != 1);
    mt_inputcfg_destroy_widgets(c);
    return *c->result;
}

static void device(const MtInputCfg* c, int32_t player, uint32_t waddr, uint32_t list) {
    int32_t mode = (int8_t)c->device_mode(c->user, player);
    uint32_t w = W(waddr), t, fn;
    memcpy(&t, c->resolve(c->user, w) + 0x34, 4);
    memcpy(&fn, c->resolve(c->user, t) + 0xd0, 4);
    c->widget_call(c->user, fn, w, mode == 1);
    int32_t on_w, on_l;
    if (mode == 2) { on_w = 0; on_l = 0; }
    else if (mode == 0) { on_w = 1; on_l = 0; }
    else if (mode == 1) { on_w = 1; on_l = 1; }
    else return;
    c->widget_enable(c->user, W(waddr), on_w);
    for (uint32_t off = 0; off != 0x100; off += 0x10) c->widget_enable(c->user, W(list + off), on_l);
}

void mt_inputcfg_device_left(const MtInputCfg* c) { device(c, 1, 0x0050e108u, 0x0050e120u); }
void mt_inputcfg_device_right(const MtInputCfg* c) { device(c, 4, 0x0050e10cu, 0x0050e220u); }

static int busy(const MtInputCfg* c) {
    if (*c->busy == 0) return 0;
    if (W(0x0050e440u)) c->busy_click(c->user, W(0x0050e440u));
    return 1;
}

static uint32_t method(const MtInputCfg* c, uint32_t w, uint32_t off) {
    uint32_t t, fn;
    memcpy(&t, c->resolve(c->user, w) + 0x34, 4);
    memcpy(&fn, c->resolve(c->user, t) + off, 4);
    return fn;
}

#define BYTE_AT(addr) (((uint8_t*)c->widgets)[(addr) - 0x0050e100u])

void mt_inputcfg_accept(const MtInputCfg* c) {
    if (busy(c)) return;
    uint32_t w = W(0x0050e108u);
    c->set_device(c->user, 1, c->widget_get(c->user, method(c, w, 0xc8), w) == 1);
    w = W(0x0050e10cu);
    c->set_device(c->user, 4, c->widget_get(c->user, method(c, w, 0xc8), w) == 1);
    for (int k = 0; k < 16; k++) {
        uint32_t key = c->keytab[8 * k];
        c->key_set_a(c->user, key, BYTE_AT(0x0050e11du + 0x10u * (uint32_t)k));
        c->key_set_b(c->user, key, BYTE_AT(0x0050e21du + 0x10u * (uint32_t)k));
    }
    c->keys_save(c->user);
    *c->exit_flag = 1;
}

void mt_inputcfg_defaults_left(const MtInputCfg* c) {
    if (busy(c)) return;
    c->keys_default_a(c->user);
    for (int k = 0; k < 16; k++) {
        uint8_t v = (uint8_t)c->key_get_a(c->user, c->keytab[8 * k]);
        c->text_set(c->user, W(0x0050e120u + 0x10u * (uint32_t)k), c->names_left[v]);
        BYTE_AT(0x0050e11du + 0x10u * (uint32_t)k) = v;
    }
    mt_inputcfg_device_left(c);
    c->render_mode(c->user, 0, 0);
}

void mt_inputcfg_defaults_right(const MtInputCfg* c) {
    if (busy(c)) return;
    c->keys_default_b(c->user);
    for (int k = 0; k < 16; k++) {
        uint8_t v = (uint8_t)c->key_get_b(c->user, c->keytab[8 * k]);
        c->text_set(c->user, W(0x0050e220u + 0x10u * (uint32_t)k), c->names_right[v]);
        BYTE_AT(0x0050e21du + 0x10u * (uint32_t)k) = v;
    }
    mt_inputcfg_device_right(c);
}

void mt_inputcfg_device_button(const MtInputCfg* c, uint32_t widget) {
    c->widget_call(c->user, method(c, widget, 0xd0), widget, 1);
    mt_inputcfg_device_left(c);
    if (busy(c)) return;
    c->message_box(c->user, c->main_window(c->user), 0x004e0a89u, 0x004e0a7bu, 0x30);
    c->input_poll(c->user);
    c->input_update(c->user);
    c->input_poll(c->user);
    c->input_update(c->user);
    c->busy_click(c->user, widget);
}

void mt_inputcfg_device_toggle_right(const MtInputCfg* c, uint32_t w) {
    if (busy(c)) return;
    uint32_t st = c->widget_get(c->user, method(c, w, 0xc8), w);
    if (st == 0) {
        c->widget_call(c->user, method(c, w, 0xd0), w, 1);
        if ((uint8_t)c->device_mode(c->user, 4) == 0) c->set_device(c->user, 4, 1);
    } else if (st == 1) {
        c->widget_call(c->user, method(c, w, 0xd0), w, 0);
        if ((uint8_t)c->device_mode(c->user, 4) == 1) c->set_device(c->user, 4, 0);
    }
    mt_inputcfg_device_right(c);
    for (int k = 0; k < 16; k++)
        c->text_set(c->user, W(0x0050e220u + 0x10u * (uint32_t)k), c->names_right[BYTE_AT(0x0050e21du + 0x10u * (uint32_t)k)]);
    c->input_poll(c->user);
    c->input_update(c->user);
}

static void rebind(const MtInputCfg* c, uint32_t w, int right) {
    if (busy(c)) return;
    uint32_t list = right ? 0x0050e220u : 0x0050e120u;
    int32_t idx = -1;
    for (int32_t k = 0; k < 16; k++)
        if (w == W(list + 0x10u * (uint32_t)k)) idx = k;
    uint32_t off = (uint32_t)idx << 4;
    if (!right) c->dlg[3] = 0;
    uint32_t o = W(list + off);
    c->widget_color(c->user, method(c, o, 0xb0), o, 0xff00);
    c->widget_redraw(c->user, W(list + off), 0, 0);
    c->dlg_setup(c->user, right ? 4 : 1, idx);
    if (right) {
        c->dlg[0] = 1;
        c->dlg[2] = 0;
        c->dlg[1] = 1;
    } else {
        c->dlg[2] = 0;
        c->dlg[0] = 1;
        c->dlg[1] = 1;
    }
    do {
        c->screen_begin(c->user);
        if (right) c->dlg_step_right(c->user, idx);
        else c->dlg_step_left(c->user, idx);
    } while (c->dlg[1] != 0);
    const uint8_t* src = (const uint8_t*)c->dlg + (c->dlg[2] ? 0x11 : 0x10);
    uint32_t key = c->keytab[8 * idx];
    if (right) c->key_set_b(c->user, key, *src);
    else c->key_set_a(c->user, key, *src);
    c->text_set(c->user, W(list + off), (right ? c->names_right : c->names_left)[*src]);
    BYTE_AT(list - 3u + off) = *src;
    c->dlg[0] = 0;
    mt_inputcfg_destroy_car_panel(c);
    c->list_redraw(c->user, 0x0050e4f8u, 0, 0, 0);
    static const int32_t rect[4] = {0xa0, 0x4d, 0x1df, 0x1ab};
    c->set_rect(c->user, rect);
    c->invalidate(c->user, 0, 0, 1);
    c->input_poll(c->user);
    c->input_update(c->user);
    if (right) c->refresh_right(c->user, 0, 0);
    else c->render_mode(c->user, 0, 0);
    c->busy_click(c->user, w);
}

void mt_inputcfg_rebind_left(const MtInputCfg* c, uint32_t w) { rebind(c, w, 0); }
void mt_inputcfg_rebind_right(const MtInputCfg* c, uint32_t w) { rebind(c, w, 1); }

static void paint(const MtInputCfg* c, uint32_t w, int32_t rgb) {
    c->widget_color(c->user, method(c, w, 0xb0), w, rgb);
    c->widget_redraw(c->user, w, 0, 0);
}

static void mark(const MtInputCfg* c, int32_t index, int32_t check, int right) {
    uint32_t list = right ? 0x0050e220u : 0x0050e120u;
    uint32_t code = list - 3u, dup = list - 2u;
    uint32_t status = W(0x0050e43cu);
    if (!check) {
        for (uint32_t k = 0; k < 16; k++) BYTE_AT(dup + 0x10u * k) = 0;
        for (uint32_t i = 0; i < 16; i++)
            for (uint32_t j = i + 1; j < 16; j++)
                if (BYTE_AT(code + 0x10u * i) == BYTE_AT(code + 0x10u * j) && BYTE_AT(code + 0x10u * i)) {
                    BYTE_AT(dup + 0x10u * i) = 1;
                    BYTE_AT(dup + 0x10u * j) = 1;
                }
        for (uint32_t k = 0; k < 16; k++) paint(c, W(list + 0x10u * k), BYTE_AT(dup + 0x10u * k) ? 0xff : 0);
        return;
    }
    c->status_clear(c->user, W(0x0050e43cu));
    int found = 0;
    for (int32_t k = 0; k < 16; k++) {
        uint8_t nc = ((const uint8_t*)c->dlg)[0x11];
        uint32_t w = W(list + 0x10u * (uint32_t)k);
        if (nc == BYTE_AT(code + 0x10u * (uint32_t)k) && nc) {
            if (k == index) continue;
            char buf[0x100];
            strcpy(buf, (const char*)c->resolve(c->user, right ? 0x004e09edu : 0x004e098au));
            uint32_t name;
            memcpy(&name, c->resolve(c->user, 0x004e7a44u + 8u * (uint32_t)k), 4);
            strcat(buf, (const char*)c->resolve(c->user, name));
            c->status_set(c->user, W(0x0050e43cu), buf);
            paint(c, w, 0xff);
            found = 1;
        } else {
            if (k == index) continue;
            paint(c, w, BYTE_AT(dup + 0x10u * (uint32_t)k) ? 0xff : 0);
        }
    }
    if (found) return;
    char buf[0x100];
    c->status_get(c->user, status, 0, buf);
    if (c->str_compare(c->user, buf, (const char*)c->resolve(c->user, right ? 0x004e09fcu : 0x004e0999u)))
        c->status_set(c->user, W(0x0050e43cu), (const char*)c->resolve(c->user, right ? 0x004e0a0au : 0x004e09a7u));
}

void mt_inputcfg_mark_left(const MtInputCfg* c, int32_t index, int32_t check) { mark(c, index, check, 0); }
void mt_inputcfg_mark_right(const MtInputCfg* c, int32_t index, int32_t check) { mark(c, index, check, 1); }
