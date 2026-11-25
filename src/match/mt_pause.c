/*
 * mt_pause.c: the in game pause menu (see mt_pause.h).
 */
#include "mt_pause.h"

#include <stdint.h>

#define DD 0x00c84840u
#define CALL(f, a, b, c) p->f(p->user, (uint32_t)(a), (uint32_t)(b), (uint32_t)(c), 0, 0)

static void detail(const MtPause* p, int32_t level, int32_t v97c, uint32_t on, uint32_t fbits, int set_cursor) {
    int32_t step;
    *p->detail = level;
    if ((uint8_t)CALL(display_ok, 0, 0, 0) == 1 && *p->v_c84f70 != 0)
        step = 2;
    else if (level == 0)
        step = 0;
    else
        step = *p->arena == 0xb ? 2 : 1;
    CALL(set_step, 0xc, 0, step);
    CALL(fx_48e97c, v97c, 0, 0);
    CALL(fx_4ad21c, on, 0, 0);
    CALL(fx_48e918, on, 0, 0);
    uint32_t st[1] = {fbits};
    p->fx_48e920(p->user, 0, 0, 0, st, 1);
    if (set_cursor) *p->cursor = 3;
}

void mt_pause_menu(const MtPause* p) {
    const uint8_t* in = p->input;
    if ((uint8_t)CALL(is_singleplayer, 0, 0, 0)) {
        *p->open = 0;
        return;
    }
    if (in[0]) {
        if (*p->released) {
            if (*p->open == 1) {
                if (*p->may_close == 1) *p->open = 0;
            } else {
                *p->may_close = 1;
                *p->cursor = 0;
                *p->page = 0;
                *p->open = 1;
            }
            *p->released = 0;
        }
    } else {
        if (*p->open == 0) *p->released = 1;
        if (*p->released == 0 && in[0] == 0) *p->released = 1;
    }
    if (*p->open == 0) return;
    if (*p->consumed) {
        if (in[1] || in[2] || in[3] || in[4]) return;
        *p->consumed = 0;
        return;
    }
    if (in[1]) {
        *p->consumed = 1;
        *p->cursor = (int32_t)((uint32_t)*p->cursor - 1);
        if (*p->cursor < *p->cursor_min) *p->cursor = *p->cursor_max;
    }
    if (in[2]) {
        *p->consumed = 1;
        *p->cursor = (int32_t)((uint32_t)*p->cursor + 1);
        if (*p->cursor > *p->cursor_max) *p->cursor = *p->cursor_min;
    }
    if (in[3]) {
        *p->consumed = 1;
        *p->cursor = (int32_t)((uint32_t)*p->cursor + 1);
        if (*p->cursor > *p->cursor_max) *p->cursor = *p->cursor_min;
    }
    if (!in[4]) return;
    *p->consumed = 1;
    uint32_t page = (uint32_t)*p->page, cur = (uint32_t)*p->cursor;
    switch (page) {
    case 0:
        switch (cur) {
        case 0: *p->page = 1; *p->cursor = 0; break;
        case 1: *p->page = 2; *p->cursor = 0; break;
        case 2: *p->page = 3; *p->cursor = 3; break;
        case 3: *p->page = 4; *p->cursor = 0; break;
        }
        break;
    case 1:
        switch (cur) {
        case 0: CALL(view_grow, 0, 8, 0); break;
        case 1: CALL(view_grow, 0, 0xfffffff8u, 0); break;
        case 2: {
            int32_t w = (int32_t)CALL(screen_width, 0, 0, 0);
            int32_t q = (w / 4) / 8;
            CALL(view_set, 0, (uint32_t)(-q) * 8u, 0);
            break;
        }
        case 3: CALL(view_set, 0, 0, 0); break;
        case 4: *p->page = 0; *p->cursor = 0; break;
        }
        break;
    case 2:
        switch (cur) {
        case 0: {
            int32_t n = (int32_t)CALL(mode_count, 0, 0, 0);
            int32_t i = (int32_t)CALL(mode_get, DD, 0, 0);
            if ((int32_t)((uint32_t)i + 1) < n) CALL(mode_set, DD, (uint32_t)i + 1, 0);
            break;
        }
        case 1: {
            CALL(mode_count, 0, 0, 0);
            int32_t i = (int32_t)CALL(mode_get, DD, 0, 0);
            if ((int32_t)((uint32_t)i - 1) >= 0) CALL(mode_set, DD, (uint32_t)i - 1, 0);
            break;
        }
        case 2: {
            int32_t n = (int32_t)CALL(mode_count, 0, 0, 0);
            CALL(mode_get, DD, 0, 0);
            if (0 < n) CALL(mode_set, DD, 0, 0);
            break;
        }
        case 3:
            *p->cursor = (int32_t)CALL(mode_get, DD, 0, 0);
            *p->page = 5;
            break;
        case 4: *p->page = 0; *p->cursor = 0; break;
        }
        break;
    case 3:
        switch (cur) {
        case 0: detail(p, 0, 0x12c, 1, 0x459c4000u, 1); break;
        case 1: detail(p, 1, 0x1f4, 1, 0x45ea6000u, 1); break;
        case 2: detail(p, 2, 0x10000, 0, 0x461c4000u, 1); break;
        case 3: *p->page = 0; *p->cursor = 0; break;
        }
        break;
    case 4:
        if (cur == 0) {
            *p->page = 0;
            *p->cursor = 0;
        } else if (cur == 1) {
            CALL(schedule_event, 2, 0, 0);
        }
        break;
    case 5:
        if ((int32_t)cur == *p->back_item) {
            *p->page = 2;
            *p->cursor = 0;
        } else {
            CALL(mode_set, DD, cur, 0);
        }
        break;
    case 6:
        CALL(schedule_event, 2, 0, 0);
        break;
    }
}

#define FONT 0x00510dd4u

static void color(const MtPauseDraw* d, int hl) {
    uint32_t st[1] = {hl ? 0u : 0xffu};
    d->font_color(d->user, FONT, 0xff, 0xff, st, 1);
}

static void text(const MtPauseDraw* d, uint32_t s, int32_t x, int32_t y) {
    uint32_t st[1] = {(uint32_t)y};
    d->font_draw(d->user, FONT, s, (uint32_t)x, st, 1);
}

static uint32_t u32at(const MtPauseDraw* d, uint32_t h) {
    uint32_t v;
    const uint8_t* p = d->resolve(d->user, h);
    v = (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
    return v;
}

void mt_pause_draw_items(const MtPauseDraw* d, uint32_t table, int32_t dy) {
    int32_t y = (int32_t)((uint32_t)*d->menu_y + (uint32_t)dy);
    if (*d->cursor_max < 0) return;
    int32_t k = 0;
    do {
        color(d, k == *d->cursor);
        text(d, u32at(d, table), *d->menu_x, y);
        uint32_t val = u32at(d, table + 4);
        if (val) {
            int32_t w = (int32_t)d->screen_width(d->user, 0, 0, 0, 0, 0);
            int32_t tw = (int32_t)d->font_width(d->user, FONT, val, 0, 0, 0);
            text(d, u32at(d, table + 4), (int32_t)((uint32_t)w - (uint32_t)tw - 0x14u), y);
        }
        table += 8;
        k++;
        y = (int32_t)((uint32_t)y + (uint32_t)*d->line_h);
    } while (k <= *d->cursor_max);
}

void mt_pause_draw_modes(const MtPauseDraw* d) {
    int32_t y = *d->menu_y, k = 0;
    uint32_t name = 0x00510a88u;
    if (*d->cursor_max > 0) {
        do {
            color(d, k == *d->cursor);
            text(d, name, *d->menu_x, y);
            k++;
            name += 0x50;
            y = (int32_t)((uint32_t)y + (uint32_t)*d->line_h);
        } while (k < *d->cursor_max);
    }
    color(d, *d->cursor == *d->back_item);
    text(d, *d->back_label, *d->menu_x, y);
}

void mt_pause_draw(const MtPauseDraw* d) {
    static const uint32_t title[7] = {0x004e1b04u, 0x004e1b15u, 0x004e1b26u, 0x004e1b39u, 0x004e1b48u, 0x004e1b57u, 0x004e1b6eu};
    static const uint32_t items[5] = {0x004e7bbcu, 0x004e7bdcu, 0x004e7c04u, 0x004e7c2cu, 0x004e7c4cu};
    static const int32_t maxv[5] = {3, 4, 4, 3, 1};
    if (*d->open == 0) return;
    d->font_scale(d->user, FONT, 1, 1, 0, 0);
    {
        uint32_t st[1] = {0x40};
        d->font_color(d->user, FONT, 0xff, 0x40, st, 1);
    }
    uint32_t page = (uint32_t)*d->page;
    if (page > 6) return;
    if (page < 5) {
        if (page == 0) {
            *d->cursor_max = maxv[page];
            *d->cursor_min = 0;
        } else {
            *d->cursor_min = 0;
            *d->cursor_max = maxv[page];
        }
        text(d, title[page], 0x14, 0xf);
        mt_pause_draw_items(d, items[page], 0);
    } else if (page == 5) {
        *d->cursor_max = *d->back_item;
        *d->cursor_min = 0;
        text(d, title[5], 0x14, 0xf);
        mt_pause_draw_modes(d);
    } else {
        *d->cursor_min = 0;
        *d->cursor_max = 0;
        text(d, title[6], 0x14, 0xf);
        text(d, 0x004e1b86u, 0x14, (int32_t)((uint32_t)*d->line_h + 0xfu));
        mt_pause_draw_items(d, 0x004e7c5cu, *d->line_h);
    }
}

int32_t mt_pause_keys(const MtPauseKeys* k) {
    const MtPause* p = k->menu;
    static const uint32_t ids[5] = {0x14, 0, 1, 4, 0x12};
    for (int i = 0; i < 5; i++) k->input[i] = (uint8_t)k->key_state(p->user, ids[i], 0, 0, 0, 0);
#define KEY(c) ((uint8_t)k->key_pressed(p->user, (c), 0, 0, 0, 0))
    if (KEY(0x4e)) CALL(view_grow, 0, 8, 0);
    if (KEY(0x0d)) CALL(view_grow, 0, 8, 0);
    if (KEY(0x4a)) CALL(view_grow, 0, 0xfffffff8u, 0);
    if (KEY(0x0c)) CALL(view_grow, 0, 0xfffffff8u, 0);
    if (KEY(0xc9)) {
        int32_t n = (int32_t)CALL(mode_count, 0, 0, 0);
        int32_t i = (int32_t)CALL(mode_get, DD, 0, 0);
        if ((int32_t)((uint32_t)i + 1) < n) CALL(mode_set, DD, (uint32_t)i + 1, 0);
    }
    if (KEY(0xd1)) {
        CALL(mode_count, 0, 0, 0);
        int32_t i = (int32_t)CALL(mode_get, DD, 0, 0);
        if ((int32_t)((uint32_t)i - 1) >= 0) CALL(mode_set, DD, (uint32_t)i - 1, 0);
    }
    if (KEY(2)) detail(p, 0, 0x12c, 1, 0x459c4000u, 0);
    if (KEY(3)) detail(p, 1, 0x1f4, 1, 0x45ea6000u, 0);
    if (KEY(4)) detail(p, 2, 0x10000, 0, 0x461c4000u, 0);
#undef KEY
    if (*p->open) k->input_update(p->user, 0, 0, 0, 0, 0);
    return 0;
}

static void detail_apply(const MtPause* p, int32_t level, int32_t v97c, uint32_t on, uint32_t fbits) {
    detail(p, level, v97c, on, fbits, 0);
}

int32_t mt_pause_detail_high(const MtPause* p) {
    detail(p, 2, 0x10000, 0, 0x461c4000u, 0);
    return 0;
}

void mt_pause_banner(const MtPauseBanner* b) {
    uint32_t text;
    int32_t y;
    void* u = b->user;
    if ((uint8_t)b->is_singleplayer(u, 0, 0, 0, 0, 0)) {
        int32_t w = (int32_t)b->text_height(u, 0, 0, 0, 0, 0);
        int32_t h = (int32_t)b->screen_height(u, 0, 0, 0, 0, 0);
        text = 0x004e1cb8u;
        y = (int32_t)((uint32_t)((h >> 1) - (w >> 1)) + 0xfu);
    } else {
        if (b->menu_open(u, 0, 0, 0, 0, 0) == 0) return;
        if ((uint8_t)b->is_multiplayer(u, 0, 0, 0, 0, 0)) return;
        text = 0x004e1cbdu;
        b->text_height(u, 0, 0, 0, 0, 0);
        y = (int32_t)(b->screen_height(u, 0, 0, 0, 0, 0) - 0x1eu);
    }
    *b->timer = (uint16_t)(*b->timer + 1);
    int16_t vis = *b->visible;
    if (vis) {
        if ((int16_t)*b->timer > 0x14) {
            *b->timer = 0;
            *b->visible = 0;
            return;
        }
        uint32_t w = b->screen_width(u, 0, 0, 0, 0, 0);
        uint32_t st[1] = {0x24};
        b->text_begin(u, 6, 0x24, 0x96, st, 1);
        st[0] = text;
        b->text_draw(u, 6, (w >> 1) + 7, (uint32_t)y, st, 1);
        b->text_end(u, 6, 0, 0, 0, 0);
    } else if ((int16_t)*b->timer >= 0x14) {
        *b->timer = 0;
        *b->visible = 1;
    }
}

void mt_detail_apply(const MtPause* p) {
    int32_t lv = *p->detail;
    if (lv == 0) detail_apply(p, 0, 0x12c, 1, 0x459c4000u);
    else if (lv == 1) detail_apply(p, 1, 0x1f4, 1, 0x45ea6000u);
    else if (lv == 2) detail_apply(p, 2, 0x10000, 0, 0x461c4000u);
}

#define R(f, a, b, c, d) s->f(p->user, (uintptr_t)(a), (uintptr_t)(b), (uintptr_t)(c), (uintptr_t)(d))

int32_t mt_settings_load(const MtSettings* s) {
    const MtPause* p = s->menu;
    uint32_t key[3];                       /* key handle, size, mode */
    R(reg_init, key, 0, 0, 0);
    if (R(reg_open, key, 0x80000002u, 0, 0)) return 1;
    key[1] = 8;
    if (R(reg_read_bin, key, 0x004e7bacu, s->view, &key[1])) {
        s->view[1] = 2;
        s->view[0] = 0;
    }
    if (R(reg_read_dword, key, 0x004e1b9au, &key[2], 0))
        key[2] = (uint32_t)CALL(mode_count, 0, 0, 0) - 1u;
    R(reg_close, key, 0, 0, 0);
    if (!((int32_t)CALL(mode_count, 0, 0, 0) > (int32_t)key[2]))
        key[2] = (uint32_t)CALL(mode_count, 0, 0, 0) - 1u;
    CALL(mode_set, DD, key[2], 0);
    CALL(view_set, 0, (uint32_t)s->view[0], 0);
    mt_detail_apply(p);
    return 0;
}

int32_t mt_settings_save(const MtSettings* s) {
    const MtPause* p = s->menu;
    uint32_t key[1];
    R(reg_init, key, 0, 0, 0);
    uint32_t mode = *s->mode_index;
    R(view_get, 0, s->view, 0, 0);
    if (R(reg_open, key, 0x80000002u, 0, 0)) return 1;
    R(reg_write_bin, key, 0x004e7bacu, s->view, 8);
    R(reg_write_dword, key, 0x004e1babu, mode, 0);
    R(reg_close, key, 0, 0, 0);
    return 0;
}

void mt_pause_init(const MtPauseInit* s) {
    const MtPause* p = s->menu;
    *s->back_item = (int32_t)CALL(mode_count, 0, 0, 0);
    for (int32_t k = 0; k < *s->back_item; k++)
        s->wsprintf_fn(p->user, 0x00510a88u + 0x50u * (uint32_t)k, 0x004e1adcu, s->modes[4 * k], s->modes[4 * k + 1]);
    int32_t h = (int32_t)s->text_height(p->user, 0x00510dd4u, 0x00510a88u);
    *s->line_h = h;
    *s->cursor_max = 3;
    *s->menu_x = 0x14;
    *p->consumed = 0;
    *p->page = 0;
    *p->cursor = 0;
    *s->cursor_min = 0;
    *p->open = 0;
    *p->released = 0;
    *p->may_close = 1;
    double y = (double)h * 1.5 + 15.0;
    *s->menu_y = (y >= 2147483648.0 || y <= -2147483649.0) ? INT32_MIN : (int32_t)y;
}
