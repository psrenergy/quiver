#include "psr/database.h"

#include "psr/migrations.h"
#include "psr/result.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include <stdexcept>

namespace {

std::atomic<uint64_t> g_logger_counter{0};

spdlog::level::level_enum to_spdlog_level(psr::LogLevel level) {
    switch (level) {
    case psr::LogLevel::debug:
        return spdlog::level::debug;
    case psr::LogLevel::info:
        return spdlog::level::info;
    case psr::LogLevel::warn:
        return spdlog::level::warn;
    case psr::LogLevel::error:
        return spdlog::level::err;
    case psr::LogLevel::off:
        return spdlog::level::off;
    default:
        return spdlog::level::info;
    }
}

std::shared_ptr<spdlog::logger> create_database_logger(const std::string& db_path, psr::LogLevel console_level) {
    namespace fs = std::filesystem;

    // Generate unique logger name for multiple Database instances
    auto id = g_logger_counter.fetch_add(1);
    auto logger_name = "psr_database_" + std::to_string(id);

    // Create console sink (thread-safe)
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(to_spdlog_level(console_level));

    if (db_path == ":memory:") {
        //
        auto logger = std::make_shared<spdlog::logger>(logger_name, console_sink);
        logger->set_level(spdlog::level::debug);
        logger->warn("...");
        return logger;
    } else {
        // File-based database: use database directory
        auto db_dir = fs::path(db_path).parent_path();
        if (db_dir.empty()) {
            // Database file in current directory (no path separator)
            db_dir = fs::current_path();
        }

        // Create file sink (thread-safe)
        std::shared_ptr<spdlog::sinks::basic_file_sink_mt> file_sink = nullptr;
        try {
            auto log_file_path = (db_dir / "psr_database.log").string();
            file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_path, true);
            file_sink->set_level(spdlog::level::debug);

            std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
            auto logger = std::make_shared<spdlog::logger>(logger_name, sinks.begin(), sinks.end());
            logger->set_level(spdlog::level::debug);

            return logger;
        } catch (const spdlog::spdlog_ex& ex) {
            // If file sink creation fails, continue with console-only logging
            auto logger = std::make_shared<spdlog::logger>(logger_name, console_sink);
            logger->set_level(spdlog::level::debug);
            logger->warn("Failed to create file sink: {}. Logging to console only.", ex.what());
            return logger;
        }
    }
}

}  // anonymous namespace

namespace psr {

struct Database::Impl {
    sqlite3* db = nullptr;
    std::string path;
    std::shared_ptr<spdlog::logger> logger;

    ~Impl() {
        if (db) {
            sqlite3_close_v2(db);
            db = nullptr;
        }
    }
};

Database::Database(const std::string& path, const DatabaseOptions& options) : impl_(std::make_unique<Impl>()) {
    impl_->path = path;
    impl_->logger = create_database_logger(path, options.console_level);

    impl_->logger->debug("Opening database: {}", path);

    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    int rc = sqlite3_open_v2(path.c_str(), &impl_->db, flags, nullptr);

    if (rc != SQLITE_OK) {
        std::string error_msg = impl_->db ? sqlite3_errmsg(impl_->db) : "Unknown error";
        impl_->logger->error("Failed to open database: {}", error_msg);
        if (impl_->db) {
            sqlite3_close(impl_->db);
            impl_->db = nullptr;
        }
        throw std::runtime_error("Failed to open database: " + error_msg);
    }

    // Enable foreign keys
    sqlite3_exec(impl_->db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    impl_->logger->debug("Database opened successfully, foreign keys enabled");

    impl_->logger->info("Database opened successfully: {}", path);
}

Database::~Database() {
    if (impl_ && impl_->db) {
        impl_->logger->debug("Closing database: {}", impl_->path);
        sqlite3_close_v2(impl_->db);
        impl_->db = nullptr;
        impl_->logger->info("Database closed");
    }
}

Database::Database(Database&& other) noexcept = default;
Database& Database::operator=(Database&& other) noexcept = default;

bool Database::is_healthy() const {
    return impl_ && impl_->db != nullptr;
}

Result Database::execute(const std::string& sql, const std::vector<Value>& params) {
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(impl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(impl_->db)));
    }

    // Bind parameters
    for (size_t i = 0; i < params.size(); ++i) {
        int idx = static_cast<int>(i + 1);
        const auto& param = params[i];

        std::visit(
            [&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::nullptr_t>) {
                    sqlite3_bind_null(stmt, idx);
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    sqlite3_bind_int64(stmt, idx, arg);
                } else if constexpr (std::is_same_v<T, double>) {
                    sqlite3_bind_double(stmt, idx, arg);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    sqlite3_bind_text(stmt, idx, arg.c_str(), static_cast<int>(arg.size()), SQLITE_TRANSIENT);
                } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                    sqlite3_bind_blob(stmt, idx, arg.data(), static_cast<int>(arg.size()), SQLITE_TRANSIENT);
                }
            },
            param);
    }

    // Get column info
    std::vector<std::string> columns;
    int col_count = sqlite3_column_count(stmt);
    columns.reserve(col_count);
    for (int i = 0; i < col_count; ++i) {
        const char* name = sqlite3_column_name(stmt, i);
        columns.emplace_back(name ? name : "");
    }

    // Execute and fetch results
    std::vector<Row> rows;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        std::vector<Value> values;
        values.reserve(col_count);

        for (int i = 0; i < col_count; ++i) {
            int type = sqlite3_column_type(stmt, i);
            switch (type) {
            case SQLITE_INTEGER:
                values.emplace_back(sqlite3_column_int64(stmt, i));
                break;
            case SQLITE_FLOAT:
                values.emplace_back(sqlite3_column_double(stmt, i));
                break;
            case SQLITE_TEXT: {
                const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                values.emplace_back(std::string(text ? text : ""));
                break;
            }
            case SQLITE_BLOB: {
                const uint8_t* data = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, i));
                int size = sqlite3_column_bytes(stmt, i);
                values.emplace_back(std::vector<uint8_t>(data, data + size));
                break;
            }
            case SQLITE_NULL:
            default:
                values.emplace_back(nullptr);
                break;
            }
        }
        rows.emplace_back(std::move(values));
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(impl_->db)));
    }

    return Result(std::move(columns), std::move(rows));
}

int64_t Database::current_version() const {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "PRAGMA user_version;";
    int rc = sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(impl_->db)));
    }

    rc = sqlite3_step(stmt);
    int64_t user_version = 0;
    if (rc == SQLITE_ROW) {
        user_version = sqlite3_column_int(stmt, 0);
    } else {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to retrieve user_version: " + std::string(sqlite3_errmsg(impl_->db)));
    }

    sqlite3_finalize(stmt);
    return user_version;
}

const std::string& Database::path() const {
    static const std::string empty;
    return impl_ ? impl_->path : empty;
}

Database Database::from_migrations(const std::string& db_path,
                                   const std::string& migrations_path,
                                   const DatabaseOptions& options) {
    Database db(db_path, options);
    db.migrate_up(migrations_path);
    return db;
}

Database Database::from_schema(const std::string& db_path,
                               const std::string& schema_path,
                               const DatabaseOptions& options) {
    Database db(db_path, options);
    db.apply_schema(schema_path);
    return db;
}

void Database::set_version(int64_t version) {
    std::string sql = "PRAGMA user_version = " + std::to_string(version) + ";";
    char* err_msg = nullptr;
    int rc = sqlite3_exec(impl_->db, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error("Failed to set user_version: " + error);
    }
    impl_->logger->debug("Set database version to {}", version);
}

void Database::begin_transaction() {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(impl_->db, "BEGIN TRANSACTION;", nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error("Failed to begin transaction: " + error);
    }
    impl_->logger->debug("Transaction started");
}

void Database::commit() {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(impl_->db, "COMMIT;", nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error("Failed to commit transaction: " + error);
    }
    impl_->logger->debug("Transaction committed");
}

void Database::rollback() {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(impl_->db, "ROLLBACK;", nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        impl_->logger->error("Failed to rollback transaction: {}", error);
        // Don't throw here - rollback is often called in error recovery
    } else {
        impl_->logger->debug("Transaction rolled back");
    }
}

void Database::execute_raw(const std::string& sql) {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(impl_->db, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error("Failed to execute SQL: " + error);
    }
}

void Database::migrate_up(const std::string& migrations_path) {
    Migrations migrations(migrations_path);
    if (migrations.empty()) {
        impl_->logger->debug("No migrations found in {}", migrations_path);
        return;
    }

    int64_t current = current_version();
    auto pending = migrations.pending(current);

    if (pending.empty()) {
        impl_->logger->debug("Database is up to date at version {}", current);
        return;
    }

    impl_->logger->info(
        "Applying {} pending migration(s) from version {} to {}", pending.size(), current, migrations.latest_version());

    for (const auto& migration : pending) {
        impl_->logger->info("Applying migration {}", migration.version());

        std::string up_sql = migration.up_sql();
        if (up_sql.empty()) {
            throw std::runtime_error("Migration " + std::to_string(migration.version()) + " has no up.sql file");
        }

        begin_transaction();
        try {
            execute_raw(up_sql);
            set_version(migration.version());
            commit();
            impl_->logger->info("Migration {} applied successfully", migration.version());
        } catch (const std::exception& e) {
            rollback();
            impl_->logger->error("Migration {} failed: {}", migration.version(), e.what());
            throw std::runtime_error("Migration " + std::to_string(migration.version()) + " failed: " + e.what());
        }
    }

    impl_->logger->info("All migrations applied successfully. Database now at version {}", current_version());
}

void Database::apply_schema(const std::string& schema_path) {
    namespace fs = std::filesystem;

    if (!fs::exists(schema_path)) {
        throw std::runtime_error("Schema file not found: " + schema_path);
    }

    std::ifstream file(schema_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open schema file: " + schema_path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string schema_sql = buffer.str();

    if (schema_sql.empty()) {
        throw std::runtime_error("Schema file is empty: " + schema_path);
    }

    impl_->logger->info("Applying schema from: {}", schema_path);

    begin_transaction();
    try {
        execute_raw(schema_sql);
        commit();
        impl_->logger->info("Schema applied successfully");
    } catch (const std::exception& e) {
        rollback();
        impl_->logger->error("Failed to apply schema: {}", e.what());
        throw std::runtime_error("Failed to apply schema: " + std::string(e.what()));
    }
}

}  // namespace psr
