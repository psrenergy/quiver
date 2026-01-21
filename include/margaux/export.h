#ifndef PSR_EXPORT_H
#define PSR_EXPORT_H

#ifdef PSR_DATABASE_STATIC
#define PSR_API
#else
#ifdef _WIN32
#ifdef PSR_DATABASE_EXPORTS
#define PSR_API __declspec(dllexport)
#else
#define PSR_API __declspec(dllimport)
#endif
#else
#define PSR_API __attribute__((visibility("default")))
#endif
#endif

#endif  // PSR_EXPORT_H
