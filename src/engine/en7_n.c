/*
 * en7_n.c: see en7_n.h.
 */
#include "en7_n.h"

/* 0x004a45f4: WM_DRAWITEM of a text button (DRAWITEMSTRUCT dis: +0x10 itemState, +0x18 hDC, +0x1c
 * rcItem): s = vtable +0xcc (o) (state bits); the background bitmap (pressed s & 1: +0x80 if bit 1
 * of +0xbc, else +0x6c if bit 0) drawn by 0x004aa2d8(bitmap, hdc, rc, rc); focus (s &
 * 8): bitmap +0x94 (bit 2) or a frame pair (rect inset 3 / 4 / 5 / 5, yellow 0xffff00 inside, dark
 * 0x808000 one pixel down right; CreateSolidBrush / FrameRect / DeleteObject); disabled item state
 * (bit 2): bitmap +0xa8 (bit 3) or vtable +0xac (o, hdc, rc); then the text (0x004a6ef4 into a 200
 * byte buffer) unless it starts with a space: DrawTextA(hdc, text, -1, rect (moved 2, 2 when s & 4),
 * 0x125). Returns 1. */
uint32_t en7_text_button_draw_item(En2Env* e, uint32_t o, uint32_t dis)
{
    const uint32_t buf = e->scratch, R1 = e->scratch + 0xc8, TR = e->scratch + 0xd8, R2 = e->scratch + 0xe8;
    uint32_t rc = dis + 0x1c, hdc = en2_r32(e, dis + 0x18);
    uint32_t vt = en2_r32(e, o + 0x34);
    uint32_t s = en2_call2(e, en2_r32(e, vt + 0xcc), o, vt);
    uint8_t bc = en2_r8(e, o + 0xbc);
    if (s & 1) {
        if (bc & 2) en2_call4(e, 0x004aa2d8u, o + 0x80, hdc, rc, rc);
    } else if (bc & 1) {
        en2_call4(e, 0x004aa2d8u, o + 0x6c, hdc, rc, rc);
    }
    if (s & 8) {
        if (en2_r8(e, o + 0xbc) & 4) {
            en2_call4(e, 0x004aa2d8u, o + 0x94, hdc, rc, rc);
        } else {
            en2_w32(e, R1, en2_r32(e, rc) + 3);
            en2_w32(e, R1 + 4, en2_r32(e, rc + 4) + 4);
            en2_w32(e, R1 + 8, en2_r32(e, rc + 8) - 5);
            en2_w32(e, R1 + 12, en2_r32(e, rc + 12) - 5);
            for (uint32_t k = 0; k < 16; k += 4) en2_w32(e, R2 + k, en2_r32(e, R1 + k));
            en2_call3(e, 0x00c9052cu, R2, 1, 1);
            uint32_t b1 = en2_call1(e, 0x00c901d4u, 0xffff00);
            uint32_t b2 = en2_call1(e, 0x00c901d4u, 0x808000);
            en2_call3(e, 0x00c904b4u, hdc, R2, b2);
            en2_call3(e, 0x00c904b4u, hdc, R1, b1);
            en2_call1(e, 0x00c901dcu, b1);
            en2_call1(e, 0x00c901dcu, b2);
        }
    }
    if (en2_r8(e, dis + 0x10) & 4) {
        if (en2_r8(e, o + 0xbc) & 8) en2_call4(e, 0x004aa2d8u, o + 0xa8, hdc, rc, rc);
        else en2_call3(e, en2_r32(e, en2_r32(e, o + 0x34) + 0xac), o, hdc, rc);
    }
    en2_call3(e, 0x004a6ef4u, o, buf, 0xc8);
    if (en2_r8(e, buf) == 0x20) return 1;
    for (uint32_t k = 0; k < 16; k += 4) en2_w32(e, TR + k, en2_r32(e, rc + k));
    if (s & 4) {
        en2_w32(e, TR + 4, en2_r32(e, TR + 4) + 2);
        en2_w32(e, TR, en2_r32(e, TR) + 2);
    }
    en2_call5(e, 0x00c9049cu, hdc, buf, 0xffffffffu, TR, 0x125);
    return 1;
}
