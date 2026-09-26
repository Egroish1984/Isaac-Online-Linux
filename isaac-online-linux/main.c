#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#include "patch.h"
#include "eid_patch.h"

#define MAX_PATH_LEN 4096

/* ---------------------------------------------------------------------
 * Steam game-path autodetection (Linux).
 * Looks in the default Steam locations and, if a libraryfolders.vdf is
 * found, also checks every extra Steam library listed there.
 * --------------------------------------------------------------------- */

static bool path_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

/* Very small VDF scanner: pulls out every quoted string that follows a
 * "path" key, e.g.  "path"        "/mnt/games/SteamLibrary" */
static void scan_library_folders(const char *vdf_path, char roots[][MAX_PATH_LEN], int *count, int max_roots) {
    FILE *f = fopen(vdf_path, "r");
    if (!f)
        return;

    char line[MAX_PATH_LEN];
    while (fgets(line, sizeof(line), f) && *count < max_roots) {
        char *key = strstr(line, "\"path\"");
        if (!key)
            continue;
        char *value_start = strchr(key + 6, '"');
        if (!value_start)
            continue;
        value_start++;
        char *value_end = strchr(value_start, '"');
        if (!value_end)
            continue;
        size_t len = (size_t) (value_end - value_start);
        if (len == 0 || len >= MAX_PATH_LEN)
            continue;
        memcpy(roots[*count], value_start, len);
        roots[*count][len] = '\0';
        (*count)++;
    }
    fclose(f);
}

static bool try_game_dir(const char *steam_root, char *out_path, size_t out_len) {
    static const char *candidates[] = { "isaac-ng.exe", "isaac-ng" };
    char game_dir[MAX_PATH_LEN];
    snprintf(game_dir, sizeof(game_dir), "%s/steamapps/common/The Binding of Isaac Rebirth", steam_root);
    if (!path_exists(game_dir))
        return false;

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        char candidate[MAX_PATH_LEN];
        snprintf(candidate, sizeof(candidate), "%s/%s", game_dir, candidates[i]);
        if (path_exists(candidate)) {
            snprintf(out_path, out_len, "%s", candidate);
            return true;
        }
    }
    return false;
}

static bool detect_game_path(char *out_path, size_t out_len) {
    const char *home = getenv("HOME");
    if (!home)
        home = "";

    char default_roots[4][MAX_PATH_LEN];
    int default_count = 0;
    snprintf(default_roots[default_count++], MAX_PATH_LEN, "%s/.steam/steam", home);
    snprintf(default_roots[default_count++], MAX_PATH_LEN, "%s/.steam/root", home);
    snprintf(default_roots[default_count++], MAX_PATH_LEN, "%s/.local/share/Steam", home);
    snprintf(default_roots[default_count++], MAX_PATH_LEN,
             "%s/.var/app/com.valvesoftware.Steam/.local/share/Steam", home);

    for (int i = 0; i < default_count; i++) {
        if (try_game_dir(default_roots[i], out_path, out_len))
            return true;
    }

    /* Look for extra Steam libraries listed in libraryfolders.vdf. */
    char extra_roots[64][MAX_PATH_LEN];
    int extra_count = 0;
    for (int i = 0; i < default_count && extra_count < 64; i++) {
        char vdf_path[MAX_PATH_LEN];
        snprintf(vdf_path, sizeof(vdf_path), "%s/steamapps/libraryfolders.vdf", default_roots[i]);
        scan_library_folders(vdf_path, extra_roots, &extra_count, 64);
    }
    for (int i = 0; i < extra_count; i++) {
        if (try_game_dir(extra_roots[i], out_path, out_len))
            return true;
    }

    return false;
}

/* ---------------------------------------------------------------------
 * EID mod-folder detection: <game_dir>/mods/<name matches "external",
 * "item", "descriptions">/features/eid_api.lua
 * --------------------------------------------------------------------- */

static void to_lower_copy(const char *src, char *dst, size_t dst_len) {
    size_t i = 0;
    for (; src[i] && i + 1 < dst_len; i++)
        dst[i] = (char) tolower((unsigned char) src[i]);
    dst[i] = '\0';
}

static bool find_eid_path(const char *game_path, char *out_path, size_t out_len) {
    char game_dir[MAX_PATH_LEN];
    snprintf(game_dir, sizeof(game_dir), "%s", game_path);
    char *slash = strrchr(game_dir, '/');
    if (!slash)
        return false;
    *slash = '\0';

    char mods_path[MAX_PATH_LEN];
    snprintf(mods_path, sizeof(mods_path), "%s/mods", game_dir);

    DIR *dir = opendir(mods_path);
    if (!dir)
        return false;

    struct dirent *entry;
    bool found = false;
    while (!found && (entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        char lower_name[512];
        to_lower_copy(entry->d_name, lower_name, sizeof(lower_name));
        if (!strstr(lower_name, "external") || !strstr(lower_name, "item")
            || !strstr(lower_name, "descriptions"))
            continue;

        char candidate[MAX_PATH_LEN];
        snprintf(candidate, sizeof(candidate), "%s/%s", mods_path, entry->d_name);
        char lua_path[MAX_PATH_LEN];
        snprintf(lua_path, sizeof(lua_path), "%s/features/eid_api.lua", candidate);
        if (path_exists(lua_path)) {
            snprintf(out_path, out_len, "%s", candidate);
            found = true;
        }
    }
    closedir(dir);
    return found;
}

/* ---------------------------------------------------------------------
 * UI helpers
 * --------------------------------------------------------------------- */

static const char *status_text(patch_status_t status) {
    switch (status) {
        case PATCH_PATCHED: return "Yes";
        case PATCH_NOT_PATCHED: return "No";
        case PATCH_PARTIALLY_PATCHED: return "Partial";
        default: return "Unsupported / unavailable";
    }
}

static void print_diagnostics(const char *game_path) {
    if (!path_exists(game_path)) {
        printf("  Game executable:      not found at this path\n");
        return;
    }
    patch_status_t coop_status = patch_get_coop_status(game_path);
    patch_status_t characters_status = patch_get_coop_characters_status(game_path);
    printf("  Co-op mods patched:      %s\n", status_text(coop_status));
    printf("  Co-op characters patched: %s\n", status_text(characters_status));

    char eid_path[MAX_PATH_LEN];
    if (find_eid_path(game_path, eid_path, sizeof(eid_path))) {
        patch_status_t eid_status = eid_get_status(eid_path);
        printf("  EID patched:             %s\n", status_text(eid_status));
    } else {
        printf("  EID patched:             not installed\n");
    }
}

static void read_line(char *buf, size_t len) {
    if (fgets(buf, (int) len, stdin) == NULL) {
        buf[0] = '\0';
        return;
    }
    size_t n = strlen(buf);
    if (n > 0 && buf[n - 1] == '\n')
        buf[n - 1] = '\0';
}

int main(int argc, char **argv) {
    char game_path[MAX_PATH_LEN] = { 0 };

    if (argc > 1) {
        snprintf(game_path, sizeof(game_path), "%s", argv[1]);
    } else if (!detect_game_path(game_path, sizeof(game_path))) {
        game_path[0] = '\0';
    }

    printf("Isaac Online Modded (Linux CLI)\n");
    printf("================================\n\n");

    if (game_path[0] && path_exists(game_path)) {
        printf("Detected game executable: %s\n", game_path);
    } else {
        printf("Game executable not detected automatically.\n");
        printf("Enter the full path to isaac-ng.exe (or isaac-ng): ");
        fflush(stdout);
        read_line(game_path, sizeof(game_path));
    }

    while (true) {
        printf("\n--- Status for: %s ---\n", game_path);
        print_diagnostics(game_path);

        printf("\nOptions:\n");
        printf("  1) Patch co-op mods (+ analytics-crash fix)\n");
        printf("  2) Patch co-op characters (requires option 1)\n");
        printf("  3) Patch External Item Descriptions (EID) for co-op\n");
        printf("  4) Change game path\n");
        printf("  5) Refresh status\n");
        printf("  0) Quit\n");
        printf("> ");
        fflush(stdout);

        char choice[16];
        read_line(choice, sizeof(choice));
        char err[256];

        if (strcmp(choice, "1") == 0) {
            if (!path_exists(game_path)) {
                printf("Invalid game path.\n");
                continue;
            }
            bool modified = patch_apply_coop(game_path, err, sizeof(err));
            printf("%s\n", modified ? "Game patched successfully." : "Game is already patched (or see error above).");
        } else if (strcmp(choice, "2") == 0) {
            if (!path_exists(game_path)) {
                printf("Invalid game path.\n");
                continue;
            }
            bool modified = patch_apply_coop_characters(game_path, err, sizeof(err));
            if (!modified && err[0])
                printf("Error: %s\n", err);
            else
                printf("%s\n", modified ? "Coop characters patched successfully."
                                         : "Coop characters are already patched.");
        } else if (strcmp(choice, "3") == 0) {
            if (!path_exists(game_path)) {
                printf("Invalid game path.\n");
                continue;
            }
            char eid_path[MAX_PATH_LEN];
            if (!find_eid_path(game_path, eid_path, sizeof(eid_path))) {
                printf("External Item Descriptions was not found in the game's mods directory.\n");
                continue;
            }
            bool modified = eid_apply_patch(eid_path, err, sizeof(err));
            if (!modified && err[0])
                printf("Error: %s\n", err);
            else
                printf("%s\n", modified ? "EID patched successfully." : "EID is already patched.");
        } else if (strcmp(choice, "4") == 0) {
            printf("Enter the full path to isaac-ng.exe (or isaac-ng): ");
            fflush(stdout);
            read_line(game_path, sizeof(game_path));
        } else if (strcmp(choice, "5") == 0) {
            continue;
        } else if (strcmp(choice, "0") == 0) {
            break;
        } else {
            printf("Unknown option.\n");
        }
    }

    return 0;
}
