#include "quiver/c/lua_runner.h"

#include "internal.h"
#include "quiver/lua_runner.h"

#include <new>

struct quiver_lua_runner {
    quiver::LuaRunner runner;

    explicit quiver_lua_runner(quiver::Database& db) : runner(db) {}
};

extern "C" {

QUIVER_C_API quiver_error_t quiver_lua_runner_new(quiver_database_t* db, quiver_lua_runner_t** out_runner) {
    QUIVER_REQUIRE(db, out_runner);

    try {
        *out_runner = new quiver_lua_runner(db->db);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    } catch (...) {
        quiver_set_last_error("Unknown error creating LuaRunner");
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_lua_runner_free(quiver_lua_runner_t* runner) {
    delete runner;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_lua_runner_run(quiver_lua_runner_t* runner, const char* script) {
    QUIVER_REQUIRE(runner, script);

    try {
        runner->runner.run(script);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    } catch (...) {
        quiver_set_last_error("Unknown error running Lua script");
        return QUIVER_ERROR;
    }
}

}  // extern "C"
