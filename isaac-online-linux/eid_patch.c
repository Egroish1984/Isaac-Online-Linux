#define _POSIX_C_SOURCE 200809L
#include "eid_patch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#define GUARD_LEN 5

static const char *ANCHOR =
    "return listUpdatedForPlayers -- dont evaluate when bad data is present";
static const char *GUARD[GUARD_LEN] = {
    "-- IsaacOnlineModded: avoid invalid player access after the run ends",
    "local stage = Game():GetLevel():GetStage()",
    "if stage == nil or stage >= 13 or stage < 1 then",
    "return listUpdatedForPlayers",
    "end",
};

typedef struct {
    char **lines;
    size_t count;
    size_t capacity;
    const char *newline;  /* "\n" or "\r\n" */
    bool has_bom;
} lua_document_t;

static void doc_init(lua_document_t *doc) {
    doc->lines = NULL;
    doc->count = 0;
    doc->capacity = 0;
    doc->newline = "\n";
    doc->has_bom = false;
}

static void doc_push(lua_document_t *doc, char *line /* takes ownership */) {
    if (doc->count == doc->capacity) {
        doc->capacity = doc->capacity ? doc->capacity * 2 : 64;
        doc->lines = realloc(doc->lines, doc->capacity * sizeof(char *));
    }
    doc->lines[doc->count++] = line;
}

static void doc_insert_at(lua_document_t *doc, size_t index, char *line) {
    if (doc->count == doc->capacity) {
        doc->capacity = doc->capacity ? doc->capacity * 2 : 64;
        doc->lines = realloc(doc->lines, doc->capacity * sizeof(char *));
    }
    memmove(&doc->lines[index + 1], &doc->lines[index], (doc->count - index) * sizeof(char *));
    doc->lines[index] = line;
    doc->count++;
}

static void doc_free(lua_document_t *doc) {
    for (size_t i = 0; i < doc->count; i++)
        free(doc->lines[i]);
    free(doc->lines);
}

static char *dup_trim(const char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r')
        s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r'))
        len--;
    char *out = malloc(len + 1);
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

static bool read_document(const char *path, lua_document_t *doc, char *err_msg, size_t err_msg_len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(err_msg, err_msg_len, "Cannot open '%s': %s", path, strerror(errno));
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    if (size < 0) {
        snprintf(err_msg, err_msg_len, "Cannot determine size of '%s'", path);
        fclose(f);
        return false;
    }
    char *raw = malloc((size_t) size + 1);
    if (size > 0 && fread(raw, 1, (size_t) size, f) != (size_t) size) {
        snprintf(err_msg, err_msg_len, "Failed to read '%s'", path);
        free(raw);
        fclose(f);
        return false;
    }
    fclose(f);
    raw[size] = '\0';

    doc_init(doc);
    size_t start = 0;
    if (size >= 3 && (unsigned char) raw[0] == 0xEF && (unsigned char) raw[1] == 0xBB
        && (unsigned char) raw[2] == 0xBF) {
        doc->has_bom = true;
        start = 3;
    }
    doc->newline = strstr(raw + start, "\r\n") ? "\r\n" : "\n";

    /* Split into lines on \n, then strip a trailing \r from each (normalizes \r\n and \n). */
    char *cursor = raw + start;
    while (true) {
        char *nl = strchr(cursor, '\n');
        size_t len = nl ? (size_t) (nl - cursor) : strlen(cursor);
        if (len > 0 && cursor[len - 1] == '\r')
            len--;
        char *line = malloc(len + 1);
        memcpy(line, cursor, len);
        line[len] = '\0';
        doc_push(doc, line);
        if (!nl)
            break;
        cursor = nl + 1;
    }

    free(raw);
    return true;
}

static bool write_document(const char *path, const lua_document_t *doc, char *err_msg, size_t err_msg_len) {
    char tmp_path[4096];
    snprintf(tmp_path, sizeof(tmp_path), "%s.isaac-online-tmp.%d", path, (int) getpid());

    FILE *f = fopen(tmp_path, "wb");
    if (!f) {
        snprintf(err_msg, err_msg_len, "Cannot create temp file '%s': %s", tmp_path, strerror(errno));
        return false;
    }
    if (doc->has_bom) {
        unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
        fwrite(bom, 1, 3, f);
    }
    for (size_t i = 0; i < doc->count; i++) {
        fputs(doc->lines[i], f);
        if (i + 1 < doc->count)
            fputs(doc->newline, f);
    }
    fclose(f);

    if (rename(tmp_path, path) != 0) {
        snprintf(err_msg, err_msg_len, "Failed to replace '%s': %s", path, strerror(errno));
        unlink(tmp_path);
        return false;
    }
    return true;
}

static char *get_api_path(const char *eid_path) {
    size_t len = strlen(eid_path);
    char *out = malloc(len + strlen("/features/eid_api.lua") + 1);
    sprintf(out, "%s/features/eid_api.lua", eid_path);
    return out;
}

static long find_unique_anchor(const lua_document_t *doc) {
    long index = -1;
    for (size_t i = 0; i < doc->count; i++) {
        if (!strstr(doc->lines[i], ANCHOR))
            continue;
        if (index >= 0)
            return -1;
        index = (long) i;
    }
    return index;
}

static bool matches_block(const lua_document_t *doc, size_t index, const char *const *block, size_t block_len) {
    if (index + block_len > doc->count)
        return false;
    for (size_t i = 0; i < block_len; i++) {
        char *t = dup_trim(doc->lines[index + i]);
        bool eq = strcmp(t, block[i]) == 0;
        free(t);
        if (!eq)
            return false;
    }
    return true;
}

static bool matches_return_and_end(const lua_document_t *doc, size_t index) {
    if (index + 1 >= doc->count)
        return false;
    char *t0 = dup_trim(doc->lines[index]);
    char *t1 = dup_trim(doc->lines[index + 1]);
    bool ok = (strncmp(t0, "return listUpdatedForPlayers", strlen("return listUpdatedForPlayers")) == 0)
              && strcmp(t1, "end") == 0;
    free(t0);
    free(t1);
    return ok;
}

static bool matches_legacy_stage_guard(const lua_document_t *doc, size_t index) {
    if (index + 7 > doc->count)
        return false;
    char *l1 = dup_trim(doc->lines[index + 1]);
    char *l2 = dup_trim(doc->lines[index + 2]);
    char *l3 = dup_trim(doc->lines[index + 3]);
    bool ok = strcmp(l1, "if stage == nil then") == 0
              && strncmp(l2, "return listUpdatedForPlayers", strlen("return listUpdatedForPlayers")) == 0
              && strcmp(l3, "end") == 0
              && strstr(doc->lines[index + 4], "stage >= 13")
              && strstr(doc->lines[index + 4], "stage < 1")
              && matches_return_and_end(doc, index + 5);
    free(l1);
    free(l2);
    free(l3);
    return ok;
}

static bool has_stage_guard(const lua_document_t *doc, long anchor_index) {
    size_t end = doc->count < (size_t) anchor_index + 14 ? doc->count : (size_t) anchor_index + 14;
    for (size_t i = (size_t) anchor_index + 1; i < end; i++) {
        if (matches_block(doc, i, GUARD, GUARD_LEN))
            return true;

        char *line = dup_trim(doc->lines[i]);
        bool legacy = strcmp(line, "local stage = Game():GetLevel():GetStage()") == 0
                      && matches_legacy_stage_guard(doc, i);
        free(line);
        if (legacy)
            return true;

        if (strstr(doc->lines[i], "Game():GetLevel():GetStage() >= LevelStage.Home")
            && matches_return_and_end(doc, i + 1))
            return true;
    }
    return false;
}

patch_status_t eid_get_status(const char *eid_path) {
    char *api_path = get_api_path(eid_path);
    lua_document_t doc;
    char err[256];
    if (!read_document(api_path, &doc, err, sizeof(err))) {
        free(api_path);
        return PATCH_UNSUPPORTED;
    }
    free(api_path);

    long anchor = find_unique_anchor(&doc);
    patch_status_t status;
    if (anchor < 0)
        status = PATCH_UNSUPPORTED;
    else
        status = has_stage_guard(&doc, anchor) ? PATCH_PATCHED : PATCH_NOT_PATCHED;

    doc_free(&doc);
    return status;
}

bool eid_apply_patch(const char *eid_path, char *err_msg, size_t err_msg_len) {
    char *api_path = get_api_path(eid_path);
    lua_document_t doc;
    if (!read_document(api_path, &doc, err_msg, err_msg_len)) {
        free(api_path);
        return false;
    }

    long anchor = find_unique_anchor(&doc);
    if (anchor < 0) {
        snprintf(err_msg, err_msg_len, "The installed EID version is missing a unique patch location.");
        doc_free(&doc);
        free(api_path);
        return false;
    }
    if (has_stage_guard(&doc, anchor)) {
        doc_free(&doc);
        free(api_path);
        return false;
    }

    long block_end = -1;
    size_t search_end = doc.count < (size_t) anchor + 4 ? doc.count : (size_t) anchor + 4;
    for (size_t i = (size_t) anchor + 1; i < search_end; i++) {
        char *t = dup_trim(doc.lines[i]);
        bool is_end = strcmp(t, "end") == 0;
        free(t);
        if (is_end) {
            block_end = (long) i;
            break;
        }
    }
    if (block_end < 0) {
        snprintf(err_msg, err_msg_len, "Could not locate the EID player validation block.");
        doc_free(&doc);
        free(api_path);
        return false;
    }

    /* Insert, in order, after block_end: "", "\t\t"+G0, "\t\t"+G1, "\t\t"+G2, "\t\t\t"+G3, "\t\t"+G4 */
    char buf[512];
    size_t at = (size_t) block_end + 1;

    doc_insert_at(&doc, at++, strdup(""));
    snprintf(buf, sizeof(buf), "\t\t%s", GUARD[0]);
    doc_insert_at(&doc, at++, strdup(buf));
    snprintf(buf, sizeof(buf), "\t\t%s", GUARD[1]);
    doc_insert_at(&doc, at++, strdup(buf));
    snprintf(buf, sizeof(buf), "\t\t%s", GUARD[2]);
    doc_insert_at(&doc, at++, strdup(buf));
    snprintf(buf, sizeof(buf), "\t\t\t%s", GUARD[3]);
    doc_insert_at(&doc, at++, strdup(buf));
    snprintf(buf, sizeof(buf), "\t\t%s", GUARD[4]);
    doc_insert_at(&doc, at++, strdup(buf));

    bool ok = write_document(api_path, &doc, err_msg, err_msg_len);
    doc_free(&doc);
    free(api_path);
    return ok;
}
