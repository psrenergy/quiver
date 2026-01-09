#ifndef PSR_C_PLATFORM_H
#define PSR_C_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

// Platform-specific export macros
#ifdef _WIN32
#ifdef PSR_DATABASE_C_EXPORTS
#define PSR_C_API __declspec(dllexport)
#else
#define PSR_C_API __declspec(dllimport)
#endif
#else
#define PSR_C_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
}
#endif

#endif  // PSR_C_PLATFORM_H
