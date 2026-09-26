#ifndef EID_PATCH_H
#define EID_PATCH_H

#include <stdbool.h>
#include <stddef.h>
#include "patch.h"

/* eid_path is the mod's root directory (the one containing "features/eid_api.lua"). */
patch_status_t eid_get_status(const char *eid_path);

/* Returns true if modified, false if already patched.
 * On failure returns false and fills err_msg. */
bool eid_apply_patch(const char *eid_path, char *err_msg, size_t err_msg_len);

#endif
