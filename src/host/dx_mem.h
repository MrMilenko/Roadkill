/*
 * dx_mem.h: game memory for the host services' buffers (surface pixels, sound buffer data): the
 * game reads and writes them by game address (Lock returns one), so they live in the runtime's
 * address space. Port code, not from the original. Chunks come from rt_heap_grow (the region the
 * game's own allocator also grows into); freed blocks are reused first fit.
 */
#ifndef TM2_HOST_DX_MEM_H
#define TM2_HOST_DX_MEM_H

#include <stdint.h>

uint32_t dx_mem_alloc(uint32_t n);   /* game address of n zeroed bytes (16 aligned), 0 when full */
void dx_mem_free(uint32_t a);

#endif
