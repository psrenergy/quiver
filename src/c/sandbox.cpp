#include "quiver/c/sandbox.h"

#include "internal.h"
#include "quiver/sandbox.h"
#include "utils/string.h"

#include <new>

struct quiver_sandbox {
    quiver::Sandbox runner;

    explicit quiver_sandbox(quiver::Database& db) : runner(db) {}
};

extern "C" {

QUIVER_C_API quiver_error_t quiver_sandbox_new(quiver_database_t* db, quiver_sandbox_t** out_runner) {
    QUIVER_REQUIRE(db, out_runner);

    try {
        *out_runner = new quiver_sandbox(db->db);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_sandbox_free(quiver_sandbox_t* runner) {
    delete runner;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_sandbox_run(quiver_sandbox_t* runner, const char* script, char** out_result) {
    QUIVER_REQUIRE(runner, script, out_result);
    *out_result = nullptr;  // so a caller that frees unconditionally never sees a stale pointer

    try {
        *out_result = quiver::string::new_c_str(runner->runner.run(script));
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_sandbox_free_string(char* str) {
    delete[] str;
    return QUIVER_OK;
}

}  // extern "C"
