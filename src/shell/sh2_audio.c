/*
 * sh2_audio.c: the audio configuration object's destructor (see sh2_audio.h).
 */
#include <string.h>
#include "sh2_audio.h"

static uint32_t call2(const Sh2Env* e, uint32_t fn, uint32_t a0, uint32_t a1) {
    uint32_t a[2] = {a0, a1};
    return e->sh.call ? e->sh.call(e->sh.user, fn, 2, a) : 0;
}

/*
 * The member walk as the machine code does it, one letter per step (taken from the instruction
 * sequence 0x00486244..0x00486854): C = eax = 0x004aa064(eax, 0), D = eax = 0x004a3ff4(eax, 0),
 * S = eax -= k, B = base = eax - k, A = eax = base + k (k from the list in order). The walk
 * follows the returned pointers, not the object's layout, exactly as the original.
 */
static const char steps[] = "ACSCSCSCSCSCSCSCSCSCBACSCSCSCSCSCSDSCSCSCBACSCSCSCSCSCSCSCSCSCBACSCSCSDSCSCSCSCSCSCBACSCSCSCSCSCSCSCSCSCBADSCSCSCSCSCSCSCSCSCBACSCSCSCSCSCSCSDSCSCBACSCSCSCSCSCSCSCSCSCBACSCSCSCSDSCSCSCSCSCBACSCSCSCSCSCSCSCSCSCBACSDSCSCSCSCSCSCSCSCBACSCSCSCSCSCSCSCSDSCBACSCSCSCSCSCSCSCSCSCBACSCSCSCSCSDSDSDSDSDBADSDS";
static const uint16_t ks[] = {0x110c, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x1058, 0x1044, 0x14, 0x34, 0x14, 0x14, 0x14, 0x6c, 0x14, 0x14, 0x14, 0xf18, 0xf04, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x34, 0xe30, 0xe1c, 0x14, 0x14, 0x6c, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0xd10, 0xcfc, 0x14, 0x14, 0x14, 0x14, 0x14, 0x34, 0x14, 0x14, 0x14, 0xc28, 0xbbc, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0xb08, 0xaf4, 0x14, 0x14, 0x34, 0x14, 0x14, 0x14, 0x6c, 0x14, 0x14, 0x9c8, 0x9b4, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x900, 0x8cc, 0x14, 0x14, 0x14, 0x6c, 0x14, 0x14, 0x14, 0x14, 0x14, 0x7c0, 0x7ac, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x34, 0x14, 0x14, 0x6d8, 0x6c4, 0x6c, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x5b8, 0x5a4, 0x14, 0x14, 0x14, 0x34, 0x14, 0x14, 0x14, 0x6c, 0x18, 0x474, 0x460, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x3ac, 0x398, 0x34, 0x14, 0x14, 0x14, 0x6c, 0x54, 0x54, 0x54, 0x54, 0x16c, 0x118, 0x7c, 0x9c};

uint32_t sh2_audio_cfg_destroy(const Sh2Env* e, uint32_t obj, uint32_t flags) {
    if (flags & 4) {
        uint32_t r = call2(e, 0x004a9ff8u, obj, 0x004e56ecu);
        uint32_t a[1] = {r};
        if (e->sh.call) e->sh.call(e->sh.user, 0x004aa018u, 1, a);
        return obj;
    }
    sh_w32(&e->sh, obj + 0x34, 0x004e57f4u);
    sh_w32(&e->sh, obj, 0);
    call2(e, 0x004ac590u, obj + 0x112c, 0);
    uint32_t eax = obj, base = obj;
    int k = 0;
    for (const char* s = steps; *s; s++) {
        switch (*s) {
        case 'C': eax = call2(e, 0x004aa064u, eax, 0); break;
        case 'D': eax = call2(e, 0x004a3ff4u, eax, 0); break;
        case 'S': eax -= ks[k++]; break;
        case 'B': base = eax - ks[k++]; break;
        default: eax = base + ks[k++]; break;
        }
    }
    uint32_t r = call2(e, 0x004a6b10u, eax, 1);
    if (flags & 2) {
        uint32_t a[1] = {r};
        if (e->sh.call) e->sh.call(e->sh.user, 0x004aa39cu, 1, a);
    }
    return r;
}

/* the caller's frame of 0x004868cc: offsets as on the original's stack */
static void fw32(uint8_t* f, uint32_t off, uint32_t v) {
    f[off] = (uint8_t)v; f[off + 1] = (uint8_t)(v >> 8); f[off + 2] = (uint8_t)(v >> 16); f[off + 3] = (uint8_t)(v >> 24);
}
static uint32_t fr32(const uint8_t* f, uint32_t off) {
    return (uint32_t)f[off] | (uint32_t)f[off + 1] << 8 | (uint32_t)f[off + 2] << 16 | (uint32_t)f[off + 3] << 24;
}
static uint32_t call3(const Sh2Env* e, uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2) {
    uint32_t a[3] = {a0, a1, a2};
    return e->sh.call ? e->sh.call(e->sh.user, fn, 3, a) : 0;
}
static uint32_t call4(const Sh2Env* e, uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t a[4] = {a0, a1, a2, a3};
    return e->sh.call ? e->sh.call(e->sh.user, fn, 4, a) : 0;
}
static void copy20(const Sh2Env* e, uint32_t dst, uint32_t src) {
    for (uint32_t i = 0; i < 20; i += 4)        /* forward, a dword at a time (rep movsd) */
        sh_w32(&e->sh, dst + i, sh_r32(&e->sh, src + i));
}

void sh2_audio_read_settings(const Sh2Env* e, uint32_t cfg) {
    uint8_t own[0x180] = {0};
    uint8_t* f = sh_local_host(&e->sh, sh_local_at(&e->sh, 0), own, sizeof own);
    memset(f, 0, sizeof own);
    Sh2Env* me = (Sh2Env*)e;
    const uint32_t key = cfg + 0x112c, card = cfg + 0x1128, ok = cfg + 0x1120;
    me->local[0] = f;
    if (call3(e, 0x004ac5bcu, key, 0x004e2d03u, card)) {
        sh_w32(&e->sh, card, 0);
        call3(e, 0x004ac614u, key, 0x004e2d0fu, sh_r32(&e->sh, card));
    }
    sh_w32(&e->sh, ok, 1);
    /* MIXERLINEA at +0: cbStruct 0xa8, dwComponentType (+0x18) 4 = speakers */
    fw32(f, 0, 0xa8);
    fw32(f, 0x18, 4);
    if (call3(e, sh_r32(&e->sh, 0x00c90628u), sh_r32(&e->sh, card), sh_local_at(&e->sh, 0), 3)) {
        sh_w32(&e->sh, ok, 0);
    } else {
        /* MIXERLINECONTROLSA at +0x13c, its MIXERCONTROLA at +0xa8 */
        fw32(f, 0x140, fr32(f, 0xc));          /* dwLineID */
        fw32(f, 0x148, fr32(f, 0x24));         /* cControls */
        fw32(f, 0x14c, 0x94);
        fw32(f, 0x13c, 0x18);
        fw32(f, 0x150, sh_local_at(&e->sh, 0xa8));
        fw32(f, 0x144, 0x50030002u);
        if (call3(e, sh_r32(&e->sh, 0x00c90624u), sh_r32(&e->sh, card), sh_local_at(&e->sh, 0x13c), 2)) {
            sh_w32(&e->sh, ok, 0);
        } else {
            /* MIXERCONTROLDETAILS at +0x154, details at +0x16c */
            fw32(f, 0x158, fr32(f, 0xac));     /* dwControlID */
            fw32(f, 0x154, 0x18);
            fw32(f, 0x15c, fr32(f, 0x1c));     /* cChannels */
            fw32(f, 0x164, 4);
            fw32(f, 0x168, sh_local_at(&e->sh, 0x16c));
            fw32(f, 0x160, 0);
            call3(e, sh_r32(&e->sh, 0x00c9061cu), sh_r32(&e->sh, card), sh_local_at(&e->sh, 0x154), 0);
        }
    }
    sh_w32(&e->sh, ok, sh_r32(&e->sh, ok));
    fw32(f, 0x174, 0x3c);
    fw32(f, 0x17c, cfg + 0x4c);
    fw32(f, 0x178, key);
    if (call4(e, 0x004ac5e4u, key, 0x004e2d1bu, cfg + 0x4c, sh_local_at(&e->sh, 0x174))) {
        uint32_t row = fr32(f, 0x17c) + sh_r32(&e->sh, card) * 20u;
        copy20(e, row, cfg + 0x38);
        row = fr32(f, 0x17c) + sh_r32(&e->sh, card) * 20u;
        copy20(e, row + 4, cfg + 0x38);
        row = fr32(f, 0x17c) + sh_r32(&e->sh, card) * 20u;
        copy20(e, row + 8, cfg + 0x38);
        uint32_t a[1] = {cfg};
        if (e->sh.call) e->sh.call(e->sh.user, 0x00486b44u, 1, a);
        call4(e, 0x004ac65cu, fr32(f, 0x178), 0x004e2d26u, cfg + 0x4c, 0x3c);
    }
    me->local[0] = 0;
}

/* one MIXERLINEA at line, MIXERLINECONTROLSA at lc with its MIXERCONTROLA at ctl, then
 * MIXERCONTROLDETAILS at cd with the two channel values at det: the control type ctype set to v.
 * Returns 0 when the mixer failed (cfg +0x1120 cleared). Offsets are frame offsets. */
static int mixer_set(const Sh2Env* e, uint8_t* f, uint32_t cfg, uint32_t line, uint32_t lc, uint32_t ctl,
                     uint32_t cd, uint32_t det, uint32_t ctype, uint32_t v) {
    const uint32_t card = cfg + 0x1128, ok = cfg + 0x1120;
    fw32(f, line, 0xa8);
    fw32(f, line + 0x18, 4);
    if (call3(e, sh_r32(&e->sh, 0x00c90628u), sh_r32(&e->sh, card), sh_local_at(&e->sh, line), 3)) {
        sh_w32(&e->sh, ok, 0);
        return 0;
    }
    fw32(f, lc + 4, fr32(f, line + 0xc));
    fw32(f, lc + 0xc, fr32(f, line + 0x24));
    fw32(f, lc, 0x18);
    fw32(f, lc + 0x10, 0x94);
    fw32(f, lc + 0x14, sh_local_at(&e->sh, ctl));
    fw32(f, lc + 8, ctype);
    if (call3(e, sh_r32(&e->sh, 0x00c90624u), sh_r32(&e->sh, card), sh_local_at(&e->sh, lc), 2)) {
        sh_w32(&e->sh, ok, 0);
        return 0;
    }
    fw32(f, det, v);
    fw32(f, det + 4, v);
    fw32(f, cd, 0x18);
    fw32(f, cd + 4, fr32(f, ctl + 4));
    fw32(f, cd + 0xc, 0);
    fw32(f, cd + 8, fr32(f, line + 0x1c));
    fw32(f, cd + 0x14, sh_local_at(&e->sh, det));
    fw32(f, cd + 0x10, 4);
    call3(e, sh_r32(&e->sh, 0x00c90630u), sh_r32(&e->sh, card), sh_local_at(&e->sh, cd), 0);
    return 1;
}

void sh2_audio_apply_volume(const Sh2Env* e, uint32_t cfg, uint32_t mode) {
    uint8_t own[0x2fc] = {0};
    uint8_t* f = sh_local_host(&e->sh, sh_local_at(&e->sh, 0), own, sizeof own);
    memset(f, 0, sizeof own);
    Sh2Env* me = (Sh2Env*)e;
    me->local[0] = f;
    uint32_t src = 0;
    if (mode == 0) src = cfg + 0x38;
    else if (mode == 1) src = cfg + 0x4c + sh_r32(&e->sh, cfg + 0x1128) * 20u;
    else if (mode == 2) src = cfg + 0x88;
    if (src)
        for (uint32_t i = 0; i < 20; i += 4) fw32(f, 0x2d8 + i, sh_r32(&e->sh, src + i));
    call3(e, 0x00489340u, cfg, 4, fr32(f, 0x2d8));
    call3(e, 0x00489340u, cfg, 0x1005, fr32(f, 0x2e0));
    call3(e, 0x00489340u, cfg, 0x1008, fr32(f, 0x2dc));
    if (sh_r32(&e->sh, cfg + 0x1120) &&
        mixer_set(e, f, cfg, 0xa8, 0x2c0, 0x150, 0x290, 0x2f4, 0x50030002u, fr32(f, 0x2e4)) &&
        sh_r32(&e->sh, cfg + 0x1120))
        mixer_set(e, f, cfg, 0, 0x2a8, 0x1e4, 0x278, 0x2ec, 0x50030003u, fr32(f, 0x2e8));
    me->local[0] = 0;
}
