#ifndef MARGAUX_TEST_UTILS_H
#define MARGAUX_TEST_UTILS_H

#include <filesystem>
#include <psr/c/database.h>
#include <string>

namespace psr::test {

// Get path relative to the test file's directory
// Usage: path_from(__FILE__, "schemas/valid/basic.sql")
inline std::string path_from(const char* test_file, const std::string& relative) {
    namespace fs = std::filesystem;
    return (fs::path(test_file).parent_path() / relative).string();
}

// Default options with logging off for tests
inline margaux_options_t quiet_options() {
    auto options = margaux_options_default();
    options.console_level = MARGAUX_LOG_OFF;
    return options;
}

}  // namespace margaux::test

// Convenience macros for common schema locations
// Usage: SCHEMA_PATH("schemas/valid/basic.sql")
#define SCHEMA_PATH(relative) psr::test::path_from(__FILE__, relative)
#define VALID_SCHEMA(name) SCHEMA_PATH("schemas/valid/" name)
#define INVALID_SCHEMA(name) SCHEMA_PATH("schemas/invalid/" name)

#endif  // MARGAUX_TEST_UTILS_H
