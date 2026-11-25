/*
 * en_view2.h: the small view accessors and the scene module init, ported from TM2.EXE
 * (EnViewBlock 0x00a03a98 is in en_view.h; entity memory through EnMem):
 *
 *   0x004938a4 en_view_screen_size   *w, *h = view 0's viewport +0x10 / +0x14 (screen size)
 *   0x004938d4 en_view_rect16        (no Ghidra entry) i16 left, top, width, height of view 0's
 *                                    viewport into 0x00a03a90, returns that address
 *   0x0049392c en_view_rect          rect (i32 left, top, right, bottom) of view i's viewport; NULL: nothing
 *   0x004941f0 en_view_resize_all    new screen size W, H (0x004e83ec / 0x004e83f0): for each view
 *                                    0 .. [0x00a0413c]: border (+0x8c / +0x90) rescaled, then the
 *                                    viewport setup 0x00493ed0 and the renderer viewport 0x004ad3d4
 *   0x00494360 en_view_border        *out = viewport i +0x8c
 *   0x0049433c en_view_border_call   0x0049437c(i, x + viewport i +0x8c)
 *   0x00494bfc en_view_set_target    camera i +0x10 = target (0 or no camera: the root entity)
 *   0x00494c3c en_view_set_viewer0   viewport entity 0 = p, [p] = 0xffe
 *   0x00494c5c en_view_set_viewer_pos  viewport entity i +0xe8..+0xf0 = v
 *   0x00494ca4 / 0x00494cc0          (no Ghidra entry) read camera i angles (+0xe0, 8 bytes) / position (+0xe8, 12)
 *   0x00494cdc / 0x00494d34          write them (camera present, i < 2)
 *   0x00494d04 / 0x00494d14 / 0x00494d24  (no Ghidra entry) address of camera i +0xe8 / +0xe0 / +0xb0
 *   0x00494d5c                       (no Ghidra entry) camera i's target (+0x10), 0 when none or i >= 2
 *   0x00494d80 en_fog_set            fog r, g, b dwords at 0x00a03b64 / 68 / 6c; 3D device mode also
 *                                    0x004b6428(0x00c84840, r & 0xff, g & 0xff, b & 0xff)
 *   0x00494e64 / 0x00494e84          address of render_pos[i] / cam_mat[i] (i >= 2: entry 0)
 *   0x004a1224 en_scene_init         (subsys_entities_init) module inits, scene driver globals cleared
 * Index tests are signed (i < 2) as in the original. Verified: tools/emu/check_engine_view2.sh.
 */
#ifndef TM2_EN_VIEW2_H
#define TM2_EN_VIEW2_H

#include <stddef.h>
#include <stdint.h>

#include "en_scene.h"
#include "en_view.h"

typedef struct {
    uint8_t* mem;
    uint32_t base;
    size_t size;
} EnMem;

void en_view_screen_size(const EnViewBlock* b, int32_t* w, int32_t* h);
uint32_t en_view_rect16(const EnViewBlock* b, int16_t out[4]);   /* out: 0x00a03a90 */
void en_view_rect(const EnViewBlock* b, int32_t i, int32_t* rect);

typedef struct {
    int32_t* count;     /* 0x00a0413c highest view index */
    int32_t* screen_w;  /* 0x004e83ec */
    int32_t* screen_h;  /* 0x004e83f0 */
    /* 0x00493ed0(i, six stack values: viewport +0x28, +0x2c, +0x30, +0x34, +0x00, +0x04) */
    void (*setup)(void* user, int32_t i, const int32_t args[6]);
    /* 0x004ad3d4(i, w, h, x0, x1, y0, y1) */
    void (*gpu_viewport)(void* user, int32_t i, int32_t w, int32_t h, int32_t x0, int32_t x1, int32_t y0, int32_t y1);
    void* user;
} EnResizeEnv;
void en_view_resize_all(EnViewBlock* b, EnResizeEnv* e, int32_t w, int32_t h);

void en_view_border(const EnViewBlock* b, int32_t i, int32_t* out);
void en_view_border_call(const EnViewBlock* b, int32_t i, int32_t x, void (*fn)(void* user, int32_t i, int32_t x), void* user);

void en_view_set_target(const EnViewBlock* b, EnMem* m, uint32_t target, int32_t i);
void en_view_set_viewer0(EnViewBlock* b, EnMem* m, uint32_t p);
void en_view_set_viewer_pos(const EnViewBlock* b, EnMem* m, const int32_t v[3], int32_t i);
void en_view_get_cam_angles(const EnViewBlock* b, EnMem* m, uint8_t out[8], int32_t i);
void en_view_get_cam_pos(const EnViewBlock* b, EnMem* m, uint8_t out[12], int32_t i);
void en_view_set_cam_angles(const EnViewBlock* b, EnMem* m, const uint8_t in[8], int32_t i);
void en_view_set_cam_pos(const EnViewBlock* b, EnMem* m, const uint8_t in[12], int32_t i);
uint32_t en_view_cam_field(const EnViewBlock* b, int32_t i, uint32_t off);   /* off 0xe8, 0xe0, 0xb0 */
uint32_t en_view_cam_target(const EnViewBlock* b, EnMem* m, int32_t i);

void en_fog_set(EnViewBlock* b, int32_t r, int32_t g, int32_t bl, int hw,
                void (*device)(void* user, uint32_t r, uint32_t g, uint32_t b), void* user);
uint32_t en_view_render_pos_addr(int32_t i);   /* 0x00a03aec + 12 i */
uint32_t en_view_cam_mat_addr(int32_t i);      /* 0x00a03b04 + 0x30 i */

/* init(user, k, a): k 0 = 0x0049d494(0x00a5eb68), 1 = 0x0048e240(0x00a5eb68),
 * 2 = 0x004c38f4(0x00a5eb68, 0x00a5eb68), 3 = 0x004a14f0(0x00a6d5c8) (which stores it to
 * 0x00a81e3c, and EDX as left by 0x004c38f4 to 0x00a81e34). Then cull force, LOD off, force visible,
 * the animation queue (count, on, 100 nodes) and the 20 extra models are cleared. */
void en_scene_init(EnScene* s, void (*init)(void* user, int k, uint32_t a), void* user);

#endif /* TM2_EN_VIEW2_H */
