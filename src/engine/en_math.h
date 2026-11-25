/*
 * en_math.h: small engine leaves, ported from TM2.EXE
 *
 *   0x0048cca8 en_mat_vec_mul       (mathMulMatVec2) out[i] = fx12(row i of M . v), i32 vector
 *   0x0048cd3c en_mat_t_vec_mul_s16 (old name math_mat_vec_mul_packed) out[i] = fx12(column i of
 *                                   M . v), v three i16 (a DPC vertex)
 *   0x0048ce88 en_copy_words        (old name math_transform_point, wrong: it is a copy of n
 *                                   dwords, nothing when n <= 0)
 *   0x0048bbcc en_view_set_size     (old name viewCopyWords) stores w, h at 0x009f7808 / 0x009f780c
 *                                   and the projection scale 0x009f7818 / 0x009f781c (en_project in
 *                                   en_clip.h): software (w << 5) - 0.5, (h << 5) - 0.5; 3D device
 *                                   ([0x00c85008]) w, h
 *   0x004aab90 en_name_init         (old name mathCopyWords) i32 0, then an empty string at +4
 *                                   (copies the "" at 0x004e3370: writes +0..+4)
 *   0x0048b460 en_mesh_set_quadrants (meshSetPrimQuadrants) per polygon of a type 0 mesh node,
 *                                   bits 13..15 of word +0x14 = the quadrant of its first vertex
 * fx12 is the original's x / 4096 rounded toward zero (sar after adding 0xfff to negatives), on
 * 32 bit wrapping products and sums. Verified under emulation: tools/emu/check_engine_math.sh.
 */
#ifndef TM2_EN_MATH_H
#define TM2_EN_MATH_H

#include <stddef.h>
#include <stdint.h>

void en_mat_vec_mul(const int32_t m[9], const int32_t v[3], int32_t out[3]);
void en_mat_t_vec_mul_s16(const int32_t m[9], const int16_t v[3], int32_t out[3]);
void en_copy_words(int32_t* dst, const int32_t* src, int32_t n);

typedef struct {
    int32_t w, h;          /* 0x009f7808 / 0x009f780c */
    float scale_x, scale_y; /* 0x009f7818 / 0x009f781c */
} EnViewSize;
void en_view_set_size(EnViewSize* v, int32_t w, int32_t h, int hw);

void en_name_init(uint8_t dst[5]);

/* node: address of a type 0 mesh node in mem (mem[0] at base): +0x04 u32 vertices (8 bytes each,
 * i16 x, y, z), +0x0c u32 first polygon, +0x10 u32 polygon count. Polygon: +0x02 u8 size in
 * dwords, +0x04 i16 first vertex index, +0x14 u32 flags (bits 13..15 written). For vertex (x, y):
 * x >= 0, y >= 0: 0; x >= 0, y < 0: 1; x < 0, y >= 0: 2; x < 0, y < 0: 3; the polygons at index
 * 2, 4 and 6 get 3, 2, 1, 0 instead (ORIGINAL QUIRK, kept). Accesses outside mem read 0 / drop. */
void en_mesh_set_quadrants(uint8_t* mem, uint32_t base, size_t size, uint32_t node);

#endif /* TM2_EN_MATH_H */
