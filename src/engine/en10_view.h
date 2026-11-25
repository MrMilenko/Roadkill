/*
 * en10_view.h: view creation and the menu view, TM2.EXE 0x00494610 (viewCreate) and 0x0049437c
 * (subsys_menu_options), in the En2Env of en2_env.h. View blocks: see en10_viewport.h.
 * Verified against the original under emulation (tools/emu/check_en10_view.sh).
 */
#ifndef TM2_EN10_VIEW_H
#define TM2_EN10_VIEW_H

#include <stdint.h>
#include "en2_env.h"

/* 0x00494610 (eax = view, six stack words passed on to 0x00493ed0; ret 0x18): views 0 and 1 only
 * (signed: >= 2 does nothing): the view count 0x00a0413c raised, the border cleared, the
 * projection (0x00493ed0), the renderer viewport (0x004ad3d4(view, W, H, left, top; right, top,
 * bottom) with right / bottom inclusive), the view's slot 0x00a03ae4, the shared root entity
 * 0x00a03ac8 and the view's camera entity 0x00a03ad4 created when missing (0x00492d90), the
 * camera's +0x10 = root and its matrix (+0x50..+0xac) = the view's clip plane normals
 * (+0x44..+0x88) transposed, the translation cleared. */
void en10_view_create(En2Env* e, uint32_t view, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
                      uint32_t a5);

/* 0x0049437c (eax = view, edx = border): border = edx when negative, else 0 (ORIGINAL: the
 * clamp is inverted), at least (100 - W) / 2 when 2 * border + W < 100, rounded toward zero to a
 * multiple of 8 (+0x8c); +0x90 = chop(border * H / W * h / w) (integer then x87); the viewport
 * rectangle from the fractions inside the borders (x87 0x127f, chopped), the centre, the renderer
 * viewport (0x004ad3d4). Returns its eax. */
uint32_t en10_view_set_border(En2Env* e, uint32_t view, uint32_t border);

#endif
