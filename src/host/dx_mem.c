/*
 * dx_mem.c: first fit allocator over chunks of the game heap region (see dx_mem.h). The block
 * list is kept on the host (sorted by address), not in game memory.
 */
#include "dx_mem.h"

#include <stdlib.h>
#include <string.h>

#include "rt.h"

#define CHUNK 0x00100000u   /* 1 MB at a time from rt_heap_grow */

typedef struct { uint32_t addr, size; int used; } Blk;
static Blk* blk;
static int nblk, capblk;

static int insert_at(int i, Blk b)
{
    if (nblk == capblk) {
        int c = capblk ? capblk * 2 : 256;
        Blk* nb = realloc(blk, sizeof(Blk) * (size_t)c);
        if (!nb) return -1;
        blk = nb;
        capblk = c;
    }
    memmove(blk + i + 1, blk + i, sizeof(Blk) * (size_t)(nblk - i));
    blk[i] = b;
    nblk++;
    return 0;
}

static void remove_at(int i)
{
    memmove(blk + i, blk + i + 1, sizeof(Blk) * (size_t)(nblk - i - 1));
    nblk--;
}

uint32_t dx_mem_alloc(uint32_t n)
{
    n = (n + 15u) & ~15u;
    if (n == 0) n = 16;
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < nblk; i++) {
            if (blk[i].used || blk[i].size < n) continue;
            uint32_t a = blk[i].addr;
            if (blk[i].size > n) {
                Blk rest = {a + n, blk[i].size - n, 0};
                blk[i].size = n;
                if (insert_at(i + 1, rest) != 0) return 0;
            }
            blk[i].used = 1;
            memset(rt_ptr(a), 0, n);
            return a;
        }
        if (pass) break;
        uint32_t c = n > CHUNK ? n : CHUNK;
        uint32_t a = rt_heap_grow(c);
        if (!a) return 0;
        /* keep the list sorted; merge with a free block that ends where this chunk starts */
        int i = 0;
        while (i < nblk && blk[i].addr < a) i++;
        if (i > 0 && !blk[i - 1].used && blk[i - 1].addr + blk[i - 1].size == a) blk[i - 1].size += c;
        else if (insert_at(i, (Blk){a, c, 0}) != 0) return 0;
    }
    return 0;
}

void dx_mem_free(uint32_t a)
{
    if (!a) return;
    for (int i = 0; i < nblk; i++) {
        if (blk[i].addr != a || !blk[i].used) continue;
        blk[i].used = 0;
        if (i + 1 < nblk && !blk[i + 1].used && blk[i].addr + blk[i].size == blk[i + 1].addr) {
            blk[i].size += blk[i + 1].size;
            remove_at(i + 1);
        }
        if (i > 0 && !blk[i - 1].used && blk[i - 1].addr + blk[i - 1].size == blk[i].addr) {
            blk[i - 1].size += blk[i].size;
            remove_at(i);
        }
        return;
    }
}

static void rt_bind_host_dx_mem(void) {}

RT_BIND_AREA(host_dx_mem)
