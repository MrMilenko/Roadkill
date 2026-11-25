/*
 * pe.h: read only view of a 32 bit PE executable (the player's TM2.EXE) by virtual address.
 * Watcom's linker leaves VirtualSize at 0, so sections are sized by their raw data.
 */
#ifndef TM2_PE_H
#define TM2_PE_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t* data;
    size_t size;
    uint32_t image_base;
    int nsec;
    struct { uint32_t va, raw_size, raw_off; } sec[16];
} PeImage;

/* 0 on success; data must stay alive while the image is used */
int pe_open(PeImage* pe, const uint8_t* data, size_t size);

/* bytes at a virtual address with at least len bytes after it, or NULL */
const uint8_t* pe_at(const PeImage* pe, uint32_t va, size_t len);

#endif /* TM2_PE_H */
