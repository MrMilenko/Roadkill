/* ai_mem.h: little endian field access on raw blocks, for the src/ai ports (private). */
#ifndef TM2_AI_MEM_H
#define TM2_AI_MEM_H

#include <stdint.h>

static inline int32_t wsub(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
static inline int8_t i8(const uint8_t* b, int o) { return (int8_t)b[o]; }
static inline int16_t i16(const uint8_t* b, int o) { return (int16_t)(b[o] | b[o + 1] << 8); }
static inline int32_t i32(const uint8_t* b, int o) {
    return (int32_t)(b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24);
}
static inline void w16(uint8_t* b, int o, int32_t v) { b[o] = (uint8_t)v; b[o + 1] = (uint8_t)(v >> 8); }
static inline void w32(uint8_t* b, int o, int32_t v) {
    for (int k = 0; k < 4; k++) b[o + k] = (uint8_t)((uint32_t)v >> (8 * k));
}
static inline void rd9(const uint8_t* b, int o, int32_t m[9]) {
    for (int k = 0; k < 9; k++) m[k] = i32(b, o + 4 * k);
}

#endif /* TM2_AI_MEM_H */
