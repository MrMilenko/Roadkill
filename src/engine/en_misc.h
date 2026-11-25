/*
 * en_misc.h: small leaves of the engine range, ported from TM2.EXE. Verified under emulation:
 * tools/emu/check_engine_misc.sh. "(no entry)" = no Ghidra function there.
 *
 * Polygon record setters (r = the record's bytes; +0x00 u8 low 7 bits n, +0x34 u32 with bits
 * 24..27 = k1 and 28..30 = k2, +0x36 / +0x38 flag bytes, +0x3c dword array, +0x04 12 byte entries):
 *   0x0048e6c0 en_prim_set_flag3c    byte +0x3f bit 1 = on
 *   0x0048e6cc                       empty function (ret)
 *   0x0048e6d4 en_prim_set_38        +0x38 bit 6 = a & 1, bits 7..12 = b & 0x3f
 *   0x0048e700 en_prim_set_mode36    mode 0: +0x36 bits 0..2 = 0, +0x38 bit 0 = 0; 2: 1 and 1; else 1 and 0
 *   0x0048e74c en_prim_set_mode36b   1: +0x36 bits 3..5 = 1; 2: cleared then OR the +0x34 bits 16..18
 *                                    (as read before) into bits 19..21; else cleared
 *   0x0048e798 en_prim_set_word      u16 at +0x3c + 4 * (n + k2) = v
 *   0x0048e7b8 en_prim_set_rgb       (r, i, b, g, stack r) dword +0x3c + 4i low 24 bits = r << 16 | g << 8 | b
 *   0x0048e7ec (no entry)            dword +0x3c + 4i low 24 bits = c & 0xffffff
 *   0x0048e810 en_prim_set_vec       (r, i, x, y, stack z) +0x04 + 12i = x, y, z
 *   0x0048e830 (no entry)            the same from a pointer
 *   0x0048e854 (no entry)            three u16 from a pointer to +0x3c + 4 k1 + 8i
 *   0x0048e888 (no entry)            u16 pair at +0x3c + 4 k2 + 4i
 *   0x0048e8a4 en_prim_set_uv_rect   (r, u, v, w, stack h) four u16 pairs at +0x3c + 4 k2:
 *                                    (u, v) (u + w, v) (u + w, v + h) (u, v + h)
 *   0x0048e468 en_prim_init         (record, u8 type 0..9) clears +0x00 / +0x34..+0x37, +0x38 bits 6 and
 *                                    16..21, sets +0x3a bits 0..5, then per type the record size +2,
 *                                    +3, +1, +0x3f, the +0x36 word, +0 and the per vertex bytes +0x43 /
 *                                    +0x47 / +0x4b (types above 9: only the clearing)
 * Renderer configuration globals (EnRenderCfg, 0x004e829c..0x004e82cc):
 *   0x0048e94c, 0x0048e92c, 0x0048e934, 0x0048e93c, 0x0048e944, 0x0048e910, 0x0048e918
 *   (subsys_render_flag_b), 0x0048e920 (subsys_render_flag_c, stack argument, ret 4), 0x0048e900,
 *   0x0048e908: plain stores (all but 0x0048e920 no entry or the old names)
 *   0x0048e954 (no entry)  texture shift n: +0x4e82c4 = n, float 0x004e82c0 = 1 / (1 << n) (32 bit
 *                          shift, n & 31; n = 31 gives -2^-31)
 *   0x0048e97c            depth scale 0x004e82b4 = (float)i
 *   0x0048e990 (no entry)  returns chop(0x004e82b4) (fistp: out of range gives 0x80000000)
 * Render mode 0x004e8434: 0x0049ea60 get, 0x0049ea68 set (values 0..3 only, others ignored).
 * Text record (EnNamed, +0x58 u32, +0x5c / +0xdc / +0x15c strings, +0x1dc u32):
 *   0x004ac01c / 0x004ac044 / 0x004ac074 strcpy into +0x5c / +0xdc / +0x15c (2 bytes per step;
 *   the original's EAX afterwards is r with its low byte 0, not used), 0x004ac040 / 0x004ac06c /
 *   0x004ac09c their addresses, 0x004ac0a4 / 0x004ac0ac set / get +0x1dc, 0x004ac0b4 / 0x004ac0b8
 *   set / get +0x58.
 *   0x004ac9a8 returns 0x424d5144; 0x004ac9b0 returns 0 when the second argument is 0xf170, else 2.
 */
#ifndef TM2_EN_MISC_H
#define TM2_EN_MISC_H

#include <stdint.h>

void en_prim_init(uint8_t* r, int32_t type);
void en_prim_set_flag3c(uint8_t* r, int32_t on);
void en_prim_nop(uint8_t* r);
void en_prim_set_38(uint8_t* r, int32_t a, int32_t b);
void en_prim_set_mode36(uint8_t* r, int32_t mode);
void en_prim_set_mode36b(uint8_t* r, int32_t mode);
void en_prim_set_word(uint8_t* r, int32_t v);
void en_prim_set_rgb(uint8_t* r, int32_t i, int32_t b, int32_t g, int32_t red);
void en_prim_set_rgb24(uint8_t* r, int32_t i, int32_t c);
void en_prim_set_vec(uint8_t* r, int32_t i, int32_t x, int32_t y, int32_t z);
void en_prim_set_vecp(uint8_t* r, int32_t i, const int32_t v[3]);
void en_prim_set_s16x3(uint8_t* r, int32_t i, const uint8_t v[6]);
void en_prim_set_pair(uint8_t* r, int32_t i, int32_t a, int32_t b);
void en_prim_set_uv_rect(uint8_t* r, int32_t u, int32_t v, int32_t w, int32_t h);

/* 0x004e829c .. 0x004e82cf */
typedef struct {
    uint32_t a29c, a2a0, a2a4, a2a8, a2ac, a2b0;
    float depth_scale;      /* 0x004e82b4 */
    uint32_t flag_b;        /* 0x004e82b8 */
    uint32_t flag_c;        /* 0x004e82bc */
    float tex_inv;          /* 0x004e82c0 */
    int32_t tex_shift;      /* 0x004e82c4 */
    uint32_t a2c8, a2cc;
} EnRenderCfg;
/* off: the global's address - 0x004e829c, for the plain stores */
void en_cfg_store(EnRenderCfg* c, uint32_t off, uint32_t v);
void en_cfg_set_tex_shift(EnRenderCfg* c, int32_t n);
void en_cfg_set_depth_scale(EnRenderCfg* c, int32_t v);
int32_t en_cfg_get_depth_scale(const EnRenderCfg* c);

int32_t en_render_mode_get(const int32_t* mode);
void en_render_mode_set(int32_t* mode, int32_t v);

void en_named_set_str(uint8_t* r, uint32_t off, const char* s);   /* off 0x5c, 0xdc, 0x15c */
uint32_t en_named_addr(uint32_t r, uint32_t off);
void en_named_set32(uint8_t* r, uint32_t off, uint32_t v);        /* off 0x1dc, 0x58 */
uint32_t en_named_get32(const uint8_t* r, uint32_t off);
uint32_t en_magic(void);
int32_t en_check_f170(int32_t a, int32_t b);

#endif /* TM2_EN_MISC_H */
