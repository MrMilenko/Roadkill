/*
 * mt2_save.c: the save / load game screens, TM2.EXE 0x0046d290..0x0046f484. See mt2_save.h.
 * Every global is read again after each call, as the original does (callees run the message
 * loop and can change them).
 */
#include "mt2_save.h"

#define G(a) (s->g[MT2_SAVE_G(a)])

/* per screen: slot button texts, the two extra buttons, their keys, the help panel, help lines,
 * the click handlers (code addresses, passed as handles) */
typedef struct {
    uint32_t slot_text, slot_text_step;
    uint32_t b7_text, b8_text;
    uint32_t b7_key_a, b7_key_b;
    uint32_t g_b8, g_b7, g_panel;
    uint32_t help1, help2;
    uint32_t on_b7, on_b8;
    uint32_t on_slot[6];
} Screen;

static const Screen SCREENS[2] = {
    {0x004e1d9e, 7, 0x004e1dc8, 0x004e1dc9, 0x74, 0x72, 0x0058a96c, 0x0058a970, 0x0058a968,
     0x004e1dca, 0x004e1df0, 0x0046ec54, 0x0046ec80,
     {0x0046da2c, 0x0046dd2c, 0x0046e034, 0x0046e33c, 0x0046e644, 0x0046e94c}},
    {0x004e2101, 0xc, 0x004e2149, 0x004e214a, 0x78, 0x77, 0x0058a978, 0x0058a97c, 0x0058a974,
     0x004e214b, 0x004e2176, 0x0046f620, 0x0046f644,
     {0x0046f51c, 0x0046f544, 0x0046f570, 0x0046f59c, 0x0046f5c8, 0x0046f5f4}},
};

static uint32_t button(const MtSave* s, uint32_t hwnd, uint32_t id, uint32_t text, int32_t x, int32_t y,
                       int32_t w, int32_t h)
{
    uint32_t buf = s->alloc(s->user, 0x1cc);
    if (buf == 0)
        return 0;
    int32_t st[5] = {1, x, y, w, h};
    return s->button_create(s->user, buf, hwnd, id, text, st);
}

static void links(const MtSave* s, uint32_t w, uint32_t a, uint32_t b, uint32_t c, uint32_t s0, uint32_t s1,
                  uint32_t s2)
{
    uint32_t st[3] = {s0, s1, s2};
    s->nav_links(s->user, w, a, b, c, st);
}

void mt2_save_widgets(const MtSave* s, int load)
{
    const Screen* sc = &SCREENS[load ? 1 : 0];
    uint32_t hwnd = s->main_window(s->user);
    for (int k = 0; k < 6; k++) {
        uint32_t w = button(s, hwnd, 0x79 + k, sc->slot_text + sc->slot_text_step * k, 0x78, 0x92 + 0x2f * k,
                            0x190, 0x19);
        G(0x0058a980 + 4 * k) = w;
        s->set_colors(s->user, w, 0xcc0ff, 0xc0c0c);
        s->set_keys(s->user, G(0x0058a980 + 4 * k), 0xd4, 0xd3, 0, 0);
    }
    s->labels(s->user, load);
    uint32_t w = button(s, hwnd, 0xb7, sc->b7_text, 0x17c, 0x1b3, 0x8c, 0x17);
    G(sc->g_b7) = w;
    s->set_colors(s->user, w, 0xcc0ff, 0xc0c0c);
    s->set_keys(s->user, G(sc->g_b7), sc->b7_key_a, sc->b7_key_b, 0, 0);
    w = button(s, hwnd, 0xb8, sc->b8_text, 0x78, 0x1b3, 0x8c, 0x17);
    G(sc->g_b8) = w;
    s->set_colors(s->user, w, 0xcc0ff, 0xc0c0c);
    s->set_keys(s->user, G(sc->g_b8), 0x76, 0x75, 0, 0);

    /* ORIGINAL BUG: when the allocation fails the panel is 0 and the method calls below read
     * address 0x34 (a crash in the original); the port calls the method on handle 0. */
    uint32_t buf = s->alloc(s->user, 0x7c);
    uint32_t p = 0;
    if (buf != 0) {
        int32_t st[4] = {0x78, 0x3f, 0x190, 0x2f};
        p = s->panel_create(s->user, buf, hwnd, 0xb9, 0x100, st);
    }
    G(sc->g_panel) = p;
    s->method(s->user, p, 0xb0, 0xffffff);
    s->method(s->user, G(sc->g_panel), 0xb4, 0);
    s->method(s->user, G(sc->g_panel), 0xb8, 0);
    s->list_clear(s->user, G(sc->g_panel));
    s->list_add(s->user, G(sc->g_panel), sc->help1);
    s->list_add(s->user, G(sc->g_panel), sc->help2);
    s->method(s->user, G(sc->g_b7), 0xe0, sc->on_b7);
    s->method(s->user, G(sc->g_b8), 0xe0, sc->on_b8);
    for (int k = 0; k < 6; k++)
        s->method(s->user, G(0x0058a980 + 4 * k), 0xe0, sc->on_slot[k]);

    /* slot k: up k-1 (slot 1: button b8), down k+1 (slot 6: b8), left / right none, the stack
     * words: 0, up (slot 1: b7), down */
    links(s, G(0x0058a980), G(sc->g_b8), G(0x0058a984), 0, 0, G(sc->g_b7), G(0x0058a984));
    for (int k = 1; k < 5; k++)
        links(s, G(0x0058a980 + 4 * k), G(0x0058a97c + 4 * k), G(0x0058a984 + 4 * k), 0, 0,
              G(0x0058a97c + 4 * k), G(0x0058a984 + 4 * k));
    links(s, G(0x0058a994), G(0x0058a990), G(sc->g_b8), 0, 0, G(0x0058a990), G(sc->g_b8));
    links(s, G(sc->g_b8), G(0x0058a994), G(0x0058a980), G(sc->g_b7), G(sc->g_b7), G(0x0058a994), G(sc->g_b7));
    links(s, G(sc->g_b7), G(0x0058a994), G(0x0058a980), G(sc->g_b8), G(sc->g_b8), G(sc->g_b8), G(0x0058a980));

    for (int k = 0; k < 6; k++)
        s->show(s->user, G(0x0058a980 + 4 * k), 5);
    s->show(s->user, G(sc->g_b7), 5);
    s->show(s->user, G(sc->g_b8), 5);
    s->show(s->user, G(sc->g_panel), 5);
    s->focus(s->user, G(sc->g_b7));
}

static void destroy(const MtSave* s, uint32_t a)
{
    uint32_t w = G(a);
    if (w != 0)
        s->method(s->user, w, 0, 2);
}

void mt2_save_destroy(const MtSave* s, int load)
{
    const Screen* sc = &SCREENS[load ? 1 : 0];
    for (int k = 0; k < 6; k++)
        destroy(s, 0x0058a980 + 4 * k);
    /* save: panel 0x0058a968, 0x0058a96c, 0x0058a970; load: 0x0058a974, 0x0058a978, 0x0058a97c */
    destroy(s, sc->g_panel);
    destroy(s, sc->g_b8);
    destroy(s, sc->g_b7);
}

uint32_t mt2_save_run(const MtSave* s)
{
    uint8_t key[4]; /* registry key object, opened and never closed */
    s->reg_open(s->user, key, 0);
    s->input_poll(s->user);
    s->input_update(s->user);
    mt2_save_widgets(s, 0);
    G(0x0058a950) = 0;
    do {
        s->input_poll(s->user);
        if ((uint8_t)s->key_state(s->user, 0x14, 0))
            G(0x0058a950) = 1;
        if ((uint8_t)s->key_state(s->user, 0x15, 0xffffffffu))
            G(0x0058a950) = 1;
    } while (G(0x0058a950) == 0);
    mt2_save_destroy(s, 0);
    return G(0x0058a954);
}

uint32_t mt2_load_run(const MtSave* s)
{
    uint8_t key[4];
    s->reg_open(s->user, key, 0);
    s->input_poll(s->user);
    s->input_update(s->user);
    mt2_save_widgets(s, 1);
    G(0x0058a950) = 0;
    G(0x0058a95c) = 0;
    do {
        s->input_poll(s->user);
        if ((uint8_t)s->key_state(s->user, 0x14, 0)) {
            G(0x0058a950) = 1;
            G(0x0058a958) = 0;
        }
        if ((uint8_t)s->key_state(s->user, 0x15, 0)) {
            G(0x0058a950) = 1;
            G(0x0058a958) = 0xffffffffu;
        }
    } while (G(0x0058a950) == 0);
    mt2_save_destroy(s, 1);
    return G(0x0058a958);
}

static void edit_mode(const MtSave* s, int on)
{
    s->list_clear(s->user, G(0x0058a968));
    s->list_add(s->user, G(0x0058a968), on ? 0x004e1e22 : 0x004e1e78);
    s->list_add(s->user, G(0x0058a968), on ? 0x004e1e48 : 0x004e1ead);
    for (int k = 0; k < 6; k++)
        s->enable(s->user, G(0x0058a980 + 4 * k), !on);
}

void mt2_save_slot(const MtSave* s, int slot)
{
    /* the slot's strings: the text of an empty slot, two message boxes, the edit box's text */
    uint32_t str = 0x004e1edf + 0x45 * (uint32_t)(slot - 1);
    uint32_t gw = 0x0058a980 + 4 * (uint32_t)(slot - 1);
    char text[0x80];
    uint32_t hwnd = s->main_window(s->user);
    s->get_text(s->user, G(gw), text, 0x80);
    if (s->str_compare(s->user, text, str) != 0 &&
        /* PC detail: slot 2 asks with MB_OKCANCEL | MB_ICONQUESTION, the others MB_OKCANCEL */
        s->message_box(s->user, hwnd, str + 0x10, str + 6, slot == 2 ? 0x21 : 1) == 2) {
        s->message_box(s->user, hwnd, str + 0x34, str + 0x2a, 0x30);
        s->input_poll(s->user);
        s->input_update(s->user);
        s->focus(s->user, G(gw));
        return;
    }
    if (G(0x0058a960) != 0)
        return;
    edit_mode(s, 1);
    s->show(s->user, G(gw), 0);
    /* ORIGINAL BUG: a failed allocation leaves the edit box 0 and the method calls below read
     * address 0x34 (a crash in the original). */
    uint32_t buf = s->alloc(s->user, 0x68);
    uint32_t e = 0;
    if (buf != 0) {
        int32_t st[5] = {0, 0x78, 0x92 + 0x2f * (slot - 1), 0x190, 0x19};
        e = s->edit_create(s->user, buf, hwnd, 0x78, str + 0x44, st);
    }
    G(0x0058a964) = e;
    s->method(s->user, e, 0xb0, 0xffffff);
    s->method(s->user, G(0x0058a964), 0xb4, 0);
    s->method(s->user, G(0x0058a964), 0xb8, 0);
    s->show(s->user, G(0x0058a964), 5);
    s->redraw(s->user, G(0x0058a964), 0, 0);
    s->focus(s->user, G(0x0058a964));
    s->screen_begin(s->user);
    G(0x0058a95c) = (uint32_t)slot;
    G(0x0058a960) = 1;
    int done = 0, enter = 0;
    do {
        if (s->has_focus(s->user, G(0x0058a964)) != 1)
            s->focus(s->user, G(0x0058a964));
        s->input_poll(s->user);
        uint8_t k = (uint8_t)s->get_key(s->user);
        if (k == 0x1c)
            enter = done = 1;
        if (k == 1)
            done = 1;
    } while (!done);
    G(0x0058a960) = 0;
    if (enter) {
        s->get_text(s->user, G(0x0058a964), text, 0x80);
        s->escape_amp(s->user, text, 1);
        s->set_text(s->user, G(gw), text);
        s->save_write(s->user, text);
    }
    if (G(0x0058a964) != 0)
        s->method(s->user, G(0x0058a964), 0, 2);
    s->show(s->user, G(gw), 5);
    s->redraw(s->user, G(gw), 0, 0);
    s->focus(s->user, G(gw));
    edit_mode(s, 0);
    s->screen_begin(s->user);
    s->input_poll(s->user);
    s->input_update(s->user);
}

void mt2_save_labels(const MtSave* s, int load)
{
    /* value names "SlotN" and the empty texts: load 0x004e1ccc / 0x004e1cd2, save 0x004e1d14 /
     * 0x004e1d1a, 0xc apart */
    uint32_t names = load ? 0x004e1ccc : 0x004e1d14;
    char data[0xa0];
    uint8_t key[4];
    uint32_t size;
    s->reg_open(s->user, key, 0);
    if (s->reg_subkey(s->user, key, 0x80000002u, 0) == 0) {
        size = 0xa0;
        for (int k = 0; k < 6; k++) {
            uint32_t w = 0x0058a980 + 4 * (uint32_t)k;
            int32_t r = s->reg_read(s->user, key, names + 0xc * (uint32_t)k, data, &size);
            if (r == 0)
                s->set_text(s->user, G(w), data);
            else
                s->set_text_h(s->user, G(w), names + 0xc * (uint32_t)k + 6);
            if (load) {
                s->enable(s->user, G(w), r == 0);
                s->redraw(s->user, G(w), 0, 0);
            }
        }
    }
    s->reg_close(s->user, key);
}

static void put32(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t* p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

void mt2_save_record_write(const MtSave* s, char* text)
{
    uint8_t rec[0xa0] = {0}; /* the original leaves the bytes after the text uninitialized */
    char name[8];
    uint8_t key[4];
    s->reg_open(s->user, key, 0);
    uint32_t hwnd = s->main_window(s->user);
    /* ORIGINAL BUG: the null and empty description paths return with the key still open */
    if (text == 0)
        return;
    if (s->str_compare(s->user, text, 0x004e1d5c) == 0) {
        s->message_box(s->user, hwnd, 0x004e1d5e, 0x004e1d5d, 0x30);
        return;
    }
    s->format_text(s->user, (char*)rec, text);
    put32(rec + 0x80, (uint32_t)(int32_t)(int8_t)s->game[0x624ead - 0x624ea8]);
    put32(rec + 0x84, (uint32_t)(int32_t)(int8_t)s->game[0x624ec7 - 0x624ea8]);
    put32(rec + 0x88, s->game[0x624ebf - 0x624ea8]);
    for (int k = 0; k < 10; k++) {
        rec[0x8c + 2 * k] = s->human0[0x856 + 2 * k];
        rec[0x8d + 2 * k] = s->human0[0x857 + 2 * k];
    }
    s->format_slot(s->user, name, 0x004e1d95, (int32_t)G(0x0058a95c));
    if (s->reg_subkey(s->user, key, 0x80000002u, 0) == 0)
        s->reg_write(s->user, key, name, rec, 0xa0);
    s->reg_close(s->user, key);
}

void mt2_save_record_load(const MtSave* s)
{
    uint8_t rec[0xa0];
    char name[8];
    uint8_t key[4];
    uint32_t size;
    uint8_t* gm = s->game;
    s->reg_open(s->user, key, 0);
    uint32_t hwnd = s->main_window(s->user);
    s->format_slot(s->user, name, 0x004e207f, (int32_t)G(0x0058a95c));
    if (s->reg_subkey(s->user, key, 0x80000002u, 0) == 0) {
        size = 0xa0;
        if (s->reg_read_s(s->user, key, name, rec, &size) != 0) {
            G(0x0058a958) = 0;
        } else if (s->str_compare(s->user, (const char*)rec, 0x004e2088) == 0) {
            G(0x0058a958) = 0;
        } else {
            gm[0] = 1;                       /* 0x00624ea8 */
            gm[1] = 1;                       /* 0x00624ea9 */
            gm[0x624ead - 0x624ea8] = rec[0x80];
            gm[0x624ec7 - 0x624ea8] = rec[0x84];
            uint8_t n = (uint8_t)s->level_count(s->user, 0x00624ea8, (int32_t)(int8_t)rec[0x84]);
            gm[0x624eaa - 0x624ea8] = (uint8_t)(n - gm[1]);
            int32_t mode = (int32_t)get32(rec + 0x88);
            gm[0x624ebf - 0x624ea8] = (uint8_t)(mode < 6 ? mode : 6);
            for (int k = 0; k < 10; k++) {
                s->human0[0x856 + 2 * k] = rec[0x8c + 2 * k];
                s->human0[0x857 + 2 * k] = rec[0x8d + 2 * k];
            }
            if (gm[0x624ed0 - 0x624ea8] == 0 && (int8_t)gm[0x624ec7 - 0x624ea8] > 4)
                s->message_box(s->user, hwnd, 0x004e208f, 0x004e208e, 0x30);
            G(0x0058a958) = 1;
        }
    }
    s->reg_close(s->user, key);
}
