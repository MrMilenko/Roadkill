/*
 * host/qol/display.c: the display switches' settings (host/qol/display.h). Port code, not from the original.
 */
#include "display.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "engine/en10_viewport.h"
#include "host/settings/settings.h"

static struct {
    int done;
    double aspect;
    int scale;
} Q;

static void load(void)
{
    if (Q.done) return;
    Q.done = 1;
    const char* s = settings_str("display.widescreen", "0");
    double a = 0;
    if (s) {
        double w = 0, h = 0;
        if (sscanf(s, "%lf:%lf", &w, &h) == 2 && w > 0 && h > 0) a = w / h;
        else if (atoi(s) == 1) a = 16.0 / 9.0;
        else if (atof(s) > 1.0) a = atof(s);   /* an aspect as a number, e.g. 1.78 */
    }
    if (a > 4.0 / 3.0 + 1e-6) {
        if (a > 2.5) a = 2.5;
        Q.aspect = a;
        en10_widescreen_aspect = a;   /* the view setup's field of view (en10_viewport.c) */
    }
    int k = settings_int("display.internal_scale", 1);
    if (k < 1) k = 1;
    if (k > QOL_DISPLAY_MAX_SCALE) k = QOL_DISPLAY_MAX_SCALE;
    Q.scale = k;
    if (Q.aspect > 0 || Q.scale > 1)
        fprintf(stderr, "roadkill: display: widescreen %s (%.3f), internal scale %d\n", Q.aspect > 0 ? "on" : "off",
                Q.aspect > 0 ? Q.aspect : 4.0 / 3.0, Q.scale);
}

double qol_display_aspect(void) { load(); return Q.aspect; }
int qol_display_scale(void) { load(); return Q.scale; }
int qol_display_active(void) { load(); return Q.aspect > 0 || Q.scale > 1; }

int qol_display_wide_width(int w, int h)
{
    load();
    if (Q.aspect <= 0 || w <= 0 || h <= 0 || w * 3 != h * 4) return w;
    return 2 * (int)lround(h * Q.aspect / 2.0);
}
