/*
 * gp_radar.h: one radar blip, ported from TM2.EXE 0x00436a40.
 * (The old name carsPhysicsUpdate and its PS1 pair are wrong: this places a dot on the radar.)
 *
 * gp_radar_blip(type, pos, angle, view): the offset pos (i32 x, y relative to the viewer) turned by
 * angle and scaled: x = (X cos - Y sin) / scale, y = -(X sin + Y cos) / scale (32 bit products, idiv),
 * scale = i32 view +0x368. Radar view records: 0x370 bytes from 0x004faa3c, +0x30 dot prims (16 bytes,
 * two per dot), +0x35C..+0x35F u8 edge flags (down, up, right, left), +0x360 i32 prims used.
 *   Inside (-35..35, -22..21) with room for two more prims (used + 2 <= 24): x, y move by the screen
 *   offset (i32 0x004e670c + 0x25, 0x004e6710 + 0x17) unless split (0x0046cc54); the two prims get
 *   the corners (x - 1, y - 1, x, y) / (x - 1, y, x, y - 1), one pixel wider (x + 1, and the rows y - 1 /
 *   y) when 0x004958ac is set; colour bytes +4 / +5 / +6 by car type (table in gp_radar.c).
 *   Outside: one edge flag: x within -35..35: up when y > 0 else down; else left (x < 0) / right
 *   (x > 0) when |x| > |y|, else up / down by y.
 * gp_radar_init (0x004367f4, PS1 pair hudRadarInit): both views' radar records: frame prims (+0x04..+0x1F),
 *   48 dot prim codes (+0x37 + 0x10 k = 0x40), the two edge prims (+0x334..+0x347), when not split
 *   (0x0046cc54) the frame corners from the screen offset (16 bit) and the edge prims placed for the
 *   wide (0x004958ac) or normal layout; then +0x364 = 8000, scale +0x368 = (8000 << 12) / 24 =
 *   0x14D555, +0x36C = 0, +0x360 = 0. DECOMPILER TRAP: Ghidra drops the scale clamp (scale < 1 -> 1)
 *   as unreachable, rightly: the constant is never below 1; the port leaves it out too.
 * gp_radar_draw (0x004361d4, PS1 pair hudDisplayRadar; view in EDX, EAX unused): queues the
 *   radar of a view: (wide: the frame quad R + j * 0x18, j = i32 +0x36C, set to the screen offset box
 *   (+0x49, +0x2D) with code bit 2, and the edge prims first) the edge arrows whose flag is set
 *   (down, up, left, right: prim 0x004f696c + 0x18 view + 0x30 k at the screen offset + (0x1F, -7),
 *   (0x1F, 0x2C), (-7, 0x11), (0x48, 0x11), uv / size / page from image 0x004fb11c + 8 view + 0x10 k,
 *   the up arrow one texel lower; the images of view 1 overlap view 2's record: only 2 views exist), every used dot prim, (not wide: the edge prims and the frame quad
 *   last), then clears +0x36C, +0x360 and the four flags. Indexes are reread from memory for each
 *   store, as the original does.
 * The original's EAX on return is whatever the last path left there (callers ignore it); not ported.
 * The original faults on scale 0.
 *
 * Verified against the original under emulation (tools/emu/check_gameplay_radar.sh).
 */
#ifndef TM2_GP_RADAR_H
#define TM2_GP_RADAR_H

#include <stdint.h>

typedef struct {
    uint8_t* views;             /* 0x004faa3c, 0x370 per view */
    const int32_t* scr_x;       /* 0x004e670c */
    const int32_t* scr_y;       /* 0x004e6710 */
    void* user;
    int32_t (*split)(void* user);      /* 0x0046cc54 */
    uint8_t (*wide)(void* user);       /* 0x004958ac */
    /* gp_radar_draw only */
    uint8_t* arrows;            /* 0x004f696c: edge arrow prims, 0x18 per view, 0x30 per edge */
    const uint8_t* images;      /* 0x004fb11c: arrow images (u8 u, v, w, h, u16 page), 8 per view, 0x10 per edge */
    uint32_t views_addr;        /* 0x004faa3c */
    uint32_t arrows_addr;       /* 0x004f696c */
    void (*begin)(void* user);                     /* 0x0046cba4 */
    void (*add_prim)(void* user, uint32_t prim);   /* 0x004967f4 (game address) */
} GpRadarEnv;

void gp_radar_init(GpRadarEnv* env);
void gp_radar_draw(GpRadarEnv* env, int32_t view);
void gp_radar_blip(GpRadarEnv* env, uint32_t type, const int32_t pos[2], int32_t angle, int32_t view);

#endif /* TM2_GP_RADAR_H */
