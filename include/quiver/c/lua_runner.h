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
// out_result receives the script's return value encoded as JSON, or an empty string if the script
// returned nothing; it is set to NULL on failure. The caller frees it with
// quiver_lua_runner_free_string.
// To execute a script without keeping its writes, wrap this call in
// quiver_database_begin_dry_run / quiver_database_end_dry_run.
// Returns QUIVER_OK on success, or an error code on failure.
// On failure the error message is available via quiver_get_last_error().
QUIVER_C_API quiver_error_t quiver_lua_runner_run(quiver_lua_runner_t* runner, const char* script, char** out_result);

// Free a string returned by quiver_lua_runner_run. NULL-tolerant.
QUIVER_C_API quiver_error_t quiver_lua_runner_free_string(char* str);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_LUA_RUNNER_H
