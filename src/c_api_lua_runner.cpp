#include "c_api_internal.h"
#include "margaux/c/lua_runner.h"
#include "margaux/lua_runner.h"

#include <new>
#include <string>

struct margaux_lua_runner {
    margaux::LuaRunner runner;
    std::string last_error;

    explicit margaux_lua_runner(margaux::Database& db) : runner(db) {}
};

margaux_lua_runner_t* margaux_lua_runner_new(margaux_t* db) {
    if (!db) {
        return nullptr;
    }
    try {
        return new margaux_lua_runner(db->db);
    } catch (...) {
        return nullptr;
    }
}

void margaux_lua_runner_free(margaux_lua_runner_t* runner) {
    delete runner;
}

margaux_error_t margaux_lua_runner_run(margaux_lua_runner_t* runner, const char* script) {
    if (!runner || !script) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        runner->last_error.clear();
        runner->runner.run(script);
        return MARGAUX_OK;
    } catch (const std::exception& e) {
        runner->last_error = e.what();
        return MARGAUX_ERROR_DATABASE;
    } catch (...) {
        runner->last_error = "Unknown error";
        return MARGAUX_ERROR_DATABASE;
    }
}

const char* margaux_lua_runner_get_error(margaux_lua_runner_t* runner) {
    if (!runner || runner->last_error.empty()) {
        return nullptr;
    }
    return runner->last_error.c_str();
}
