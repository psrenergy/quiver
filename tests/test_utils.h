#ifndef QUIVER_TEST_UTILS_H
#define QUIVER_TEST_UTILS_H

#include <quiver/c/database.h>
#include <sqlite3.h>

#include <filesystem>
#include <stdexcept>
#include <string>

namespace quiver::test {

// Get path relative to the test file's directory
// Usage: path_from(__FILE__, "schemas/valid/basic.sql")
inline std::string path_from(const char* test_file, const std::string& relative) {
    namespace fs = std::filesystem;
    return (fs::path(test_file).parent_path() / relative).string();
}

// Default options with logging off for tests
inline quiver_database_options_t quiet_options() {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    return options;
}

// Runs raw SQL on a file-backed database through a second sqlite3 connection. query_* only accepts read-only SQL,
// so this is how a test plants data the typed API refuses (an invalid cell, an extra column).
inline void exec_sql(const std::string& db_path, const std::string& sql) {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        throw std::runtime_error("exec_sql: cannot open " + db_path);
    }
    char* error = nullptr;
    const auto rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error);
    const std::string message = error ? error : "";
    sqlite3_free(error);
    sqlite3_close(db);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("exec_sql: " + message);
    }
}

}  // namespace quiver::test

// Convenience macros for common schema locations
// Usage: SCHEMA_PATH("schemas/valid/basic.sql")
#define SCHEMA_PATH(relative) quiver::test::path_from(__FILE__, relative)
#define VALID_SCHEMA(name) SCHEMA_PATH("schemas/valid/" name)
#define INVALID_SCHEMA(name) SCHEMA_PATH("schemas/invalid/" name)

#endif  // QUIVER_TEST_UTILS_H
