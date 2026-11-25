/*
 * en_misc2.h: small pure leaves of TM2.EXE 0x00497a28..0x00498968, ported from the machine code.
 *
 * A fixed table of 8 entries (obj +0x04 entry pointers, +0x24 count; entries 0x138 bytes:
 * +0x000 key, +0x094 block, +0x114 / +0x118 / +0x11c words):
 *   0x00498634 entry_block(i)  ptr[i] + 0x94 for 0 <= i < count, else 0
 *   0x0049864c entry_key(i)    [ptr[i]], else 0
 *   0x00498660 find_114(key)   [ptr[k] + 0x114] for the LAST k with [ptr[k]] == key, else -1
 *   0x004986ac get_118(i) / 0x004986c8 set_118(i, EBX) (no Ghidra entries), 0x004986dc get_11c(i)
 *     (else -1) / 0x004986f8 set_11c(i, EBX); the setters return EAX = ptr[i] (obj when out of range)
 *   0x00498820 remove(key): for each i < count with [ptr[i]] == key: count -= 1 and the CONTENTS of
 *     entries i+1..7 move down one (0x138 bytes each, pointers unchanged). ORIGINAL QUIRK: i is not
 *     re-checked after a removal, so of two adjacent matches the second survives
 *   0x004988c8 obj + 0xc4, 0x004988e0 obj + 0x44, 0x004988e4 [obj + 0x30], 0x00498968 [obj + 0x24]
 * A linked list (next pointer at +0x134):
 *   0x00497c50 find(head, key): first node with [node] == key, else 0; 0x00497c78 returns [EAX]
 *   0x00497a28 free_all(list): frees every node from [list] (cursor kept in +8), then +0, +4, +0xc = 0
 * And 0x004958f0: [0x00a04670] (the locked surface pitch).
 */
#ifndef TM2_EN_MISC2_H
#define TM2_EN_MISC2_H

#include <stdint.h>

typedef struct {
    uint8_t* (*mem)(void* user, uint32_t addr);
    void* user;
    void (*free_fn)(void* user, uint32_t p);   /* 0x004aa39c (free 0x004bf76f when nonzero) */
} EnMisc2Env;

uint32_t en_tab_entry_block(EnMisc2Env* e, uint32_t obj, int32_t i);
uint32_t en_tab_entry_key(EnMisc2Env* e, uint32_t obj, int32_t i);
uint32_t en_tab_find_114(EnMisc2Env* e, uint32_t obj, uint32_t key);
uint32_t en_tab_get(EnMisc2Env* e, uint32_t obj, int32_t i, uint32_t ofs);              /* 0x118 / 0x11c */
uint32_t en_tab_set(EnMisc2Env* e, uint32_t obj, int32_t i, uint32_t ofs, uint32_t v);  /* returns EAX */
void en_tab_remove(EnMisc2Env* e, uint32_t obj, uint32_t key);
uint32_t en_list_find(EnMisc2Env* e, uint32_t head, uint32_t key);
uint32_t en_word_at(EnMisc2Env* e, uint32_t a);
uint32_t en_list_free_all(EnMisc2Env* e, uint32_t list);

#endif /* TM2_EN_MISC2_H */
