#ifndef QUIVER_C_LUA_RUNNER_H
#define QUIVER_C_LUA_RUNNER_H

#include "common.h"
#include "database.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle type
typedef struct quiver_lua_runner quiver_lua_runner_t;

// Create a new LuaRunner for the given database
QUIVER_C_API quiver_error_t quiver_lua_runner_new(quiver_database_t* db, quiver_lua_runner_t** out_runner);

// Destroy a LuaRunner
QUIVER_C_API quiver_error_t quiver_lua_runner_free(quiver_lua_runner_t* runner);

// Run a Lua script
// Returns QUIVER_OK on success, or an error code on failure.
// If an error occurs, call quiver_lua_runner_get_error() to get the error message.
QUIVER_C_API quiver_error_t quiver_lua_runner_run(quiver_lua_runner_t* runner, const char* script);

// Get the last error message (or NULL if no error).
// The returned pointer is valid until the next call to quiver_lua_runner_run().
// Callers should copy the string if they need to retain it beyond that.
QUIVER_C_API quiver_error_t quiver_lua_runner_get_error(quiver_lua_runner_t* runner, const char** out_error);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_LUA_RUNNER_H
