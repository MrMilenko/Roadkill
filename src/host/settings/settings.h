/*
 * host/settings/settings.h: Roadkill's settings (port code, not from the original game).
 *
 * The settings file is roadkill.ini in the per user data folder (settings_dir):
 *   macOS    ~/Library/Application Support/Roadkill/
 *   Linux    $XDG_DATA_HOME/roadkill/ (else ~/.local/share/roadkill/)
 *   Windows  %APPDATA%\Roadkill\
 * It is written with every known key, its default and a comment the first time Roadkill runs
 * (TM2_SETTINGS=<file> uses another file). The game's own saved settings (the registry the
 * original kept, win32_reg.c) live in the same folder.
 *
 * A key is "section.name", e.g. "display.scale". Every key has an environment variable that
 * overrides the file (the TM2_* variables Roadkill always had, settings.c lists them; a key not
 * in that list answers to TM2_<NAME IN CAPITALS>). Order: environment, file, the caller's default.
 * An empty value counts as not set.
 *
 * Booleans read 1 / 0, on / off, yes / no, true / false. Every call is cheap after the first
 * (which loads the file); any thread may call.
 */
#ifndef TM2_HOST_SETTINGS_H
#define TM2_HOST_SETTINGS_H

int settings_int(const char* key, int def);
int settings_bool(const char* key, int def);
/* the value's text, or def when not set; the pointer stays valid for the run */
const char* settings_str(const char* key, const char* def);

/* the per user data folder (created on first use), without a trailing separator */
const char* settings_dir(void);
/* the settings file in use */
const char* settings_file(void);
/* the game's registry file (win32_reg.c): paths.registry, else registry.txt in settings_dir,
 * copied there once from ~/.config/opentm2/registry.txt (where Roadkill kept it before) */
const char* settings_registry_file(void);

#endif /* TM2_HOST_SETTINGS_H */
