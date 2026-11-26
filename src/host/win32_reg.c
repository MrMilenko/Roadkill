/*
 * win32_reg.c: the ADVAPI32 registry imports on a small text file (port code).
 *
 * The file is settings_registry_file() (settings.h: [paths] registry / TM2_REGISTRY, else
 * registry.txt next to roadkill.ini, moved there once from ~/.config/opentm2). One line per
 * key ("K\t<path>") and per value ("V\t<key path>\t<name>\t<type>\t<hex bytes>"); key
 * paths are "HKLM\Software\...", compared case insensitively. It is read at the first registry
 * call and written after every change.
 */
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#include "win32_host.h"

#include "host/qol/qol.h"
#include "host/settings/settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

typedef struct { char* key; char* name; uint32_t type, size; uint8_t* data; } Val;
static char** keys;
static int nkeys;
static Val* vals;
static int nvals;
static int loaded;
static char file[1024];

static const char* reg_file(void)
{
    /* settings.h: paths.registry / TM2_REGISTRY, else registry.txt in the per user data folder */
    if (!file[0]) snprintf(file, sizeof file, "%s", settings_registry_file());
    return file;
}

static int key_index(const char* k)
{
    for (int i = 0; i < nkeys; i++) if (!strcasecmp(keys[i], k)) return i;
    return -1;
}
static void key_add(const char* k)
{
    if (key_index(k) >= 0) return;
    keys = realloc(keys, sizeof(char*) * (size_t)(nkeys + 1));
    keys[nkeys++] = strdup(k);
}
static Val* val_find(const char* k, const char* name)
{
    for (int i = 0; i < nvals; i++) if (!strcasecmp(vals[i].key, k) && !strcasecmp(vals[i].name, name)) return &vals[i];
    return NULL;
}

static void seed_defaults(void);

static void load(void)
{
    if (loaded) return;
    loaded = 1;
    FILE* f = fopen(reg_file(), "r");
    if (!f) { seed_defaults(); return; }
    char line[4096];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        char* fld[5] = {0};
        int nf = 0;
        for (char* p = line; nf < 5;) {
            fld[nf++] = p;
            char* t = strchr(p, '\t');
            if (!t) break;
            *t = 0;
            p = t + 1;
        }
        if (nf >= 2 && !strcmp(fld[0], "K")) key_add(fld[1]);
        else if (nf == 5 && !strcmp(fld[0], "V")) {
            key_add(fld[1]);
            vals = realloc(vals, sizeof(Val) * (size_t)(nvals + 1));
            Val* v = &vals[nvals++];
            v->key = strdup(fld[1]);
            v->name = strdup(fld[2]);
            v->type = (uint32_t)strtoul(fld[3], NULL, 10);
            v->size = (uint32_t)(strlen(fld[4]) / 2);
            v->data = malloc(v->size + 1);
            for (uint32_t i = 0; i < v->size; i++) { unsigned b; sscanf(fld[4] + 2 * i, "%2x", &b); v->data[i] = (uint8_t)b; }
        }
    }
    fclose(f);
    seed_defaults();
}

/* First run (port choice, not the original): on Windows the installer TM2SETUP.EXE writes the
 * input device switches before the game first runs; without them the game leaves every input
 * device off (input_settings_load 0x004a97f4 returns early and its caller ignores that), so the
 * title screen takes no keys. Seed the installer's state when a value is missing: keyboard on
 * (1), joystick and mouse off (2), the device flags 1. The key tables are not seeded: the game
 * restores its own defaults when they are missing. */
#ifdef ROADKILL_XBOX
/* The Xbox has no keyboard for the network setup screen's names: when missing, a driver name,
 * an alias and the connection are filled in (REG_SZ with its NUL, as the game reads them). A name
 * changed in the game stays. */
static void seed_string(const char* k, const char* name, const char* text)
{
    if (val_find(k, name)) return;
    vals = realloc(vals, sizeof(Val) * (size_t)(nvals + 1));
    Val* v = &vals[nvals++];
    v->key = strdup(k);
    v->name = strdup(name);
    v->type = 1;   /* REG_SZ */
    v->size = (uint32_t)strlen(text) + 1;
    v->data = malloc(v->size + 1);
    memcpy(v->data, text, v->size);
}
#endif

static void seed_defaults(void)
{
    static const char* k = "HKLM\\Software\\Sony Interactive\\Twisted Metal 2\\1.0.1";
    static const struct { const char* name; uint32_t v; } d[] = {
        {"InputDeviceStatus", 1}, {"InputKeyboardStatus", 1}, {"InputJoystickStatus", 2}, {"InputMouseStatus", 2},
    };
    const char* parts[] = {"HKLM", "HKLM\\Software", "HKLM\\Software\\Sony Interactive",
                           "HKLM\\Software\\Sony Interactive\\Twisted Metal 2", k};
    for (unsigned i = 0; i < sizeof parts / sizeof parts[0]; i++) key_add(parts[i]);
    for (unsigned i = 0; i < sizeof d / sizeof d[0]; i++) {
        if (val_find(k, d[i].name)) continue;
        vals = realloc(vals, sizeof(Val) * (size_t)(nvals + 1));
        Val* v = &vals[nvals++];
        v->key = strdup(k);
        v->name = strdup(d[i].name);
        v->type = 4;   /* REG_DWORD */
        v->size = 4;
        v->data = malloc(5);
        for (int b = 0; b < 4; b++) v->data[b] = (uint8_t)(d[i].v >> (8 * b));
    }
#ifdef ROADKILL_XBOX
    seed_string(k, "NetPlayerName", "Xbox Player");
    seed_string(k, "NetPlayerCallsign", "Xbox");
    seed_string(k, "NetProviderName", "Internet TCP/IP Connection For DirectPlay");
#endif
}

static void save(void)
{
    FILE* f = fopen(reg_file(), "w");
    if (!f) return;
    for (int i = 0; i < nkeys; i++) fprintf(f, "K\t%s\n", keys[i]);
    for (int i = 0; i < nvals; i++) {
        fprintf(f, "V\t%s\t%s\t%u\t", vals[i].key, vals[i].name, vals[i].type);
        for (uint32_t k = 0; k < vals[i].size; k++) fprintf(f, "%02x", vals[i].data[k]);
        fprintf(f, "\n");
    }
    fclose(f);
}

/* the path of an open key handle or a predefined root */
static const char* key_path(uint32_t h)
{
    switch (h) {
    case 0x80000000u: return "HKCR";
    case 0x80000001u: return "HKCU";
    case 0x80000002u: return "HKLM";
    case 0x80000003u: return "HKU";
    case 0x80000005u: return "HKCC";
    }
    return w32_handle_get(h, H_KEY);
}
static void join(char* out, size_t cap, uint32_t h, uint32_t sub)
{
    const char* base = key_path(h);
    const char* s = w32_str(sub);
    snprintf(out, cap, "%s%s%s", base ? base : "?", s && *s ? "\\" : "", s ? s : "");
    size_t l = strlen(out);
    while (l && out[l - 1] == '\\') out[--l] = 0;
}

static uint32_t open_key(uint32_t h, uint32_t sub, uint32_t out, int create, uint32_t disp)
{
    load();
    if (!key_path(h)) return 6;   /* ERROR_INVALID_HANDLE */
    char p[1024];
    join(p, sizeof p, h, sub);
    int existed = key_index(p) >= 0;
    if (!existed) {
        if (!create) return 2;   /* ERROR_FILE_NOT_FOUND */
        /* the parents too */
        for (char* q = strchr(p, '\\'); q; q = strchr(q + 1, '\\')) { *q = 0; key_add(p); *q = '\\'; }
        key_add(p);
        save();
    }
    if (out) rt_w32(out, w32_handle_new(H_KEY, strdup(p)));
    if (disp) rt_w32(disp, existed ? 2u : 1u);   /* REG_OPENED_EXISTING_KEY / REG_CREATED_NEW_KEY */
    return 0;
}

/* RegOpenKeyA(key, sub, &result) */
static uint32_t r_RegOpenKeyA(const uint32_t* a, int n) { return open_key(W(0), W(1), W(2), 0, 0); }
/* RegOpenKeyExA(key, sub, options, sam, &result) */
static uint32_t r_RegOpenKeyExA(const uint32_t* a, int n) { return open_key(W(0), W(1), W(4), 0, 0); }
/* RegCreateKeyExA(key, sub, reserved, class, options, sam, sec, &result, &disposition) */
static uint32_t r_RegCreateKeyExA(const uint32_t* a, int n) { return open_key(W(0), W(1), W(7), 1, W(8)); }
static uint32_t r_RegCloseKey(const uint32_t* a, int n)
{
    char* p = w32_handle_get(W(0), H_KEY);
    if (p) { free(p); w32_handle_free(W(0)); }
    return 0;
}
/* RegQueryValueExA(key, name, reserved, &type, data, &size) */
static uint32_t r_RegQueryValueExA(const uint32_t* a, int n)
{
    load();
    const char* k = key_path(W(0));
    if (!k) return 6;
    Val* v = val_find(k, W(1) ? w32_str(W(1)) : "");
    if (!v) return 2;
    if (W(3)) rt_w32(W(3), v->type);
    uint32_t cap = W(5) ? rt_r32(W(5)) : 0;
    if (W(5)) rt_w32(W(5), v->size);
    if (W(4)) {
        if (cap < v->size) return 0xea;   /* ERROR_MORE_DATA */
        memcpy(rt_ptr(W(4)), v->data, v->size);
        /* QoL (host/qol/qol.h, [input] controller): with a controller connected the saved
         * joystick switch reads as on (1) so the game uses the pad without a visit to its input
         * screen; the file keeps what the game wrote */
        if (v->size == 4 && !strcasecmp(v->name, "InputJoystickStatus") && qol_controller_default())
            rt_w32(W(4), 1);
    }
    return 0;
}
/* RegSetValueExA(key, name, reserved, type, data, size) */
static uint32_t r_RegSetValueExA(const uint32_t* a, int n)
{
    load();
    const char* k = key_path(W(0));
    if (!k) return 6;
    const char* name = W(1) ? w32_str(W(1)) : "";
    Val* v = val_find(k, name);
    if (!v) {
        vals = realloc(vals, sizeof(Val) * (size_t)(nvals + 1));
        v = &vals[nvals++];
        v->key = strdup(k);
        v->name = strdup(name);
        v->data = NULL;
    }
    uint32_t size = W(5);
    /* REG_SZ / REG_EXPAND_SZ are kept with their terminating NUL whatever cbData says: the game
     * passes lstrlenA (en2_reg_write_string 0x004ac638, no NUL) and reads the value back into an
     * unterminated buffer (the network screen's NetProviderName, NetPlayerName, NetSessionName),
     * so it relied on reading the NUL back; without it nothing it saved was found again. */
    int sz = W(3) == 1 || W(3) == 2;
    if (sz) {
        const char* str = w32_str(W(4));
        uint32_t len = 0;
        while (len < size && str[len]) len++;
        if (size == 0) len = (uint32_t)strlen(str);
        size = len + 1;
    }
    v->type = W(3);
    v->size = size;
    free(v->data);
    v->data = malloc(size + 1);
    memcpy(v->data, rt_ptr(W(4)), size);
    if (sz) v->data[size - 1] = 0;
    save();
    return 0;
}

void rt_bind_win32_reg(void)
{
    rt_bind_import("ADVAPI32.dll!RegCloseKey", r_RegCloseKey);
    rt_bind_import("ADVAPI32.dll!RegCreateKeyExA", r_RegCreateKeyExA);
    rt_bind_import("ADVAPI32.dll!RegOpenKeyA", r_RegOpenKeyA);
    rt_bind_import("ADVAPI32.dll!RegOpenKeyExA", r_RegOpenKeyExA);
    rt_bind_import("ADVAPI32.dll!RegQueryValueExA", r_RegQueryValueExA);
    rt_bind_import("ADVAPI32.dll!RegSetValueExA", r_RegSetValueExA);
}

RT_BIND_AREA(win32_reg)
