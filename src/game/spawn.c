/* spawn.c: see spawn.h */
#include "spawn.h"

#include "../core/tm2rand.h"

int32_t spawn_pick(SpawnList* s, int32_t count) {
    int32_t start = tm2_rand() % count;
    int32_t result = -1;
    int found = 0;
    for (int32_t i = start; i < count && !found; i++)
        if (!s->used[i]) { found = 1; result = s->node[i]; s->used[i] = 1; }
    for (int32_t i = 0; i < start && !found; i++)
        if (!s->used[i]) { found = 1; result = s->node[i]; s->used[i] = 1; }
    if (result < 0) result = s->node[tm2_rand() % count];
    return result;
}
