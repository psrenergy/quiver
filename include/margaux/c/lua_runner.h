#ifndef DECK_DATABASE_C_LUA_RUNNER_H
#define DECK_DATABASE_C_LUA_RUNNER_H

#include "common.h"
#include "database.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle type
typedef struct lua_runner lua_runner_t;

// Create a new LuaRunner for the given database
DECK_DATABASE_C_API lua_runner_t* lua_runner_new(database_t* db);

// Destroy a LuaRunner
DECK_DATABASE_C_API void lua_runner_free(lua_runner_t* runner);

// Run a Lua script
// Returns DECK_DATABASE_OK on success, or an error code on failure.
// If an error occurs, call lua_runner_get_error() to get the error message.
DECK_DATABASE_C_API margaux_error_t lua_runner_run(lua_runner_t* runner, const char* script);

// Get the last error message (or NULL if no error).
// The returned pointer is valid until the next call to lua_runner_run().
// Callers should copy the string if they need to retain it beyond that.
DECK_DATABASE_C_API const char* lua_runner_get_error(lua_runner_t* runner);

#ifdef __cplusplus
}
#endif

#endif  // DECK_DATABASE_C_LUA_RUNNER_H
