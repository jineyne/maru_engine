#include "error.h"

const char* maru_error_string(maru_result err) {
    switch (err) {
        case MARU_OK: return "Success";

        case MARU_ERR_INVALID_ARG: return "Invalid argument";
        case MARU_ERR_OUT_OF_MEMORY: return "Out of memory";
        case MARU_ERR_NOT_FOUND: return "Not found";
        case MARU_ERR_ALREADY_EXISTS: return "Already exists";
        case MARU_ERR_RESOURCE_EXHAUSTED: return "Resource exhausted";

        case MARU_ERR_IO_READ: return "I/O read error";
        case MARU_ERR_IO_WRITE: return "I/O write error";
        case MARU_ERR_FILE_NOT_FOUND: return "File not found";

        case MARU_ERR_PARSE_JSON: return "JSON parse error";
        case MARU_ERR_PARSE_OBJ: return "OBJ parse error";
        case MARU_ERR_PARSE_SHADER: return "Shader parse error";

        case MARU_ERR_TYPE_MISMATCH: return "Type mismatch";

        case MARU_ERR_SHADER_COMPILE: return "Shader compilation failed";
        case MARU_ERR_PIPELINE_CREATE: return "Pipeline creation failed";
        case MARU_ERR_DEVICE_LOST: return "Graphics device lost";
        case MARU_ERR_INVALID_HANDLE: return "Invalid handle";

        case MARU_ERR_PLUGIN_LOAD: return "Plugin load failed";
        case MARU_ERR_PLUGIN_SYMBOL: return "Plugin symbol not found";

        default: return "Unknown error";
    }
}