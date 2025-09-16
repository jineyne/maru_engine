#ifndef MARU_MACRO_H
#define MARU_MACRO_H

#ifdef __cplusplus
extern "C" {
#endif

#define UNUSED(x) (void)(x)

#if defined(__GNUC__) || defined(__clang__)
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x)   (x)
#define UNLIKELY(x) (x)
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define ALIGN_UP(x, align)   (((x) + ((align) - 1)) & ~((align) - 1))
#define ALIGN_DOWN(x, align) ((x) & ~((align) - 1))

#define SAFE_FREE(p) do { if (p) { free(p); (p) = NULL; } } while (0)

#define SAFE_RELEASE(p, release_fn) do { if (p) { release_fn(p); (p) = NULL; } } while (0)

#ifndef MARU_INLINE
#define MARU_INLINE static inline
#endif

#if defined(__GNUC__)
#define MARU_UNUSED_FUNC __attribute__((unused))
#else
#define MARU_UNUSED_FUNC
#endif

#define RETURN_IF_FAIL(cond, errcode) do { if (!(cond)) return (errcode); } while (0)
#define GOTO_IF_FAIL(cond, label) do { if (!(cond)) goto label; } while (0)

#ifdef __cplusplus
}
#endif

#endif /* MARU_MACRO_H */