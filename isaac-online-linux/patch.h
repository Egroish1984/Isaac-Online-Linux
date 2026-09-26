#ifndef PATCH_H
#define PATCH_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    PATCH_NOT_PATCHED,
    PATCH_PARTIALLY_PATCHED,
    PATCH_PATCHED,
    PATCH_UNSUPPORTED,
} patch_status_t;

/* Status of the base co-op mods patch. */
patch_status_t patch_get_coop_status(const char *game_path);

/* Status of the optional co-op-characters patch (requires the base patch). */
patch_status_t patch_get_coop_characters_status(const char *game_path);

/* Applies the base co-op mods patch and the analytics-crash patch.
 * Returns true if the file was modified, false if it was already patched.
 * On failure, returns false and fills err_msg (caller-supplied buffer). */
bool patch_apply_coop(const char *game_path, char *err_msg, size_t err_msg_len);

/* Applies the optional co-op-characters patch. Requires the base co-op
 * patch to already be applied. */
bool patch_apply_coop_characters(const char *game_path, char *err_msg, size_t err_msg_len);

#endif
