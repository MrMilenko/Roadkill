/*
 * bind_match_mt2_world.c: cars_update_all / worldUpdate 0x00458470 (src/match/mt2_world.c),
 * called the way tools/emu/diff_mt2_world.py called it: no arguments; blk = rt_ptr(0x00502f00),
 * the input latch rt_ptr(0x004e7420), fn[] the 70 callees in mt2_world.h's order.
 *
 * Callback arguments a..e go to EAX, EDX, EBX, ECX, then stack word 0 (only car_collide
 * 0x0049cec0 takes one). Host pointers into the address space are passed as game addresses.
 * The radar offset (0x00436a40's EDX) and the viewer position (0x00494c5c's EAX) are int32[3]
 * locals of the port (the original's are on its stack): copied into a frame for the call and back.
 */
#include "rt_env.h"
#include "match/mt2_world.h"

typedef char mt2w_n_is_70[(MT2W_N == 70) ? 1 : -1];
static const uint32_t addr_of[MT2W_N] = {
    0x00474508u, 0x0045606cu, 0x00472b70u, 0x00417734u, 0x004225a0u, 0x00436f60u, 0x00436f80u, 0x00436f98u,
    0x00415f00u, 0x0048ce88u, 0x004632b0u, 0x00473254u, 0x00463f78u, 0x00473534u, 0x00437850u, 0x0047068cu,
    0x004899c2u, 0x00436a40u, 0x0045b5f0u, 0x00453c00u, 0x00453c38u, 0x00451058u, 0x0045a39cu, 0x0046ca68u,
    0x004632d4u, 0x004327fcu, 0x00432838u, 0x004931d4u, 0x0048caecu, 0x0046cc54u, 0x0043cdc0u, 0x00494c5cu,
    0x0045c56cu, 0x0045c350u, 0x00493694u, 0x00422e08u, 0x0046f918u, 0x004325c8u, 0x0043a6d8u, 0x00438320u,
    0x00421920u, 0x0046f898u, 0x0044df10u, 0x004498d8u, 0x0049cec0u, 0x00419604u, 0x0041f40cu, 0x0041fc3cu,
    0x0042026cu, 0x0048b5a0u, 0x0048b71cu, 0x0048c9f4u, 0x0046fc04u, 0x0046fcb0u, 0x00493038u, 0x0046fca8u,
    0x0046fca0u, 0x0046f8b0u, 0x0046f94cu, 0x0046f958u, 0x00493038u, 0x0044a59cu, 0x0044ad14u, 0x0044bb18u,
    0x0044bbe4u, 0x0044bd3cu, 0x0044cee0u, 0x0044d1f4u, 0x0045450cu, 0x00455050u,
};

static int inside(uintptr_t v) { return v >= (uintptr_t)rt_base && v < (uintptr_t)rt_base + RT_SIZE; }
static uint32_t g(uintptr_t v) { return inside(v) ? (uint32_t)(v - (uintptr_t)rt_base) : (uint32_t)v; }

static uintptr_t wcall(int id, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e)
{
    uintptr_t r[5] = {a, b, c, d, e};
    int vec = id == MT2W_RADAR_BLIP ? 1 : id == MT2W_VIEWER_POS ? 0 : -1;
    uint32_t f = 0;
    if (vec >= 0 && r[vec] && !inside(r[vec])) {
        f = rt_frame_push(16);
        memcpy(rt_ptr(f), (const void*)r[vec], 12);
    }
    uint32_t w[5];
    for (int i = 0; i < 5; i++) w[i] = (f && i == vec) ? f : g(r[i]);
    uint32_t res = rt_call(addr_of[id], w, id == MT2W_COLLIDE ? 5 : 4);
    if (f) {
        memcpy((void*)r[vec], rt_ptr(f), 12);
        rt_frame_pop(16);
    }
    return res;
}

/* one callback per slot: the slot names the callee */
static uintptr_t cb_0(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(0, a, b, c, d, e); }
static uintptr_t cb_1(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(1, a, b, c, d, e); }
static uintptr_t cb_2(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(2, a, b, c, d, e); }
static uintptr_t cb_3(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(3, a, b, c, d, e); }
static uintptr_t cb_4(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(4, a, b, c, d, e); }
static uintptr_t cb_5(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(5, a, b, c, d, e); }
static uintptr_t cb_6(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(6, a, b, c, d, e); }
static uintptr_t cb_7(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(7, a, b, c, d, e); }
static uintptr_t cb_8(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(8, a, b, c, d, e); }
static uintptr_t cb_9(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(9, a, b, c, d, e); }
static uintptr_t cb_10(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(10, a, b, c, d, e); }
static uintptr_t cb_11(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(11, a, b, c, d, e); }
static uintptr_t cb_12(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(12, a, b, c, d, e); }
static uintptr_t cb_13(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(13, a, b, c, d, e); }
static uintptr_t cb_14(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(14, a, b, c, d, e); }
static uintptr_t cb_15(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(15, a, b, c, d, e); }
static uintptr_t cb_16(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(16, a, b, c, d, e); }
static uintptr_t cb_17(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(17, a, b, c, d, e); }
static uintptr_t cb_18(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(18, a, b, c, d, e); }
static uintptr_t cb_19(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(19, a, b, c, d, e); }
static uintptr_t cb_20(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(20, a, b, c, d, e); }
static uintptr_t cb_21(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(21, a, b, c, d, e); }
static uintptr_t cb_22(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(22, a, b, c, d, e); }
static uintptr_t cb_23(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(23, a, b, c, d, e); }
static uintptr_t cb_24(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(24, a, b, c, d, e); }
static uintptr_t cb_25(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(25, a, b, c, d, e); }
static uintptr_t cb_26(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(26, a, b, c, d, e); }
static uintptr_t cb_27(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(27, a, b, c, d, e); }
static uintptr_t cb_28(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(28, a, b, c, d, e); }
static uintptr_t cb_29(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(29, a, b, c, d, e); }
static uintptr_t cb_30(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(30, a, b, c, d, e); }
static uintptr_t cb_31(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(31, a, b, c, d, e); }
static uintptr_t cb_32(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(32, a, b, c, d, e); }
static uintptr_t cb_33(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(33, a, b, c, d, e); }
static uintptr_t cb_34(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(34, a, b, c, d, e); }
static uintptr_t cb_35(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(35, a, b, c, d, e); }
static uintptr_t cb_36(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(36, a, b, c, d, e); }
static uintptr_t cb_37(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(37, a, b, c, d, e); }
static uintptr_t cb_38(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(38, a, b, c, d, e); }
static uintptr_t cb_39(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(39, a, b, c, d, e); }
static uintptr_t cb_40(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(40, a, b, c, d, e); }
static uintptr_t cb_41(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(41, a, b, c, d, e); }
static uintptr_t cb_42(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(42, a, b, c, d, e); }
static uintptr_t cb_43(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(43, a, b, c, d, e); }
static uintptr_t cb_44(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(44, a, b, c, d, e); }
static uintptr_t cb_45(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(45, a, b, c, d, e); }
static uintptr_t cb_46(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(46, a, b, c, d, e); }
static uintptr_t cb_47(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(47, a, b, c, d, e); }
static uintptr_t cb_48(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(48, a, b, c, d, e); }
static uintptr_t cb_49(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(49, a, b, c, d, e); }
static uintptr_t cb_50(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(50, a, b, c, d, e); }
static uintptr_t cb_51(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(51, a, b, c, d, e); }
static uintptr_t cb_52(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(52, a, b, c, d, e); }
static uintptr_t cb_53(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(53, a, b, c, d, e); }
static uintptr_t cb_54(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(54, a, b, c, d, e); }
static uintptr_t cb_55(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(55, a, b, c, d, e); }
static uintptr_t cb_56(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(56, a, b, c, d, e); }
static uintptr_t cb_57(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(57, a, b, c, d, e); }
static uintptr_t cb_58(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(58, a, b, c, d, e); }
static uintptr_t cb_59(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(59, a, b, c, d, e); }
static uintptr_t cb_60(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(60, a, b, c, d, e); }
static uintptr_t cb_61(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(61, a, b, c, d, e); }
static uintptr_t cb_62(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(62, a, b, c, d, e); }
static uintptr_t cb_63(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(63, a, b, c, d, e); }
static uintptr_t cb_64(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(64, a, b, c, d, e); }
static uintptr_t cb_65(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(65, a, b, c, d, e); }
static uintptr_t cb_66(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(66, a, b, c, d, e); }
static uintptr_t cb_67(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(67, a, b, c, d, e); }
static uintptr_t cb_68(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(68, a, b, c, d, e); }
static uintptr_t cb_69(void* u, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) { (void)u; return wcall(69, a, b, c, d, e); }

static const Mt2WFn cbs[MT2W_N] = {
    cb_0, cb_1, cb_2, cb_3, cb_4, cb_5, cb_6, cb_7,
    cb_8, cb_9, cb_10, cb_11, cb_12, cb_13, cb_14, cb_15,
    cb_16, cb_17, cb_18, cb_19, cb_20, cb_21, cb_22, cb_23,
    cb_24, cb_25, cb_26, cb_27, cb_28, cb_29, cb_30, cb_31,
    cb_32, cb_33, cb_34, cb_35, cb_36, cb_37, cb_38, cb_39,
    cb_40, cb_41, cb_42, cb_43, cb_44, cb_45, cb_46, cb_47,
    cb_48, cb_49, cb_50, cb_51, cb_52, cb_53, cb_54, cb_55,
    cb_56, cb_57, cb_58, cb_59, cb_60, cb_61, cb_62, cb_63,
    cb_64, cb_65, cb_66, cb_67, cb_68, cb_69,
};

static uint8_t* w_resolve(void* u, uint32_t h) { (void)u; return rt_ptr(h); }

static uint32_t b_world_update(const uint32_t* a, int n)
{
    (void)a; (void)n;
    static Mt2World w;
    if (!w.blk) {
        w.blk = rt_ptr(MT2W_BASE);
        w.input_latch = (int32_t*)rt_ptr(0x004e7420u);
        w.resolve = w_resolve;
        for (int i = 0; i < MT2W_N; i++) w.fn[i] = cbs[i];
    }
    mt2_world_update(&w);
    return 0;
}

void rt_bind_match_mt2_world(void)
{
    rt_bind(0x00458470u, b_world_update, "cars_update_all");
}

RT_BIND_AREA(match_mt2_world)
