#include "json.h"
#include <stdlib.h>

struct json_value { int _placeholder; };

json_value_t* json_parse(const char *src) {
    (void)src; /* placeholder */
    json_value_t *v = (json_value_t*)malloc(sizeof(json_value_t));
    v->_placeholder = 1;
    return v;
}

void json_free(json_value_t *v) {
    free(v);
}

int json_deep_merge(json_value_t *dst, const json_value_t *src) {
    (void)dst; (void)src; /* placeholder */
    return MARU_OK;
}

const char *json_get_string(const json_value_t *v, const char *key, const char *defval) {
    (void)v; (void)key; return defval;
}

int json_get_int(const json_value_t *v, const char *key, int defval) {
    (void)v; (void)key; return defval;
}