#include "patch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

/* ---- Byte patterns, ported verbatim from GamePatcher.cs (Convert.FromHexString) ---- */

static const unsigned char CoopOriginal[] = {
    0x83,0xE8,0x02,0x74,0x2A,0x83,0xE8,0x01,0x74,0x1E,0x83,0xE8,0x01,0x74,0x12,
    0x32,0xC0,0x8B,0x4D,0xF4,0x64,0x89,0x0D,0x00,0x00,0x00,0x00
};
static const unsigned char CoopPatched[] = {
    0x83,0xE8,0x02,0x90,0x90,0x83,0xE8,0x01,0x90,0x90,0x83,0xE8,0x01,0x90,0x90,
    0x32,0xC0,0x8B,0x4D,0xF4,0x64,0x89,0x0D,0x00,0x00,0x00,0x00
};

static const unsigned char AnalyticsOriginal[] = {
    0x55,0x8B,0xEC,0x83,0xEC,0x10,0x53,0x56,0x57,0xFF,0x15
};
static const unsigned char AnalyticsPatched[] = {
    0xC3,0x8B,0xEC,0x83,0xEC,0x10,0x53,0x56,0x57,0xFF,0x15
};

static const unsigned char CoopCharactersToggleOriginal[] = {
    0x8A,0x4F,0x09,0x84,0xC9,0x51,0x0F,0x94,0xC0,0x33,0xD2,0x84,0xC9,0x88,0x47,
    0x09,0x0F,0xB6,0x47,0x08,0x0F,0x44,0xD3,0x03,0xD0,0x8D,0x04,0x52,0x8D,0x0C,
    0xC5,0x40,0x41,0xB2,0x00,0xE8,0x1F,0x9B,0x05,0x00
};
static const unsigned char CoopCharactersTogglePatched[] = {
    0x80,0x77,0x09,0x01,0x0F,0xB6,0x47,0x08,0x3C,0x11,0x76,0x04,0xB0,0x01,0xEB,
    0x18,0x0F,0xB6,0x57,0x09,0x6B,0xD2,0x12,0x03,0xD0,0x6B,0xCA,0x18,0x51,0x81,
    0xC1,0x40,0x41,0xB2,0x00,0xE8,0x1F,0x9B,0x05,0x00
};

static const unsigned char CoopCharactersCycleOriginal[] = {
    0x8A,0x7F,0x08,0x33,0xF6,0x80,0x7F,0x09,0x00,0xB9,0x12,0x00,0x00,0x00,0x8A,
    0xDF,0x0F,0x45,0xF1,0x0F,0x1F,0x00,0x02,0xD8,0x88,0x5F,0x08,0x80,0xFB,0xFF,
    0x75,0x08,0xC6,0x47,0x08,0x11,0xB3,0x11,0xEB,0x0B,0x80,0xFB,0x12,0x72,0x06,
    0xC6,0x47,0x08,0x00,0x32,0xDB,0x0F,0xB6,0xC3,0x03,0xC6,0x51,0x8D,0x04,0x40,
    0x8D,0x0C,0xC5,0x40,0x41,0xB2,0x00,0xE8,0x7E,0x9A,0x05,0x00,0x84,0xC0,0x75,
    0x07,0x8B,0x45,0x08,0x3A,0xDF,0x75,0xC3,0x81,0x7D,0xF8,0xE9,0x03,0x00,0x00,
    0x74,0x52
};
static const unsigned char CoopCharactersCyclePatched[] = {
    0x8A,0x7F,0x08,0x8A,0xDF,0xBE,0x88,0x98,0xC7,0x00,0x8B,0x0E,0x2B,0x4E,0xFC,
    0xC1,0xF9,0x03,0x83,0xC1,0x12,0x8A,0x45,0x08,0x02,0xD8,0x3A,0xD9,0x72,0x09,
    0xC0,0xF8,0x07,0x8A,0xD8,0x22,0xD9,0x02,0xD8,0x88,0x5F,0x08,0x80,0xFB,0x12,
    0x73,0x20,0x0F,0xB6,0x57,0x09,0x6B,0xD2,0x12,0x0F,0xB6,0xCB,0x03,0xCA,0x6B,
    0xC9,0x18,0x8D,0x8C,0x0E,0xB8,0xA8,0xEA,0xFF,0x51,0xE8,0x7B,0x9A,0x05,0x00,
    0x84,0xC0,0x74,0xBB,0x80,0x7D,0xF8,0xE9,0x74,0x59,0x90,0x90,0x90,0x90,0x90,
    0x90,0x90
};

static const unsigned char CoopCharactersRelocationOriginal[] = {
    0xB3,0x31,0x49,0x32,0x73,0x32
};
static const unsigned char CoopCharactersRelocationPatched[] = {
    0xB3,0x31,0x10,0x32,0x73,0x32
};

static const unsigned char CoopCharactersAvailabilityOriginal[] = {
    0x0F,0xB6,0xC1,0x51,0x8D,0x04,0xC0,0x8D,0x04,0x42,0x8D,0x04,0x40,0x8D,0x0C,
    0xC5,0x40,0x41,0xB2,0x00,0xE8,0x47,0xD1,0xFF,0xFF,0xC3,0xCC,0xCC,0xCC,0xCC,
    0xCC,0xCC
};
static const unsigned char CoopCharactersAvailabilityPatched[] = {
    0x83,0xFA,0x12,0x72,0x03,0xB0,0x01,0xC3,0x0F,0xB6,0xC1,0x6B,0xC0,0x12,0x03,
    0xC2,0x6B,0xC8,0x18,0x51,0x81,0xC1,0x40,0x41,0xB2,0x00,0xE8,0x41,0xD1,0xFF,
    0xFF,0xC3
};

static const unsigned char CoopCharactersAvailabilityRelocationOriginal[] = {
    0x07,0x3B,0x1E,0x3B,0x32,0x3B,0x80,0x3B,0x9C,0x3B,0xB4,0x3B,0xCD,0x3B
};
static const unsigned char CoopCharactersAvailabilityRelocationPatched[] = {
    0x07,0x3B,0x1E,0x3B,0x32,0x3B,0x86,0x3B,0x9C,0x3B,0xB4,0x3B,0xCD,0x3B
};

#define REGION(name) { name##Original, name##Patched, sizeof(name##Original) }

typedef struct {
    const unsigned char *original;
    const unsigned char *patched;
    size_t length;
} region_t;

/* ---- Byte buffer helpers ---- */

typedef struct {
    unsigned char *data;
    size_t length;
} buffer_t;

static bool read_file(const char *path, buffer_t *out, char *err_msg, size_t err_msg_len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(err_msg, err_msg_len, "Cannot open '%s': %s", path, strerror(errno));
        return false;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        snprintf(err_msg, err_msg_len, "Cannot seek '%s': %s", path, strerror(errno));
        fclose(f);
        return false;
    }
    long size = ftell(f);
    if (size < 0) {
        snprintf(err_msg, err_msg_len, "Cannot determine size of '%s'", path);
        fclose(f);
        return false;
    }
    rewind(f);

    unsigned char *data = malloc((size_t) size);
    if (!data) {
        snprintf(err_msg, err_msg_len, "Out of memory reading '%s'", path);
        fclose(f);
        return false;
    }
    if (size > 0 && fread(data, 1, (size_t) size, f) != (size_t) size) {
        snprintf(err_msg, err_msg_len, "Failed to read '%s'", path);
        free(data);
        fclose(f);
        return false;
    }
    fclose(f);

    out->data = data;
    out->length = (size_t) size;
    return true;
}

/* Atomically writes bytes to path via a temp file in the same directory + rename. */
static bool write_file_atomic(const char *path, const unsigned char *data, size_t length,
                               char *err_msg, size_t err_msg_len) {
    char tmp_path[4096];
    snprintf(tmp_path, sizeof(tmp_path), "%s.isaac-online-tmp.%d", path, (int) getpid());

    FILE *f = fopen(tmp_path, "wb");
    if (!f) {
        snprintf(err_msg, err_msg_len, "Cannot create temp file '%s': %s", tmp_path, strerror(errno));
        return false;
    }
    if (length > 0 && fwrite(data, 1, length, f) != length) {
        snprintf(err_msg, err_msg_len, "Failed to write temp file '%s'", tmp_path);
        fclose(f);
        unlink(tmp_path);
        return false;
    }
    fclose(f);

    if (rename(tmp_path, path) != 0) {
        snprintf(err_msg, err_msg_len, "Failed to replace '%s': %s", path, strerror(errno));
        unlink(tmp_path);
        return false;
    }
    return true;
}

/* Finds the first occurrence of pattern in body, or -1 if absent. */
static long find_pattern(const unsigned char *body, size_t body_len,
                          const unsigned char *pattern, size_t pattern_len) {
    if (pattern_len == 0 || body_len < pattern_len)
        return -1;
    for (size_t i = 0; i <= body_len - pattern_len; i++) {
        if (memcmp(body + i, pattern, pattern_len) == 0)
            return (long) i;
    }
    return -1;
}

/* Counts non-overlapping occurrences of pattern in body, stopping early once 2 are found
 * (mirrors the C# CountPattern, which only cares whether the count is 0, 1, or >1). */
static int count_pattern(const unsigned char *body, size_t body_len,
                          const unsigned char *pattern, size_t pattern_len) {
    int count = 0;
    if (pattern_len == 0 || body_len < pattern_len)
        return 0;
    for (size_t i = 0; i <= body_len - pattern_len; i++) {
        if (memcmp(body + i, pattern, pattern_len) != 0)
            continue;
        count++;
        if (count > 1)
            return count;
        i += pattern_len - 1;
    }
    return count;
}

static patch_status_t region_status(const buffer_t *buf, const region_t *region) {
    int original_count = count_pattern(buf->data, buf->length, region->original, region->length);
    int patched_count = count_pattern(buf->data, buf->length, region->patched, region->length);
    if (original_count == 0 && patched_count == 1)
        return PATCH_PATCHED;
    if (original_count == 1 && patched_count == 0)
        return PATCH_NOT_PATCHED;
    return PATCH_UNSUPPORTED;
}

/* Applies one patch region in-place. Returns 1 if modified, 0 if already patched,
 * -1 on error (err_msg filled). */
static int apply_region(buffer_t *buf, const region_t *region, char *err_msg, size_t err_msg_len) {
    patch_status_t status = region_status(buf, region);
    if (status == PATCH_NOT_PATCHED) {
        long index = find_pattern(buf->data, buf->length, region->original, region->length);
        memcpy(buf->data + index, region->patched, region->length);
        return 1;
    }
    if (status == PATCH_PATCHED)
        return 0;
    snprintf(err_msg, err_msg_len,
             "Patch pattern is missing or ambiguous. The game version is not supported.");
    return -1;
}

static patch_status_t coop_characters_status_of(const buffer_t *buf) {
    region_t toggle = REGION(CoopCharactersToggle);
    region_t cycle = REGION(CoopCharactersCycle);
    region_t relocation = REGION(CoopCharactersRelocation);
    region_t availability = REGION(CoopCharactersAvailability);
    region_t availability_relocation = REGION(CoopCharactersAvailabilityRelocation);

    patch_status_t s_toggle = region_status(buf, &toggle);
    patch_status_t s_cycle = region_status(buf, &cycle);
    patch_status_t s_relocation = region_status(buf, &relocation);
    patch_status_t s_availability = region_status(buf, &availability);
    patch_status_t s_availability_relocation = region_status(buf, &availability_relocation);

    if (s_toggle == PATCH_PATCHED && s_cycle == PATCH_PATCHED && s_relocation == PATCH_PATCHED
        && s_availability == PATCH_PATCHED && s_availability_relocation == PATCH_PATCHED)
        return PATCH_PATCHED;
    if (s_toggle == PATCH_NOT_PATCHED && s_cycle == PATCH_NOT_PATCHED && s_relocation == PATCH_NOT_PATCHED
        && s_availability == PATCH_NOT_PATCHED && s_availability_relocation == PATCH_NOT_PATCHED)
        return PATCH_NOT_PATCHED;
    return PATCH_UNSUPPORTED;
}

/* ---- Public API ---- */

patch_status_t patch_get_coop_status(const char *game_path) {
    buffer_t buf;
    char err[256];
    if (!read_file(game_path, &buf, err, sizeof(err)))
        return PATCH_UNSUPPORTED;
    region_t coop = REGION(Coop);
    patch_status_t status = region_status(&buf, &coop);
    free(buf.data);
    return status;
}

patch_status_t patch_get_coop_characters_status(const char *game_path) {
    buffer_t buf;
    char err[256];
    if (!read_file(game_path, &buf, err, sizeof(err)))
        return PATCH_UNSUPPORTED;
    patch_status_t status = coop_characters_status_of(&buf);
    free(buf.data);
    return status;
}

bool patch_apply_coop(const char *game_path, char *err_msg, size_t err_msg_len) {
    buffer_t buf;
    if (!read_file(game_path, &buf, err_msg, err_msg_len))
        return false;

    region_t coop = REGION(Coop);
    region_t analytics = REGION(Analytics);

    int r1 = apply_region(&buf, &coop, err_msg, err_msg_len);
    if (r1 < 0) {
        free(buf.data);
        return false;
    }
    int r2 = apply_region(&buf, &analytics, err_msg, err_msg_len);
    if (r2 < 0) {
        free(buf.data);
        return false;
    }

    bool modified = (r1 == 1) || (r2 == 1);
    if (modified) {
        if (!write_file_atomic(game_path, buf.data, buf.length, err_msg, err_msg_len)) {
            free(buf.data);
            return false;
        }
    }
    free(buf.data);
    return modified;
}

bool patch_apply_coop_characters(const char *game_path, char *err_msg, size_t err_msg_len) {
    buffer_t buf;
    if (!read_file(game_path, &buf, err_msg, err_msg_len))
        return false;

    region_t coop = REGION(Coop);
    if (region_status(&buf, &coop) != PATCH_PATCHED) {
        snprintf(err_msg, err_msg_len, "The co-op mods patch must be applied first.");
        free(buf.data);
        return false;
    }

    patch_status_t status = coop_characters_status_of(&buf);
    if (status == PATCH_UNSUPPORTED) {
        snprintf(err_msg, err_msg_len, "The co-op character patch does not support this game version.");
        free(buf.data);
        return false;
    }
    if (status == PATCH_PATCHED) {
        free(buf.data);
        return false;
    }

    region_t toggle = REGION(CoopCharactersToggle);
    region_t cycle = REGION(CoopCharactersCycle);
    region_t relocation = REGION(CoopCharactersRelocation);
    region_t availability = REGION(CoopCharactersAvailability);
    region_t availability_relocation = REGION(CoopCharactersAvailabilityRelocation);

    region_t *regions[] = { &toggle, &cycle, &relocation, &availability, &availability_relocation };
    for (size_t i = 0; i < sizeof(regions) / sizeof(regions[0]); i++) {
        if (apply_region(&buf, regions[i], err_msg, err_msg_len) < 0) {
            free(buf.data);
            return false;
        }
    }

    if (coop_characters_status_of(&buf) != PATCH_PATCHED) {
        snprintf(err_msg, err_msg_len, "The co-op character patch could not be verified.");
        free(buf.data);
        return false;
    }

    bool ok = write_file_atomic(game_path, buf.data, buf.length, err_msg, err_msg_len);
    free(buf.data);
    return ok;
}
