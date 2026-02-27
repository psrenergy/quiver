#include <quiver/database.h>
#include <quiver/lua_runner.h>

#include <argparse/argparse.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

static std::string read_script_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to read script file: " + path);
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // Strip UTF-8 BOM if present
    if (content.size() >= 3 &&
        content[0] == '\xEF' &&
        content[1] == '\xBB' &&
        content[2] == '\xBF') {
        content.erase(0, 3);
    }

    return content;
}

static quiver_log_level_t parse_log_level(const std::string& level) {
    if (level == "debug") return QUIVER_LOG_DEBUG;
    if (level == "info") return QUIVER_LOG_INFO;
    if (level == "warn") return QUIVER_LOG_WARN;
    if (level == "error") return QUIVER_LOG_ERROR;
    if (level == "off") return QUIVER_LOG_OFF;
    throw std::runtime_error("Unknown log level: " + level);
}

int main(int argc, char* argv[]) {
    argparse::ArgumentParser program("quiver_lua", "quiver_lua " QUIVER_VERSION);

    program.add_argument("database")
        .help("path to the database file");

    program.add_argument("script")
        .help("path to the Lua script file")
        .nargs(argparse::nargs_pattern::optional);

    program.add_argument("--schema")
        .help("create database from schema file");

    program.add_argument("--migrations")
        .help("create database from migrations directory");

    program.add_argument("--read-only")
        .help("open database in read-only mode")
        .flag();

    program.add_argument("--log-level")
        .help("set log verbosity (debug, info, warn, error, off)")
        .default_value(std::string("warn"));

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 2;
    }

    // Mutual exclusivity check
    if (program.is_used("--schema") && program.is_used("--migrations")) {
        std::cerr << "Cannot use --schema and --migrations together" << std::endl;
        return 2;
    }

    // Script required (for now) -- prepare for future REPL
    auto script_path = program.present<std::string>("script");
    if (!script_path) {
        std::cerr << "No script provided" << std::endl;
        return 2;
    }

    try {
        // Check script file exists
        if (!std::filesystem::exists(*script_path)) {
            std::cerr << "Script file not found: " << *script_path << std::endl;
            return 1;
        }

        // Configure database options
        quiver::DatabaseOptions options{};
        options.read_only = program.get<bool>("--read-only") ? 1 : 0;
        options.console_level = parse_log_level(program.get<std::string>("--log-level"));

        // Construct database (one of three modes)
        auto db_path = program.get<std::string>("database");

        quiver::Database db = [&]() -> quiver::Database {
            if (program.is_used("--schema")) {
                return quiver::Database::from_schema(db_path, program.get<std::string>("--schema"), options);
            }
            if (program.is_used("--migrations")) {
                return quiver::Database::from_migrations(db_path, program.get<std::string>("--migrations"), options);
            }
            return quiver::Database(db_path, options);
        }();

        // Read and execute script
        auto script = read_script_file(*script_path);
        quiver::LuaRunner lua(db);
        lua.run(script);

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
