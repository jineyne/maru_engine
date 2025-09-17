#ifndef MARU_JSON_H
#define MARU_JSON_H

#include "error.h"


typedef struct json_value json_value_t;

json_value_t *json_parse(const char *src);
json_value_t *json_parse_file(const char *path);
int json_deep_merge(json_value_t *dst, const json_value_t *src);

void json_free(json_value_t *v);

const char *json_get_string(const json_value_t *v, const char *dotted_key, const char *defval);
int json_get_int(const json_value_t *v, const char *dotted_key, int defval);

#endif /* MARU_JSON_H */
