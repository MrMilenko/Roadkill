/*
 * mt_scene.c: scene engine setup, frame loop and redraw (see mt_scene.h).
 */
#include "mt_scene.h"

#include <string.h>

#include "../game/scene.h"

static int32_t i32(const uint8_t* b, int o) { int32_t v; memcpy(&v, b + o, 4); return v; }
static void w32(uint8_t* b, int o, uint32_t v) { memcpy(b + o, &v, 4); }
static void w16(uint8_t* b, int o, uint16_t v) { memcpy(b + o, &v, 2); }

void mt_scene_init(uint8_t* sc, uint32_t a, uint32_t b, uint32_t c, uint32_t stack, const MtSceneEnv* env) {
    void* u = env->user;
    uint32_t buf = env->screen_buffer(u);
    w32(sc, 0x0, buf);
    w32(sc, 0x4, buf);
    buf = env->screen_buffer(u);
    w32(sc, 0x10, 0);
    w32(sc, 0x14, 0);
    w32(sc, 0x18, 0);
    w16(sc, 0x1c, 0);
    w16(sc, 0x1e, 0);
    w16(sc, 0x20, 0);
    w32(sc, 0x2c, 1);
    w32(sc, 0x30, 1);
    w32(sc, 0x34, 1);
    w32(sc, 0x38, 0x27f);
    w32(sc, 0x3c, 0x1df);
    w32(sc, 0x8, buf);
    w32(sc, 0xc, buf);
    w32(sc, 0x24, stack);
    env->input_start(u);
    env->input_finalize(u);
    static const uint32_t ent[6] = {0, 0, 0x3f800000, 0x3f800000, 0x46, 0x23};
    env->entity_create(u, 0, ent);
    env->textures_setup(u, 0);
    for (int k = 0; k < 3; k++) env->light_a(u, 0, k, 0xff, 0xff, 0xff);
    for (int k = 0; k < 3; k++) env->light_b(u, 0, k, 200, 0x1e);
    env->light_c(u, 0, 0x20, 0x20, 0x20);
    w32(sc, 0x40, 0);
    sc[0x44] = 0;
    w32(sc, 0x48, 0);
    w32(sc, 0x4c, 0);
    w32(sc, 0x50, 0);
    w32(sc, 0x74, 0);
    w32(sc, 0xb8, 0);
    w32(sc, 0x13c, 0);
    w32(sc, 0xaa0, 0);
    w32(sc, 0xacc, 0);
    w32(sc, 0xf8c, 0);
    w32(sc, 0x28, 1);
    w32(sc, 0xf98, 0);
    w32(sc, 0xf9c, 0);
    w32(sc, 0xf90, 0x3c);
    w32(sc, 0xf84, b);
    w32(sc, 0xf88, c);
    w32(sc, 0xf80, a);
}

typedef struct {
    const MtSceneEnv* env;
} RunCtx;

static void run_once(void* user, uint32_t cmd) {
    const MtSceneEnv* env = ((RunCtx*)user)->env;
    env->run_command(env->user, cmd);
}

int32_t mt_scene_run(uint8_t* sc, const MtSceneEnv* env) {
    void* u = env->user;
    RunCtx ctx = {env};
    env->pad_a(u, sc + 0x10, 0);
    env->input_poll(u, env->pad_b(u, sc + 0x1c, 0));
    env->input_update(u);
    int32_t n = i32(sc, 0xacc);
    if (n < 0x96) {
        sc[0xad0 + 8 * n] = 0;
        n = i32(sc, 0xacc);
        w32(sc, 0xad4 + 8 * n, 0);
        w32(sc, 0xacc, (uint32_t)i32(sc, 0xacc) + 1);
    }
    uint32_t start = env->timer_now(u);
    while (i32(sc, 0x40) == 0) {
        env->render_effects(u);
        uint32_t f = (uint32_t)i32(sc, 0x0);
        w32(sc, 0x4, (uint32_t)i32(sc, 0x4) == f ? f + 0x8010 : f);
        f = (uint32_t)i32(sc, 0x8);
        w32(sc, 0xc, f == (uint32_t)i32(sc, 0xc) ? f + 0x8010 : f);
        if (i32(sc, 0xaa0) > 0) {
            int32_t k = 0;
            do {
                uint32_t cmd = (uint32_t)i32(sc, 0xaa4 + 4 * k);
                k++;
                env->run_command(u, cmd);
            } while (k < i32(sc, 0xaa0));
        }
        scene_dispatch_events(sc, run_once, &ctx);
        mt_scene_draw(sc, (uint32_t)i32(sc, 0x4), env);
        int32_t rate = env->tics_to_rate(u, env->timer_now(u) - start);
        if (i32(sc, 0xf90) < 0x3c) {
            while (rate > i32(sc, 0xf90)) {
                env->idle(u, 0);
                rate = env->tics_to_rate(u, env->timer_now(u) - start);
            }
        }
        env->frame_timer_update(u);
        start = env->timer_now(u);
    }
    return i32(sc, 0xf94);
}

void mt_scene_draw(uint8_t* sc, uint32_t buffer, const MtSceneEnv* env) {
    void* u = env->user;
    if (i32(sc, 0x2c) == 0) return;
    uint32_t img = (uint32_t)i32(sc, 0x4c);
    if (img) {
        uint8_t* rec = env->resolve(u, img);
        if (rec[0x130]) {
            int32_t v = i32(rec, 0x12c);
            env->bg_draw(u, buffer != env->screen_buffer(u), v, 0, 0);
        }
    }
    env->draw_begin(u);
    uint8_t* bp = env->resolve(u, buffer);
    w32(bp, 0x8008, (uint32_t)i32(bp, 0x8004));
    env->buffer_reset(u, buffer, 0x1000);
    if (i32(sc, 0xf8c)) env->textures_setup(u, 0);
    if (i32(sc, 0x74) > 0) {
        int32_t k = 0;
        do {
            uint32_t obj = (uint32_t)i32(sc, 0x78 + 4 * k);
            k++;
            env->draw_object(u, obj, buffer);
        } while (k < i32(sc, 0x74));
    }
    if (i32(sc, 0xf8c)) {
        env->list_flush(u, (uint32_t)i32(bp, 0));
        env->buffer_reset(u, buffer, 0x1000);
        env->textures_setup(u, 1);
    }
    if (i32(sc, 0xb8) > 0) {
        int32_t k = 0;
        do {
            uint32_t obj = (uint32_t)i32(sc, 0xbc + 4 * k);
            k++;
            env->draw_object(u, obj, buffer);
        } while (k < i32(sc, 0xb8));
    }
    env->list_flush(u, (uint32_t)i32(bp, 0));
    env->draw_end(u);
    env->present_rect(u, sc + 0x30);
    w32(sc, 0x2c, 0);
}

int32_t mt_scene_hit(const uint8_t* o, int32_t x, int32_t y, const MtSceneEnv* env) {
    if (i32(o, 0x44) != 0)
        return x > i32(o, 0x34) && x < i32(o, 0x3c) && y > i32(o, 0x38) && y < i32(o, 0x40);
    int32_t n = i32(o, 0x4c);
    if (n <= 0) return 0;
    const uint8_t* pts = env->resolve(env->user, (uint32_t)i32(o, 0x48));
    for (int32_t k = 0; k < n; k++) {
        const uint8_t* p = pts + 8 * k;
        const uint8_t* q = k == n - 1 ? pts : pts + 8 * k + 8;
        uint32_t ex = (uint32_t)i32(q, 0) - (uint32_t)i32(p, 0), ey = (uint32_t)i32(q, 4) - (uint32_t)i32(p, 4);
        uint32_t dx = (uint32_t)x - (uint32_t)i32(p, 0), dy = (uint32_t)y - (uint32_t)i32(p, 4);
        if ((int32_t)(ex * dy - ey * dx) < 0) return 0;
        n = i32(o, 0x4c);
    }
    return 1;
}

void mt_scene_image_load(uint8_t* rec, const MtSceneEnv* env) {
    env->bg_load(env->user, rec, 0);
    rec[0x130] = 1;
}

int32_t mt_scene_load_resources(uint32_t a, uint32_t b, const MtSceneEnv* env) {
    (void)a;
    env->idle(env->user, 0);
    env->file_load(env->user, b);
    return 1;
}
