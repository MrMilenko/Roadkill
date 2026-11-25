/*
 * host/audio/mixer.c: the mixer's levels (host/audio/mixer.h), port code. The volume is taken
 * as linear amplitude (0xffff = 1), the two channels averaged. TM2_MIXER_LOG=1
 * ([debug] mixer_log) logs every change and the gains it gives.
 */
#include "host/audio/mixer.h"

#include "host/settings/settings.h"

#include <stdio.h>

static volatile uint32_t vol[MIXER_LINES][2] = {{0xffff, 0xffff}, {0xffff, 0xffff}, {0xffff, 0xffff}};
static volatile int mute[MIXER_LINES];

static int line_ok(int line) { return line >= 0 && line < MIXER_LINES; }

uint32_t mixer_volume(int line, int channel) { return line_ok(line) ? vol[line][channel & 1] : 0; }
int mixer_muted(int line) { return line_ok(line) ? mute[line] : 0; }

/* a line's level as 0..256 */
static int level(int line)
{
    if (mute[line]) return 0;
    return (int)((vol[line][0] + vol[line][1]) * 128u / 0xffffu);
}
int mixer_effects_gain(void) { return level(MIXER_SPEAKERS) * level(MIXER_WAVE) / 256; }
int mixer_music_gain(void) { return level(MIXER_SPEAKERS) * level(MIXER_CD) / 256; }

static void logged(const char* what, int line)
{
    static const char* names[MIXER_LINES] = {"speakers", "wave", "cd"};
    if (!settings_bool("debug.mixer_log", 0)) return;
    fprintf(stderr, "tm2 mixer: %s %s: volume %04x / %04x%s -> effects gain %d, music gain %d (of 256)\n", names[line], what,
            vol[line][0], vol[line][1], mute[line] ? " muted" : "", mixer_effects_gain(), mixer_music_gain());
}

void mixer_set_volume(int line, uint32_t left, uint32_t right)
{
    if (!line_ok(line)) return;
    if (left > 0xffff) left = 0xffff;
    if (right > 0xffff) right = 0xffff;
    if (vol[line][0] == left && vol[line][1] == right) return;
    vol[line][0] = left;
    vol[line][1] = right;
    logged("volume", line);
}
void mixer_set_mute(int line, int on)
{
    if (!line_ok(line) || mute[line] == (on != 0)) return;
    mute[line] = on != 0;
    logged("mute", line);
}
