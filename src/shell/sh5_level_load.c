/*
 * sh5_level_load.c: the level and car files of a session, TM2.EXE 0x004753a4 (see
 * sh5_level_load.h). Frame offsets below are from fb = ebp - 0x154:
 *   +0 path buffer, +0x12c level DPC size, +0x130 / +0x134 a human car's TPC page / DPC size,
 *   +0x138 / +0x13c an opponent's, +0x140 gs, +0x144 opponent index, +0x148 human pointer,
 *   +0x14c human index, +0x150 opponent pointer.
 * The car loops keep the original's signed compare of the count byte against the index, and
 * the car id is the signed byte (an id above 14 or negative takes the default name).
 */
#include "sh5_level_load.h"

#define R8(a) sh_r8(e, a)
#define R32(a) sh_r32(e, a)
#define W32(a, v) sh_w32(e, a, v)
#define FB(o) (fb + (uint32_t)(o))
#define C0(f) sh3_call(e, f, 0, 0, 0, 0, 0, 0)
#define C1(f, a) sh3_call(e, f, 1, a, 0, 0, 0, 0)
#define C2(f, a, b) sh3_call(e, f, 2, a, b, 0, 0, 0)
#define C3(f, a, b, c) sh3_call(e, f, 3, a, b, c, 0, 0)

#define NAME 0x009f6920u
#define DPC_SIZE 0x009f691cu
#define AUX_SIZE 0x009f6918u

static void sprintf5(const ShEnv* e, uint32_t fmt, uint32_t a, uint32_t b, uint32_t c) {
    uint32_t v[5] = {NAME, fmt, a, b, c};
    if (e->call) e->call(e->user, 0x004b27e6u, 5, v);   /* cdecl, all on the stack */
}

static uint32_t level_stem(uint8_t lv) {
    static const uint32_t t[12] = {0x004e2241u, 0x004e2249u, 0x004e2264u, 0x004e225cu, 0x004e226au, 0x004e224eu,
                                   0x004e226fu, 0x004e2256u, 0x004e2284u, 0x004e2273u, 0x004e2279u, 0x004e227eu};
    uint8_t k = (uint8_t)(lv - 1);
    return k <= 11 ? t[k] : 0x004e2284u;
}

static uint32_t car_stem(int8_t id) {
    static const uint32_t t[15] = {0x004e22bbu, 0x004e22beu, 0x004e22c1u, 0x004e22c4u, 0x004e22c7u,
                                   0x004e22cau, 0x004e22cdu, 0x004e22d0u, 0x004e22d3u, 0x004e22d6u,
                                   0x004e22d9u, 0x004e22dcu, 0x004e22e2u, 0x004e22dfu, 0x004e22e5u};
    return (uint8_t)id <= 14 ? t[(uint8_t)id] : 0x004e22e8u;
}

/* path = root 2 \ name; load into dst; returns the size */
static uint32_t load(const ShEnv* e, uint32_t fb, uint32_t dst) {
    C3(0x00499408u, FB(0), NAME, 2);
    return C2(0x00498d18u, dst, FB(0));
}

/* one car (id byte at idp, read again for each name): its DPC at *dpc with TPC page *page, its
 * second file at *aux; size_slot / page_slot are the loop's frame slots */
static void car(const ShEnv* e, uint32_t fb, uint32_t idp, uint32_t* dpc, uint32_t* page, uint32_t* aux,
                uint32_t size_slot, uint32_t page_slot) {
    sprintf5(e, 0x004e2300u, 0x004e22f8u, car_stem((int8_t)R8(idp)), 0x004e22f4u);
    uint32_t n = load(e, fb, *dpc);
    W32(FB(size_slot), n);
    W32(DPC_SIZE, R32(DPC_SIZE) + n);
    W32(FB(page_slot), C2(0x00496fd8u, *dpc, *page));
    (*page)++;
    sprintf5(e, 0x004e22b3u, 0x004e22abu, car_stem((int8_t)R8(idp)), 0x004e22a7u);
    uint32_t m = load(e, fb, *aux);
    W32(AUX_SIZE, R32(AUX_SIZE) + m);
    C2(0x004502b0u, *aux, R32(FB(page_slot)));
    *aux += m;
    C1(0x0049ee90u, *aux);
    *dpc += R32(FB(size_slot));
}

void sh5_level_load(const ShEnv* e, uint32_t fb, uint32_t gs) {
    uint32_t aux = 0x00626018u, dpc = 0x0074af98u;
    W32(FB(0x140), gs);
    C1(0x0049ee90u, aux);
    sprintf5(e, 0x004e229fu, 0x004e2296u, level_stem(R8(gs + 0x1f)), 0x004e2292u);
    uint32_t n0 = load(e, fb, dpc);
    W32(FB(0x12c), n0);
    W32(DPC_SIZE, n0);
    uint32_t objs = C2(0x00496fd8u, dpc, 0);
    sprintf5(e, 0x004e2239u, 0x004e2230u, level_stem(R8(gs + 0x1f)), 0x004e222cu);
    uint32_t m0 = load(e, fb, aux);
    W32(AUX_SIZE, m0);
    C2(0x004575c0u, m0, 0x1000);
    static const uint32_t resets[7] = {0x00452d90u, 0x004707f8u, 0x00492df4u, 0x00489a00u, 0x004a129cu,
                                       0x00493aa4u, 0x00499960u};
    for (int k = 0; k < 7; k++) C0(resets[k]);
    /* the slot table setup gets eax = 0x0046cb9c's result and edx = 0x1000, left in edx since the
     * call to 0x004575c0 (every call in between keeps it) */
    C2(0x00496718u, C0(0x0046cb9cu), 0x1000);
    aux += m0;
    {   /* level entity: (0, DPC size, aux size, aux end; 0, 0, 1.0f, 1.0f, 0x46, 0x23) */
        uint32_t a[10] = {0, n0, m0, aux, 0, 0, 0x3f800000u, 0x3f800000u, 0x46, 0x23};
        if (e->call) e->call(e->user, 0x00494610u, 10, a);
    }
    dpc += n0;
    C1(0x0049456cu, 0);
    static const uint32_t inits[8] = {0x00449844u, 0x00470b20u, 0x0047129cu, 0x00495328u, 0x0045029cu,
                                      0x00453b68u, 0x00447d30u, 0x00410700u};
    for (int k = 0; k < 8; k++) C0(inits[k]);
    C1(0x0044f130u, objs);
    C0(0x00412dacu);
    C0(0x00451458u);
    C0(0x00413960u);
    C0(0x004133d0u);
    W32(FB(0x14c), 0);          /* human index (ebp-8) */
    C1(0x0049ee90u, aux);
    uint32_t page = 1;
    W32(FB(0x148), gs);         /* human pointer (ebp-0xc) */
    while ((int32_t)R8(R32(FB(0x140)) + 1) > (int32_t)R32(FB(0x14c))) {
        car(e, fb, R32(FB(0x148)) + 5, &dpc, &page, &aux, 0x134, 0x130);
        W32(FB(0x148), R32(FB(0x148)) + 1);
        W32(FB(0x14c), R32(FB(0x14c)) + 1);
    }
    W32(FB(0x150), R32(FB(0x140)));     /* opponent pointer (ebp-4) */
    W32(FB(0x144), 0);                  /* opponent index (ebp-0x10) */
    while ((int32_t)R8(R32(FB(0x140)) + 2) > (int32_t)R32(FB(0x144))) {
        car(e, fb, R32(FB(0x150)) + 0xd, &dpc, &page, &aux, 0x13c, 0x138);
        W32(FB(0x150), R32(FB(0x150)) + 1);
        W32(FB(0x144), R32(FB(0x144)) + 1);
    }
    C0(0x00497390u);
}
