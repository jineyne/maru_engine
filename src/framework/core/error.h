#ifndef MARU_ERROR_H
#define MARU_ERROR_H

typedef enum {
    MARU_OK = 0,

    /* General errors */
    MARU_ERR_INVALID_ARG = -1,
    MARU_ERR_OUT_OF_MEMORY = -2,
    MARU_ERR_NOT_FOUND = -3,
    MARU_ERR_ALREADY_EXISTS = -4,
    MARU_ERR_RESOURCE_EXHAUSTED = -5,

    /* I/O errors */
    MARU_ERR_IO_READ = -10,
    MARU_ERR_IO_WRITE = -11,
    MARU_ERR_FILE_NOT_FOUND = -12,

    /* Parsing errors */
    MARU_ERR_PARSE_JSON = -20,
    MARU_ERR_PARSE_OBJ = -21,
    MARU_ERR_PARSE_SHADER = -22,

    /* Type errors */
    MARU_ERR_TYPE_MISMATCH = -30,

    /* Graphics errors */
    MARU_ERR_SHADER_COMPILE = -40,
    MARU_ERR_PIPELINE_CREATE = -41,
    MARU_ERR_DEVICE_LOST = -42,
    MARU_ERR_INVALID_HANDLE = -43,

    /* Plugin errors */
    MARU_ERR_PLUGIN_LOAD = -50,
    MARU_ERR_PLUGIN_SYMBOL = -51,

    /* Legacy compatibility */
    MARU_ERR_INVALID = MARU_ERR_INVALID_ARG,
    MARU_ERR_IO = MARU_ERR_IO_READ,
    MARU_ERR_PARSE = MARU_ERR_PARSE_JSON,
    MARU_ERR_TYPE = MARU_ERR_TYPE_MISMATCH
} maru_result;

const char* maru_error_string(maru_result err);

#endif /* MARU_ERROR_H */