#include "json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "fs.h"
#include "misc/cjson.h"
#include "macro.h"

struct json_value {
    cJSON *root;
};

static const cJSON *find_by_dotted(const cJSON *root, const char *dotted) {
    if (!root || !dotted) return NULL;

    char *copy = strdup(dotted);
    if (!copy) return NULL;

    const cJSON *cur = root;
    char *tok = strtok(copy, ".");
    while (tok) {
        cur = cJSON_GetObjectItemCaseSensitive((cJSON*) cur, tok);
        if (!cur) {
            free(copy);
            return NULL;
        }
        tok = strtok(NULL, ".");
    }

    free(copy);
    return cur;
}

static cJSON *dup_item(const cJSON *item) {
    return cJSON_Duplicate((cJSON*) item, 1 /* recurse */);
}

static void merge_object(cJSON *dst, const cJSON *src) {
    if (!cJSON_IsObject(dst) || !cJSON_IsObject((cJSON*) src)) return;

    for (const cJSON *s = src->child; s; s = s->next) {
        const char *name = s->string;
        if (!name) continue;

        cJSON *d = cJSON_GetObjectItemCaseSensitive(dst, name);
        if (d && cJSON_IsObject(d) && cJSON_IsObject((cJSON*) s)) {
            merge_object(d, s);
        } else {
            if (d) {
                cJSON_DeleteItemFromObjectCaseSensitive(dst, name);
            }
            cJSON *copy = dup_item(s);
            cJSON_AddItemToObject(dst, name, copy);
        }
    }
}

static void path_dirname(const char *path, char *out, size_t out_sz) {
    size_t len = path ? strlen(path) : 0;
    size_t cut = 0;
    for (size_t i = 0; i < len; ++i) {
        if (path[i] == '/' || path[i] == '\\') cut = i + 1;
    }
    if (cut > 0 && cut < out_sz) {
        memcpy(out, path, cut);
        out[cut] = '\0';
    } else {
        out[0] = '\0';
    }
}

static void path_join(char *out, size_t out_sz, const char *dir, const char *name) {
    if (!dir || !*dir) {
        snprintf(out, out_sz, "%s", name ? name : "");
    } else {
#ifdef _WIN32
        const char sep = '\\';
#else
        const char sep = '/';
#endif
        size_t dl = strlen(dir);
        if (dl > 0 && (dir[dl - 1] == '/' || dir[dl - 1] == '\\')) {
            snprintf(out, out_sz, "%s%s", dir, name ? name : "");
        } else {
            snprintf(out, out_sz, "%s%c%s", dir, sep, name ? name : "");
        }
    }
}


json_value_t *json_parse(const char *src) {
    if (!src) return NULL;
    cJSON *r = cJSON_Parse(src);
    if (!r) return NULL;

    json_value_t *v = (json_value_t*) malloc(sizeof(json_value_t));
    if (!v) {
        cJSON_Delete(r);
        return NULL;
    }
    v->root = r;
    return v;
}

static int json_merge_includes_for_file(cJSON *dst_root, const char *base_file_path) {
    if (!dst_root || !base_file_path) return MARU_OK;

    cJSON *includes = cJSON_GetObjectItemCaseSensitive(dst_root, "includes");
    if (!includes || !cJSON_IsArray(includes)) return MARU_OK;

    char basedir[512];
    path_dirname(base_file_path, basedir, sizeof(basedir));

    cJSON *it = NULL;
    cJSON_ArrayForEach(it, includes) {
        if (!cJSON_IsString(it) || !it->valuestring) continue;

        char incpath[1024];
        path_join(incpath, sizeof(incpath), basedir, it->valuestring);

        char *buf = NULL;
        size_t sz = 0;
        if (fs_read_all(incpath, &buf, &sz) != MARU_OK) continue;

        cJSON *inc_root = cJSON_ParseWithLength(buf, sz);
        free(buf);
        if (!inc_root) continue;

        json_merge_includes_for_file(inc_root, incpath);

        merge_object(dst_root, inc_root);
        cJSON_Delete(inc_root);
    }
    return MARU_OK;
}

json_value_t *json_parse_file(const char *path) {
    if (!path) return NULL;

    char *buf = NULL;
    size_t sz = 0;
    if (fs_read_all(path, &buf, &sz) != MARU_OK) return NULL;

    cJSON *r = cJSON_ParseWithLength(buf, sz);
    free(buf);
    if (!r) return NULL;

    /* includes º´ÇÕ */
    json_merge_includes_for_file(r, path);

    json_value_t *v = (json_value_t*) malloc(sizeof(json_value_t));
    if (!v) {
        cJSON_Delete(r);
        return NULL;
    }
    v->root = r;
    return v;
}

int json_deep_merge(json_value_t *dst, const json_value_t *src) {
    if (!dst || !dst->root || !src || !src->root) return MARU_ERR_INVALID;
    if (!cJSON_IsObject(dst->root) || !cJSON_IsObject(src->root)) return MARU_ERR_TYPE;
    merge_object(dst->root, src->root);
    return MARU_OK;
}

void json_free(json_value_t *v) {
    if (!v) return;
    if (v->root) cJSON_Delete(v->root);
    free(v);
}

const char *json_get_string(const json_value_t *v, const char *dotted_key, const char *defval) {
    if (!v || !v->root || !dotted_key) return defval;
    cJSON *node = find_by_dotted(v->root, dotted_key);
    if (cJSON_IsString(node) && node->valuestring) return node->valuestring;
    return defval;
}

int json_get_int(const json_value_t *v, const char *dotted_key, int defval) {
    if (!v || !v->root || !dotted_key) return defval;
    cJSON *node = find_by_dotted(v->root, dotted_key);
    if (cJSON_IsNumber(node)) return node->valueint;
    if (cJSON_IsString(node) && node->valuestring) {
        return atoi(node->valuestring);
    }

    return defval;
}
