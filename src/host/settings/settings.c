/*
 * host/settings/settings.c: roadkill.ini and the environment behind settings.h (port code, not
 * from the original game). The key table below is the one list of every setting: its section
 * and name, the environment variable that overrides it, its default and the comment written
 * into a new settings file.
 */
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#include "host/settings/settings.h"

#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include "host/os/os.h"
#ifdef _WIN32
#include <direct.h>
#define mkdir_one(p) _mkdir(p)
#define SEP '\\'
#else
#define mkdir_one(p) mkdir((p), 0755)
#define SEP '/'
#endif

enum {
    ENV_VALUE = 0,   /* the variable's text is the value */
    ENV_INVERT = 1,  /* a boolean the variable switches off: TM2_NOJOY=1 is controller = 0 */
    ENV_PRESENT = 2, /* a boolean that is on when the variable is set at all (unless "0") */
};

typedef struct {
    const char* key;   /* section.name */
    const char* env;
    int env_kind;
    const char* def;   /* written into a new file ("" = empty) */
    const char* help;
} Key;

/* Every [debug] key is written commented out: they are for tests and diagnosis. */
static const Key keys[] = {
    {"display.fullscreen", "TM2_FULLSCREEN", ENV_VALUE, "0", "1: full screen (the desktop's resolution, the picture scaled to fit)"},
    {"display.scale", "TM2_SCALE", ENV_VALUE, "2", "window size as a whole multiple of the game's resolution (1, 2, 3, ...)"},
    {"display.window_width", "TM2_WINDOW_WIDTH", ENV_VALUE, "0", "window size in pixels instead of the scale (0: use the scale)"},
    {"display.window_height", "TM2_WINDOW_HEIGHT", ENV_VALUE, "0", NULL},
    {"display.vsync", "TM2_VSYNC", ENV_VALUE, "1", "1: present in step with the display's refresh"},
    {"display.widescreen", "TM2_WIDESCREEN", ENV_VALUE, "0", "1: show more of the arena left and right on wide screens (0: 4:3 as the original)"},
    {"display.internal_scale", "TM2_INTERNAL_SCALE", ENV_VALUE, "1", "the 3D view drawn at this multiple of the game's resolution (1: as the original)"},
    {"display.skip_movies", "TM2_SKIP_MOVIES", ENV_VALUE, "0", "1: skip the movies (logo, intro, endings) (0: play them as the original)"},

    {"audio.sound", "TM2_NOSOUND", ENV_INVERT, "1", "0: no sound at all (no effects, no CD music)"},
    {"audio.master_volume", "TM2_VOLUME", ENV_VALUE, "100", "0..100, applied on top of the game's own volume settings"},
    {"audio.effects_volume", "TM2_SFX_VOLUME", ENV_VALUE, "100", "0..100, the sound effects"},
    {"audio.music_volume", "TM2_MUSIC_VOLUME", ENV_VALUE, "100", "0..100, the CD music"},

    {"paths.data", "TM2_DATA", ENV_VALUE, "", "the folder with the game's files (TM2.EXE, SINTAB.DAT, ...). Empty: data/ beside Roadkill, else data/ in this folder"},
    {"paths.cd_image", "TM2_CDIMAGE", ENV_VALUE, "", "the disc image's .cue for the music (empty: the first .cue in the data folder, else its parent)"},
    {"paths.registry", "TM2_REGISTRY", ENV_VALUE, "", "the file with the game's saved settings (empty: registry.txt in this folder)"},

    {"input.controller", "TM2_NOJOY", ENV_INVERT, "1", "1: game controllers work as joysticks, and the game uses a connected one without a visit to its input screen first\n; 0: no controllers"},

    {"net.port", "TM2_NET_PORT", ENV_VALUE, "27420", "the UDP port network play listens on (the next free one when it is taken, up to 9 more);\n; LAN games are looked for on this port and the next 3"},
    {"net.join", "TM2_NET_JOIN", ENV_VALUE, "", "a host to look for sessions at directly, host:port (a name or an address; empty: the local network only)"},
    {"net.lan", "TM2_NET_LAN", ENV_VALUE, "1", "1: look for sessions on the local network (a broadcast while the join screen is open)"},
    {"net.announce", "TM2_NET_ANNOUNCE", ENV_VALUE, "roadkill.horrible.dev:27400", "the announce server internet sessions are listed and introduced through, host:port (empty: none,\n; LAN and [net] join only)"},
    {"net.public", "TM2_NET_PUBLIC", ENV_VALUE, "1", "1: a session this machine hosts is announced on that server (anyone can see and join it; a\n; password still applies); 0: not announced"},
    {"net.timeout", "TM2_NET_TIMEOUT", ENV_VALUE, "10", "seconds without a word from another machine before it is dropped from the session"},
    {"net.session_name", "TM2_NET_SESSION_NAME", ENV_VALUE, "", "the name of a game you host (empty: what you typed last in the game)"},
    {"player.name", "TM2_PLAYER_NAME", ENV_VALUE, "", "your driver name in network games (empty: what you typed last in the game)"},
    {"player.alias", "TM2_PLAYER_ALIAS", ENV_VALUE, "", "your driver alias in network games (empty: what you typed last in the game)"},

    {"gameplay.loop_music", "TM2_LOOP_MUSIC", ENV_VALUE, "0", "1: a level's CD track starts again at once when it ends (the original's CD monitor restarts it after up to 3 seconds of silence)"},
    {"gameplay.fix_crashes", "TM2_FIX_CRASHES", ENV_VALUE, "0", "1: guard the original's own crash / hang bugs (docs/QOL.md lists them); 0: as the original"},
    {"gameplay.net_drop_stale", "TM2_NET_DROP_STALE", ENV_VALUE, "0", "1: network play drops another car's state packet that arrives after a newer one (0: as DirectPlay, late packets snap the car back)"},

    {"debug.keep_active", "TM2_KEEP_ACTIVE", ENV_PRESENT, "0", "1: the game never sees the window deactivate (the original pauses)"},
    {"debug.keys", "TM2_KEYS", ENV_VALUE, "", "scripted key presses frame:KEY[:hold], ... (src/host/win32_keys.c)"},
    {"debug.keys_log", "TM2_KEYS_LOG", ENV_PRESENT, "0", NULL},
    {"debug.shot_every", "TM2_SHOT_EVERY", ENV_VALUE, "0", "a screenshot every n frames into shot_dir"},
    {"debug.shot_dir", "TM2_SHOT_DIR", ENV_VALUE, "shots", NULL},
    {"debug.shot", "TM2_SHOT", ENV_VALUE, "", "one screenshot into this file at shot_frame"},
    {"debug.shot_frame", "TM2_SHOT_FRAME", ENV_VALUE, "100", NULL},
    {"debug.pixel", "TM2_PIXEL", ENV_VALUE, "565", "the 16 bit pixel format the DirectDraw host offers (565 or 555)"},
    {"debug.bpp", "TM2_BPP", ENV_VALUE, "16", "the desktop depth GetDeviceCaps answers"},
    {"debug.vblank", "TM2_VBLANK", ENV_VALUE, "1", "0: WaitForVerticalBlank does not wait"},
    {"debug.cd_letter", "TM2_CD", ENV_VALUE, "D", "the CD drive letter the game sees"},
    {"debug.cmdline", "TM2_CMDLINE", ENV_VALUE, "", "arguments after TM2.EXE on the game's command line"},
    {"debug.msgbox", "TM2_MSGBOX", ENV_VALUE, "", "every MessageBox answers this button id"},
    {"debug.debug", "TM2_DEBUG", ENV_VALUE, "0", "1: the game's debug prints on stderr"},
    {"debug.dx_log", "TM2_DX_LOG", ENV_VALUE, "0", NULL},
    {"debug.cd_log", "TM2_CD_LOG", ENV_VALUE, "0", NULL},
    {"debug.net_log", "TM2_NET_LOG", ENV_VALUE, "0", "1: the network provider's log (sessions, peers, players, refused packets) on stderr"},
    {"debug.trace_files", "TM2_TRACE_FILES", ENV_PRESENT, "0", NULL},
    {"debug.pad", "TM2_PAD", ENV_VALUE, "", "the virtual pad's script (src/host/input_joy.c)"},
    {"debug.pad_log", "TM2_PAD_LOG", ENV_PRESENT, "0", NULL},
    {"debug.virtual_pad", "TM2_VIRTUAL_PAD", ENV_VALUE, "0", "1: attach a scripted virtual controller"},
};
#define NKEYS ((int)(sizeof keys / sizeof keys[0]))

static const char* const sections[][2] = {
    {"display", "The window"},
    {"audio", "Sound"},
    {"paths", "Where things are"},
    {"input", "Controllers"},
    {"net", "Network play (Multi Player)"},
    {"gameplay", "Changes to the original game, each off by default"},
    {"debug", "Tests and diagnosis (remove the ; to use one)"},
};

/* ---- the loaded file ---- */

typedef struct { char* key; char* value; } Entry;
static Entry* ents;
static int nents;
static char dir[1024], file[1100];
static pthread_once_t once = PTHREAD_ONCE_INIT;

static const Key* key_find(const char* k)
{
    for (int i = 0; i < NKEYS; i++) if (!strcasecmp(keys[i].key, k)) return &keys[i];
    return NULL;
}

static char* trim(char* s)
{
    while (isspace((unsigned char)*s)) s++;
    size_t l = strlen(s);
    while (l && isspace((unsigned char)s[l - 1])) s[--l] = 0;
    if (l >= 2 && ((s[0] == '"' && s[l - 1] == '"') || (s[0] == '\'' && s[l - 1] == '\''))) {
        s[l - 1] = 0;
        s++;
    }
    return s;
}

/* mkdir -p */
static void make_dirs(const char* path)
{
    char p[1024];
    snprintf(p, sizeof p, "%s", path);
    for (char* q = p + 1; *q; q++)
        if (*q == '/' || *q == '\\') {
            char c = *q;
            *q = 0;
            mkdir_one(p);
            *q = c;
        }
    mkdir_one(p);
}

static void find_dir(void)
{
#if defined(ROADKILL_XBOX)
    snprintf(dir, sizeof dir, "D:");   /* beside the XBE */
#elif defined(_WIN32)
    const char* a = getenv("APPDATA");
    snprintf(dir, sizeof dir, "%s\\Roadkill", a && *a ? a : ".");
#elif defined(__APPLE__)
    const char* h = getenv("HOME");
    snprintf(dir, sizeof dir, "%s/Library/Application Support/Roadkill", h && *h ? h : ".");
#else
    const char* h = getenv("HOME");
    const char* x = getenv("XDG_DATA_HOME");
    if (x && *x) snprintf(dir, sizeof dir, "%s/roadkill", x);
    else snprintf(dir, sizeof dir, "%s/.local/share/roadkill", h && *h ? h : ".");
#endif
    make_dirs(dir);
}

static void write_defaults(const char* path)
{
    FILE* f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "roadkill: cannot write %s: %s\n", path, strerror(errno));
        return;
    }
    fprintf(f, "; Roadkill settings. Written with the defaults on the first run; edit freely.\n"
               "; Environment variables (TM2_SCALE=3, ...) override what is here.\n"
               "; With every value as written here Roadkill plays like the 1997 PC game.\n");
    for (size_t s = 0; s < sizeof sections / sizeof sections[0]; s++) {
        const char* sec = sections[s][0];
        size_t sl = strlen(sec);
        int dbg = !strcmp(sec, "debug");
        fprintf(f, "\n; %s\n[%s]\n", sections[s][1], sec);
        for (int i = 0; i < NKEYS; i++) {
            const Key* k = &keys[i];
            if (strncmp(k->key, sec, sl) || k->key[sl] != '.') continue;
            if (k->help) fprintf(f, "; %s\n", k->help);
            fprintf(f, "%s%s =%s%s\n", dbg ? "; " : "", k->key + sl + 1, *k->def ? " " : "", k->def);
        }
    }
    fclose(f);
    fprintf(stderr, "roadkill: wrote the default settings to %s\n", path);
}

static const char* file_value(const char* key)
{
    for (int i = nents - 1; i >= 0; i--)
        if (!strcasecmp(ents[i].key, key)) return ents[i].value;
    return NULL;
}

static void load(void)
{
    find_dir();
    const char* f = getenv("TM2_SETTINGS");
    if (f && *f) snprintf(file, sizeof file, "%s", f);
    else snprintf(file, sizeof file, "%s%croadkill.ini", dir, SEP);
    FILE* in = fopen(file, "r");
    if (!in) {
        write_defaults(file);
        in = fopen(file, "r");
    }
    char sec[64] = "", line[2048];
    int ln = 0;
    while (in && fgets(line, sizeof line, in)) {
        ln++;
        char* s = trim(line);
        if (!*s || *s == ';' || *s == '#') continue;
        if (*s == '[') {
            char* e = strchr(s, ']');
            if (e) *e = 0;
            snprintf(sec, sizeof sec, "%s", trim(s + 1));
            continue;
        }
        char* eq = strchr(s, '=');
        if (!eq) { fprintf(stderr, "roadkill: %s:%d: not a setting: %s\n", file, ln, s); continue; }
        *eq = 0;
        char k[200];
        snprintf(k, sizeof k, "%s.%s", sec, trim(s));
        for (char* p = k; *p; p++) *p = (char)tolower((unsigned char)*p);
        if (!key_find(k)) fprintf(stderr, "roadkill: %s:%d: unknown setting %s (kept)\n", file, ln, k);
        ents = realloc(ents, sizeof(Entry) * (size_t)(nents + 1));
        ents[nents].key = strdup(k);
        ents[nents].value = strdup(trim(eq + 1));
        nents++;
    }
    if (in) fclose(in);
    /* the game data folder is also read by runtime code outside the host (src/rt's data
     * checks): hand a file setting on through the variable they read */
}

static void init(void) { pthread_once(&once, load); }

/* ---- lookups ---- */

static const char* env_of(const char* key, const Key* k, char* buf, size_t cap)
{
    const char* name = k ? k->env : NULL;
    if (!k) {   /* TM2_<NAME> */
        const char* dot = strrchr(key, '.');
        snprintf(buf, cap, "TM2_%s", dot ? dot + 1 : key);
        for (char* p = buf; *p; p++) *p = (char)toupper((unsigned char)*p);
        name = buf;
    }
    if (!name) return NULL;
    const char* v = getenv(name);
    if (!v) return NULL;
    int kind = k ? k->env_kind : ENV_VALUE;
    if (kind == ENV_PRESENT) return strcmp(v, "0") ? "1" : "0";
    if (!*v) return NULL;
    if (kind == ENV_INVERT) return (*v == '1' || !strcasecmp(v, "on") || !strcasecmp(v, "yes") || !strcasecmp(v, "true")) ? "0" : "1";
    return v;
}

const char* settings_str(const char* key, const char* def)
{
    init();
    char buf[96];
    const Key* k = key_find(key);
    const char* v = env_of(key, k, buf, sizeof buf);
    if (v) return v;   /* getenv's text or a literal: valid for the run */
    v = file_value(key);
    return v && *v ? v : def;
}

static int parse_int(const char* s, int* out)
{
    if (!strcasecmp(s, "on") || !strcasecmp(s, "yes") || !strcasecmp(s, "true")) { *out = 1; return 1; }
    if (!strcasecmp(s, "off") || !strcasecmp(s, "no") || !strcasecmp(s, "false")) { *out = 0; return 1; }
    char* e;
    long v = strtol(s, &e, 0);
    if (e == s) return 0;
    *out = (int)v;
    return 1;
}

int settings_int(const char* key, int def)
{
    const char* s = settings_str(key, NULL);
    int v;
    if (!s || !parse_int(s, &v)) return def;
    return v;
}

int settings_bool(const char* key, int def) { return settings_int(key, def) != 0; }

const char* settings_dir(void) { init(); return dir; }
const char* settings_file(void) { init(); return file; }

/* ---- the game's registry file ---- */

const char* settings_registry_file(void)
{
    static char reg[1200];
    if (reg[0]) return reg;
    const char* r = settings_str("paths.registry", NULL);
    if (r) { snprintf(reg, sizeof reg, "%s", r); return reg; }
    snprintf(reg, sizeof reg, "%s%cregistry.txt", settings_dir(), SEP);
    /* one time move from where Roadkill kept it before (a copy: the old file stays) */
    struct stat st;
    if (stat(reg, &st) == 0) return reg;
    const char* x = getenv("XDG_CONFIG_HOME");
    const char* h = getenv("HOME");
    char old[1100];
    if (x && *x) snprintf(old, sizeof old, "%s/opentm2/registry.txt", x);
    else snprintf(old, sizeof old, "%s/.config/opentm2/registry.txt", h ? h : ".");
    FILE* in = fopen(old, "rb");
    if (!in) return reg;
    FILE* out = fopen(reg, "wb");
    if (out) {
        char b[4096];
        size_t n;
        while ((n = fread(b, 1, sizeof b, in)) > 0) fwrite(b, 1, n, out);
        fclose(out);
        fprintf(stderr, "roadkill: the game's saved settings copied from %s to %s\n", old, reg);
    }
    fclose(in);
    return reg;
}
