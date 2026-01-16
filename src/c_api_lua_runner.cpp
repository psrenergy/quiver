#include "psr/c/lua_runner.h"
#include "psr/database.h"
#include "psr/lua_runner.h"

#include <new>
#include <string>

// psr_database struct (must match definition in c_api_database.cpp)
struct psr_database {
    psr::Database db;
    psr_database(const std::string& path, const psr::DatabaseOptions& options) : db(path, options) {}
    psr_database(psr::Database&& database) : db(std::move(database)) {}
};

struct psr_lua_runner {
    psr::LuaRunner runner;
    std::string last_error;

    explicit psr_lua_runner(psr::Database& db) : runner(db) {}
};

psr_lua_runner_t* psr_lua_runner_new(psr_database_t* db) {
    if (!db) {
        return nullptr;
    }
    try {
        return new psr_lua_runner(db->db);
    } catch (...) {
        return nullptr;
    }
}

void psr_lua_runner_free(psr_lua_runner_t* runner) {
    delete runner;
}

psr_error_t psr_lua_runner_run(psr_lua_runner_t* runner, const char* script) {
    if (!runner || !script) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    try {
        runner->last_error.clear();
        runner->runner.run(script);
        return PSR_OK;
    } catch (const std::exception& e) {
        runner->last_error = e.what();
        return PSR_ERROR_DATABASE;
    } catch (...) {
        runner->last_error = "Unknown error";
        return PSR_ERROR_DATABASE;
    }
}

const char* psr_lua_runner_get_error(psr_lua_runner_t* runner) {
    if (!runner || runner->last_error.empty()) {
        return nullptr;
    }
    return runner->last_error.c_str();
}
