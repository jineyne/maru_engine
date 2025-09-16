/* maru_export.h */

#ifndef MARU_EXPORT_H
#define MARU_EXPORT_H

#if defined(_WIN32)
#define PLUGIN_API __declspec(dllexport)
#else
#define PLUGIN_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
    /* ... */
#ifdef __cplusplus
}
#endif

#endif /* MARU_EXPORT_H */