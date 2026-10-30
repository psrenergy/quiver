#ifndef QUIVER_C_SANDBOX_H
#define QUIVER_C_SANDBOX_H

#include "common.h"
#include "database.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle type
typedef struct quiver_sandbox quiver_sandbox_t;

// Create a new Sandbox for the given database
QUIVER_C_API quiver_error_t quiver_sandbox_new(quiver_database_t* db, quiver_sandbox_t** out_sandbox);

// Destroy a Sandbox
QUIVER_C_API quiver_error_t quiver_sandbox_free(quiver_sandbox_t* sandbox);

// Run a Lua script
// out_result receives the script's return value encoded as JSON, or an empty string if the script
// returned nothing; it is set to NULL on failure. The caller frees it with
// quiver_database_free_string.
// To execute a script without keeping its writes, wrap this call in
// quiver_database_begin_dry_run / quiver_database_end_dry_run.
// timeout_ms > 0 limits how long the script runs, in wall-clock milliseconds; 0 means no limit, and
// a negative value is an error. The limit is soft: it is checked between Lua instructions and SQLite
// VM steps, so a single long call the script makes finishes first. A script past its limit fails
// with "Cannot run: script exceeded its time limit of <N> ms".
// Returns QUIVER_OK on success, or an error code on failure.
// On failure the error message is available via quiver_get_last_error().
QUIVER_C_API quiver_error_t quiver_sandbox_run(
    quiver_sandbox_t* sandbox,
    const char* script,
    int64_t timeout_ms,
    char** out_result
);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_SANDBOX_H
