#ifndef PSR_C_LUA_RUNNER_H
#define PSR_C_LUA_RUNNER_H

#include "common.h"
#include "database.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle type
typedef struct psr_lua_runner psr_lua_runner_t;

// Create a new LuaRunner for the given database
PSR_C_API psr_lua_runner_t* psr_lua_runner_new(psr_database_t* db);

// Destroy a LuaRunner
PSR_C_API void psr_lua_runner_free(psr_lua_runner_t* runner);

// Run a Lua script
// Returns PSR_OK on success, or an error code on failure.
// If an error occurs, call psr_lua_runner_get_error() to get the error message.
PSR_C_API psr_error_t psr_lua_runner_run(psr_lua_runner_t* runner, const char* script);

// Get the last error message (or NULL if no error).
// The returned pointer is valid until the next call to psr_lua_runner_run().
// Callers should copy the string if they need to retain it beyond that.
PSR_C_API const char* psr_lua_runner_get_error(psr_lua_runner_t* runner);

#ifdef __cplusplus
}
#endif

#endif  // PSR_C_LUA_RUNNER_H
