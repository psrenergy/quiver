#include "quiver/c/sandbox.h"

#include "internal.h"
#include "quiver/sandbox.h"
#include "utils/string.h"

#include <new>

struct quiver_sandbox {
    quiver::Sandbox sandbox;

    explicit quiver_sandbox(quiver::Database& db) : sandbox(db) {}
};

extern "C" {

QUIVER_C_API quiver_error_t quiver_sandbox_new(quiver_database_t* db, quiver_sandbox_t** out_sandbox) {
    QUIVER_REQUIRE(db, out_sandbox);

    try {
        *out_sandbox = new quiver_sandbox(db->db);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_sandbox_free(quiver_sandbox_t* sandbox) {
    delete sandbox;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_sandbox_run(quiver_sandbox_t* sandbox, const char* script, char** out_result) {
    QUIVER_REQUIRE(sandbox, script, out_result);
    *out_result = nullptr;  // so a caller that frees unconditionally never sees a stale pointer

    try {
        *out_result = quiver::string::new_c_str(sandbox->sandbox.run(script));
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
