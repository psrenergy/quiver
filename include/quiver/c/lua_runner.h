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
// On failure the error message is available via quiver_get_last_error().
QUIVER_C_API quiver_error_t quiver_lua_runner_run(quiver_lua_runner_t* runner, const char* script);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_LUA_RUNNER_H
