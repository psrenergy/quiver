#ifndef MARGAUX_C_LUA_RUNNER_H
#define MARGAUX_C_LUA_RUNNER_H

#include "common.h"
#include "database.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle type
typedef struct lua_runner lua_runner_t;

// Create a new LuaRunner for the given database
MARGAUX_C_API lua_runner_t* lua_runner_new(database_t* db);

// Destroy a LuaRunner
MARGAUX_C_API void lua_runner_free(lua_runner_t* runner);

// Run a Lua script
// Returns MARGAUX_OK on success, or an error code on failure.
// If an error occurs, call lua_runner_get_error() to get the error message.
MARGAUX_C_API margaux_error_t lua_runner_run(lua_runner_t* runner, const char* script);

// Get the last error message (or NULL if no error).
// The returned pointer is valid until the next call to lua_runner_run().
// Callers should copy the string if they need to retain it beyond that.
MARGAUX_C_API const char* lua_runner_get_error(lua_runner_t* runner);

#ifdef __cplusplus
}
#endif

#endif  // MARGAUX_C_LUA_RUNNER_H
