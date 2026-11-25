/*
 * bind_all.c: runs every binding table that registered itself (RT_BIND_AREA in rt.h).
 */
#include "rt.h"

#include <stdio.h>
#include <stdlib.h>

#define MAX_AREAS 1024
static struct { void (*fn)(void); const char* name; } areas[MAX_AREAS];
static int nareas;

void rt_register_area(void (*fn)(void), const char* name)
{
    if (nareas == MAX_AREAS) { fprintf(stderr, "tm2: more than %d binding tables (%s)\n", MAX_AREAS, name); exit(3); }
    areas[nareas].fn = fn;
    areas[nareas].name = name;
    nareas++;
}

void rt_bind_all(void)
{
    for (int i = 0; i < nareas; i++) areas[i].fn();
    if (getenv("TM2_LIST_BINDS")) rt_list_binds(stdout);
    if (getenv("TM2_BINDS")) fprintf(stderr, "tm2: %d binding tables, %d bindings\n", nareas, rt_bind_count());
}
