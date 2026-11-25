/*
 * cb2_arena_setup.h: arenaSetup, TM2.EXE 0x004576a8 (PS1 decomp: arenaSetup; old OpenTM2 name
 * vehicle_slots_setup, wrong). Argument al = event (arena) number, used sign extended from a byte.
 *
 * Sequence:
 *   1. 0x0050e0b4[8] = 0 and 0x0050e0d4[8] = 0 (i32).
 *   2. radar_init 0x004367f4().
 *   3. for i < *(i32*)0x00503084 (read again after every call): objRegistryChoiceRegister
 *      0x00470650(obj 0x00503048[i], i16 0x00503028[i] sign extended)  (Ghidra shows the i16 as
 *      `*(int*)(0x00503026 + 2i) >> 16`).
 *   4. rt_screen_buffer 0x0046cb9c() (result unused); level_entity_create 0x00494610(0; stack 0, 0,
 *      1.0f, 1.0f, 0x46, 0x23; ret 0x18): DECOMPILER TRAP: Ghidra shows one argument, the callee
 *      takes six more on the stack. t = level_textures_setup 0x0049456c(0).
 *   5. If the root entity 0x0050c2b8 is 0: 0x0050c2b8 = entity_alloc 0x00492d90(t), then
 *      view_set_target 0x00494bfc(entity, 0).
 *   6. 0x00502fb4[9] = -1 (i32).
 *   7. Same event as 0x005075e4 (i32 compare of the sign extended byte): startNodesClearUsed
 *      0x0043bd60() only. Else 0x005075e4 = event, set_621168 0x0046fc90(event),
 *      model_set_position 0x004952f4(0, 0xAF, 0xAF, 0xAF), lightSetEnvDirection 0x0049517c(0, j,
 *      0, az) for j = 0, 1, 2 with az 0x3B, 0x12, 0x15, light_set_pos 0x0049527c(0, j, 0x4B, 0x4B;
 *      stack 0x4B), then per event ((uint8)(event - 1) <= 11, jump table 0x00457678): a fog
 *      colour fog_set_color 0x00494d80(r, g, b), the byte 0x0050c2c4 (events 1..8 only; 10..12
 *      leave it), three per arena setup calls, plus eiffelTowerReset 0x004125b0 (3) or
 *      worldCollapseClearDone 0x00455014 (6); events 7, 9 and anything else take the default
 *      branch (which also sets 0x00502f7c = 1 before the common tail). Common tail:
 *      event_objects_find_zones 0x004107c0(), landmark_b_mark 0x00411270(), 0x00502f7c = 0,
 *      worldWarpsSetup 0x00453fac(event).
 *   8. 0x0050c2c0 = 0; crt_srand 0x004899e6(timer_get_elapsed 0x00493038()).
 *
 * Every call goes through env->call with the callee's address and its arguments (registers eax,
 * edx, ebx, ecx in that order, then the stack arguments in push order reversed, i.e. as the
 * callee sees them from esp + 4 up).
 *
 * Verified against the original under emulation (tools/emu/check_cb2_arena_setup.sh).
 */
#ifndef TM2_CB2_ARENA_SETUP_H
#define TM2_CB2_ARENA_SETUP_H

#include <stdint.h>

typedef struct {
    int32_t* clear_a;               /* 0x0050e0b4, 8 */
    int32_t* clear_b;               /* 0x0050e0d4, 8 */
    const int32_t* slot_count;      /* 0x00503084 */
    const int16_t* slot_ids;        /* 0x00503028, i16 per slot */
    const uint32_t* slot_objs;      /* 0x00503048 */
    uint32_t* root_entity;          /* 0x0050c2b8 */
    int32_t* view_marks;            /* 0x00502fb4, 9 */
    int32_t* current_event;         /* 0x005075e4 */
    uint8_t* fog_flag;              /* 0x0050c2c4 */
    int32_t* flag_502f7c;           /* 0x00502f7c */
    int32_t* flag_50c2c0;           /* 0x0050c2c0 */
    uint32_t (*call)(void* user, uint32_t addr, const uint32_t* args, int nargs);
    void* user;
} Cb2ArenaEnv;

void cb2_arena_setup(const Cb2ArenaEnv* env, uint8_t event);

#endif /* TM2_CB2_ARENA_SETUP_H */
