#ifndef MARGAUX_EXPORT_H
#define MARGAUX_EXPORT_H

#ifdef MARGAUX_DATABASE_STATIC
#define MARGAUX_API
#else
#ifdef _WIN32
#ifdef MARGAUX_DATABASE_EXPORTS
#define MARGAUX_API __declspec(dllexport)
#else
#define MARGAUX_API __declspec(dllimport)
#endif
#else
#define MARGAUX_API __attribute__((visibility("default")))
#endif
#endif

#endif  // MARGAUX_EXPORT_H
