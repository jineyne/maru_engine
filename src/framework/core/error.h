#ifndef MARU_ERROR_H
#define MARU_ERROR_H

typedef enum {
    MARU_OK = 0,
    MARU_ERR_INVALID = -1,
    MARU_ERR_IO = -2,
    MARU_ERR_PARSE = -3,
    MARU_ERR_TYPE = -4
} maru_result;

#endif /* MARU_ERROR_H */