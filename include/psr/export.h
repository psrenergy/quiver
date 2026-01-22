#ifndef QUIVER_EXPORT_H
#define QUIVER_EXPORT_H

#ifdef QUIVER_DATABASE_STATIC
#define QUIVER_API
#else
#ifdef _WIN32
#ifdef QUIVER_DATABASE_EXPORTS
#define QUIVER_API __declspec(dllexport)
#else
#define QUIVER_API __declspec(dllimport)
#endif
#else
#define QUIVER_API __attribute__((visibility("default")))
#endif
#endif

#endif  // QUIVER_EXPORT_H
