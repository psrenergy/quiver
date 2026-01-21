#ifndef DECK_DATABASE_EXPORT_H
#define DECK_DATABASE_EXPORT_H

#ifdef DECK_DATABASE_DATABASE_STATIC
#define DECK_DATABASE_API
#else
#ifdef _WIN32
#ifdef DECK_DATABASE_DATABASE_EXPORTS
#define DECK_DATABASE_API __declspec(dllexport)
#else
#define DECK_DATABASE_API __declspec(dllimport)
#endif
#else
#define DECK_DATABASE_API __attribute__((visibility("default")))
#endif
#endif

#endif  // DECK_DATABASE_EXPORT_H
