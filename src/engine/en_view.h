/*
 * en_view.h: viewport and light environment setters, ported from TM2.EXE
 *
 *   0x0048bb10 en_viewport_set_origin (viewport_set_bounds): 0x009f7800 / 0x009f7804 = x + 16,
 *              y + 16 and the projection centre 0x009f7810 / 0x009f7814 = (float)(x * 32 + 0x200),
 *              (y * 32 + 0x200) (edge units with the renderer's guard band); 3D device mode
 *              ([0x00c85008]) x, y as they are
 *   0x0048bb94 en_viewport_set_width (no Ghidra entry): 0x009f7808 = w, scale x 0x009f7818 =
 *              (w << 5) - 0.5 (device: w). The full view size setter is 0x0048bbcc (en_math.h)
 *   0x00493880 en_viewport_data (level_get_viewport_data): 0x00a03ba8 + 0x94 * [0x00a04138]
 *   0x00494c48 en_view_entity: the viewport's entity [0x00a03acc + 4 * i] (i >= 2: entry 0)
 *   0x00494dc0 en_fog_color: 0x00RRGGBB from the bytes 0x00a03b64, 0x00a03b68, 0x00a03b6c
 *   0x00495864 en_light_env_addr (lightGetEnv): 0x00a041c0 + 0x6c * i (i >= 4: entry 0)
 *   0x0049527c en_light_set_pos:  light j of environment e: position (i16 x, y, z)
 *   0x0049517c en_light_set_dir (lightSetEnvDirection): light j of environment e: direction from
 *              two i16 angles in degrees
 *   0x00495050 en_light_set:      both, angles as i32, position from three i16 stack arguments
 *   0x00493aa4 en_views_init (viewInit): zeroes the view table 0x00a03ac8..0x00a03ae8 and 0x00a0413c
 *   0x00494ec4 en_view_fixed_camera (viewSetFixedCamera): the scene root as a fixed camera over
 *              view i: m = max(w / 2, h / 2) of its viewport, root +0x14..+0x40 = position and
 *              matrix (mode 0: looking down, root z = -dist * (m - 1); else root y = dist * (m - 1)),
 *              view size (m, m) (0x0048bbcc), origin (w / 2, h / 2 - 1) (0x0048bb10), and the view
 *              entity's two culling matrices (+0x50, +0x80, 12 ints each) copied to the root
 *   0x004948b4 en_view_proc (viewProc / old name level_camera_setup): per frame camera of view i
 *              (see the function)
 * The light setters do nothing for e > 4 or j >= 3 (signed tests: negative indices write before the
 * tables in the original; the port ignores them).
 * Verified under emulation: tools/emu/check_engine_view.sh.
 */
#ifndef TM2_EN_VIEW_H
#define TM2_EN_VIEW_H

#include <stdint.h>

/* 0x009f7800 */
typedef struct {
    int32_t x, y, w, h;
    float cx, cy, sx, sy;   /* projection (EnProj in en_clip.h: cx, cy, sx, sy) */
} EnViewport;

void en_viewport_set_origin(EnViewport* v, int32_t x, int32_t y, int hw);
void en_viewport_set_width(EnViewport* v, int32_t w, int hw);
uint32_t en_viewport_data(int32_t cur);
uint32_t en_view_entity(const uint32_t tab[2], int32_t i);
uint32_t en_fog_color(uint8_t r, uint8_t g, uint8_t b);
uint32_t en_light_env_addr(int32_t i);

/* 0x00a04178 */
typedef struct {
    int32_t deg[3][3];   /* per light: elevation, 0, azimuth (degrees, as given to 0x0049517c) */
    int32_t pos[3][3];   /* 0x00a0419c per light: x, y, z (0x0049527c) */
    /* 0x00a041c0, 0x6c bytes per environment: +0x0c x[3], +0x18 y[3], +0x24 z[3] positions << 4
     * (one column per light), +0x3c 3 x 3 direction vectors (.12) */
    int32_t env[5][27];
} EnLights;

void en_light_set_pos(EnLights* l, int32_t e, int32_t j, int16_t x, int16_t y, int16_t z);
void en_light_set_dir(EnLights* l, int32_t e, int32_t j, int16_t az, int16_t el);
void en_light_set(EnLights* l, int32_t e, int32_t j, int32_t az, int32_t el, int16_t x, int16_t y, int16_t z);

/* 0x00a03ac8 */
typedef struct {
    uint32_t root;        /* 0x00a03ac8 the default camera target entity (viewCreate allocates it;
                           * 0x00494bfc falls back to it) */
    uint32_t entity[2];   /* 0x00a03acc viewport entity (0x00494c48) */
    uint32_t camera[2];   /* 0x00a03ad4 view entity (camera) */
    int32_t c[2];         /* 0x00a03adc */
    int32_t slot[2];      /* 0x00a03ae4 viewport slot (0x94 byte entries at 0x00a03b70) */
} EnViews;

/* cur: 0x00a0413c */
void en_views_init(EnViews* v, int32_t* cur);

/* vports: the 0x94 byte viewport entries at 0x00a03b70 (+0x18 width, +0x1c height read);
 * root: the scene root entity's memory, cam: views->camera[i]'s memory */
void en_view_fixed_camera(EnViewport* vp, const EnViews* views, const uint8_t* vports, int32_t* root, const int32_t* cam,
                          int32_t i, int32_t dist, int32_t mode, int hw);

/* the view globals 0x00a03a98..0x00a03c50 as one block */
typedef struct {
    int32_t rot[12];          /* 0x00a03a98 view tilt matrix (+0, +0x14, +0x1c set from the viewport) */
    EnViews views;            /* 0x00a03ac8 */
    int32_t render_pos[2][3]; /* 0x00a03aec camera position per view */
    int32_t cam_mat[2][12];   /* 0x00a03b04 camera matrix per view */
    uint8_t fog[12];          /* 0x00a03b64 r, -, -, -, g, -, -, -, b (0x00494dc0) */
    uint8_t vports[2][0x94];  /* 0x00a03b70 viewports: +0x08 / +0x0c view size, +0x18 / +0x1c width,
                               * height, +0x20 / +0x24 left, top, +0x38 / +0x3c / +0x40 tilt (cos, sin, sin) */
} EnViewBlock;

/* 0x004948b4: cam = the view's camera entity (views.camera[i]), its +0x10 entity V the camera's
 * target, root the scene root. Builds cam +0xb0 and V +0xb0 from their angles +0xe0 (0x0048c360),
 * T = tilt * cam.b0; root +0x20 = T * V.b0; root +0x14 = V.b0^T * -cam.e8 - V.e8; root +0x44 =
 * root.20 * root.14 (0x0048cca8); copies to render_pos / cam_mat; root +0x50 / +0x80 =
 * V.b0^T * cam.b0^T * cam +0x50 / +0x80; view size (vp +0x08, +0x0c) and origin (w / 2 + left,
 * top + h / 2) of viewport i (the tilt comes from viewport views.slot[i]; slots 0 and 1 only here). trans: 0x009f77c4, left (0, 0, 0) by the SetTransMatrix calls. i >= 2: nothing. */
void en_view_proc(EnViewport* vp, EnViewBlock* b, int32_t* root, int32_t* cam, int32_t* tgt, int32_t trans[3],
                  int32_t i, int hw);

#endif /* TM2_EN_VIEW_H */
