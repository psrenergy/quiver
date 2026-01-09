#include "psr/database.h"

#include "psr/migrations.h"
#include "psr/result.h"
#include "psr/schema.h"
#include "psr/schema_validator.h"
#include "psr/type_validator.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include <sstream>
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
        // In-memory database: no file logging
        const auto logger = std::make_shared<spdlog::logger>(logger_name, console_sink);
        logger->set_level(spdlog::level::debug);
        logger->warn("Database is in-memory only; no file logging will be performed.");
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
            const auto log_file_path = (db_dir / "psr_database.log").string();
            file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_path, true);
            file_sink->set_level(spdlog::level::debug);

            std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
            auto logger = std::make_shared<spdlog::logger>(logger_name, sinks.begin(), sinks.end());
            logger->set_level(spdlog::level::debug);

            return logger;
        } catch (const spdlog::spdlog_ex& ex) {
            // If file sink creation fails, continue with console-only logging
            const auto logger = std::make_shared<spdlog::logger>(logger_name, console_sink);
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
    std::unique_ptr<Schema> schema;
    std::unique_ptr<TypeValidator> type_validator;

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
    const auto rc = sqlite3_open_v2(path.c_str(), &impl_->db, flags, nullptr);

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
    auto rc = sqlite3_prepare_v2(impl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(impl_->db)));
    }

    // Bind parameters
    for (size_t i = 0; i < params.size(); ++i) {
        const auto idx = static_cast<int>(i + 1);
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
    const auto col_count = sqlite3_column_count(stmt);
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
    auto rc = sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr);
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

const Schema* Database::schema() const {
    return impl_ ? impl_->schema.get() : nullptr;
}

Database Database::from_migrations(const std::string& db_path,
                                   const std::string& migrations_path,
                                   const DatabaseOptions& options) {
    auto db = Database(db_path, options);
    db.migrate_up(migrations_path);
    return db;
}

Database
Database::from_schema(const std::string& db_path, const std::string& schema_path, const DatabaseOptions& options) {
    auto db = Database(db_path, options);
    db.apply_schema(schema_path);
    return db;
}

void Database::set_version(int64_t version) {
    const auto sql = "PRAGMA user_version = " + std::to_string(version) + ";";
    char* err_msg = nullptr;
    const auto rc = sqlite3_exec(impl_->db, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error("Failed to set user_version: " + error);
    }
    impl_->logger->debug("Set database version to {}", version);
}

void Database::begin_transaction() {
    char* err_msg = nullptr;
    const auto rc = sqlite3_exec(impl_->db, "BEGIN TRANSACTION;", nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error("Failed to begin transaction: " + error);
    }
    impl_->logger->debug("Transaction started");
}

void Database::commit() {
    char* err_msg = nullptr;
    const auto rc = sqlite3_exec(impl_->db, "COMMIT;", nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error("Failed to commit transaction: " + error);
    }
    impl_->logger->debug("Transaction committed");
}

void Database::rollback() {
    char* err_msg = nullptr;
    const auto rc = sqlite3_exec(impl_->db, "ROLLBACK;", nullptr, nullptr, &err_msg);
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
    const auto rc = sqlite3_exec(impl_->db, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error("Failed to execute SQL: " + error);
    }
}

void Database::migrate_up(const std::string& migrations_path) {
    const auto migrations = Migrations(migrations_path);
    if (migrations.empty()) {
        impl_->logger->debug("No migrations found in {}", migrations_path);
        return;
    }

    const auto current = current_version();
    const auto pending = migrations.pending(current);

    if (pending.empty()) {
        impl_->logger->debug("Database is up to date at version {}", current);
        return;
    }

    impl_->logger->info(
        "Applying {} pending migration(s) from version {} to {}", pending.size(), current, migrations.latest_version());

    for (const auto& migration : pending) {
        impl_->logger->info("Applying migration {}", migration.version());

        const auto up_sql = migration.up_sql();
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
    const auto schema_sql = buffer.str();

    if (schema_sql.empty()) {
        throw std::runtime_error("Schema file is empty: " + schema_path);
    }

    impl_->logger->info("Applying schema from: {}", schema_path);

    begin_transaction();
    try {
        execute_raw(schema_sql);

        // Load schema metadata
        impl_->schema = std::make_unique<Schema>(Schema::from_database(impl_->db));

        // Validate schema structure
        SchemaValidator validator(*impl_->schema);
        validator.validate();

        // Create type validator
        impl_->type_validator = std::make_unique<TypeValidator>(*impl_->schema);

        commit();
    } catch (const std::exception& e) {
        rollback();
        impl_->logger->error("Failed to apply schema: {}", e.what());
        throw;
    }

    impl_->logger->info("Schema applied successfully");
}

int64_t Database::create_element(const std::string& collection, const Element& element) {
    impl_->logger->debug("Creating element in collection: {}", collection);

    // Require schema to be loaded - fail explicitly
    if (!impl_->schema) {
        throw std::runtime_error("Cannot create element: no schema loaded");
    }
    if (!impl_->schema->has_table(collection)) {
        throw std::runtime_error("Collection not found in schema: " + collection);
    }

    const auto& scalars = element.scalars();
    if (scalars.empty()) {
        throw std::runtime_error("Element must have at least one scalar attribute");
    }

    // Validate scalar types
    for (const auto& [name, value] : scalars) {
        impl_->type_validator->validate_scalar(collection, name, value);
    }

    // Build INSERT SQL: INSERT INTO <collection> (<cols>) VALUES (<placeholders>)
    std::string sql = "INSERT INTO " + collection + " (";
    std::string placeholders;
    std::vector<Value> params;

    auto first = true;
    for (const auto& [name, value] : scalars) {
        if (!first) {
            sql += ", ";
            placeholders += ", ";
        }
        sql += name;
        placeholders += "?";
        params.push_back(value);
        first = false;
    }
    sql += ") VALUES (" + placeholders + ")";

    execute(sql, params);
    const auto element_id = sqlite3_last_insert_rowid(impl_->db);
    impl_->logger->debug("Inserted element with id: {}", element_id);

    // Insert vector groups into vector tables
    const auto& vector_groups = element.vector_groups();
    for (const auto& [group_name, rows] : vector_groups) {
        // Validate vector group types
        impl_->type_validator->validate_vector_group(collection, group_name, rows);

        std::string vector_table = Schema::vector_table_name(collection, group_name);

        if (rows.empty()) {
            throw std::runtime_error("Empty vector group not allowed for group '" + group_name + "'");
        }

        // Build INSERT SQL dynamically based on columns in the first row
        // All rows in a group must have the same columns
        for (size_t row_idx = 0; row_idx < rows.size(); ++row_idx) {
            const auto& row = rows[row_idx];
            int64_t vector_index = static_cast<int64_t>(row_idx + 1);  // 1-based index

            std::string vec_sql = "INSERT INTO " + vector_table + " (id, vector_index";
            std::string vec_placeholders = "?, ?";
            std::vector<Value> vec_params = {element_id, vector_index};

            for (const auto& [col_name, value] : row) {
                vec_sql += ", " + col_name;
                vec_placeholders += ", ?";
                vec_params.push_back(value);
            }
            vec_sql += ") VALUES (" + vec_placeholders + ")";

            execute(vec_sql, vec_params);
        }
        impl_->logger->debug("Inserted {} vector rows for group {}", rows.size(), group_name);
    }

    // Insert set groups into set tables
    const auto& set_groups = element.set_groups();
    for (const auto& [group_name, rows] : set_groups) {
        // Validate set group types
        impl_->type_validator->validate_set_group(collection, group_name, rows);

        std::string set_table = Schema::set_table_name(collection, group_name);

        for (const auto& row : rows) {
            std::string set_sql = "INSERT INTO " + set_table + " (id";
            std::string set_placeholders = "?";
            std::vector<Value> set_params = {element_id};

            for (const auto& [col_name, value] : row) {
                set_sql += ", " + col_name;
                set_placeholders += ", ?";
                set_params.push_back(value);
            }
            set_sql += ") VALUES (" + set_placeholders + ")";

            execute(set_sql, set_params);
        }
        impl_->logger->debug("Inserted {} set rows for group {}", rows.size(), group_name);
    }

    impl_->logger->info("Created element {} in {}", element_id, collection);
    return element_id;
}

std::vector<Value> Database::read_scalar_parameters(const std::string& collection, const std::string& attribute) const {
    impl_->logger->debug("Reading scalar parameters: {}.{}", collection, attribute);

    if (!impl_->schema) {
        throw std::runtime_error("Cannot read parameters: no schema loaded");
    }

    if (!impl_->schema->is_collection(collection)) {
        throw std::runtime_error("'" + collection + "' is not a valid collection");
    }

    const auto* table_def = impl_->schema->get_table(collection);
    if (!table_def) {
        throw std::runtime_error("Collection not found in schema: " + collection);
    }
    if (!table_def->has_column(attribute)) {
        throw std::runtime_error("Attribute '" + attribute + "' not found in collection '" + collection + "'");
    }

    std::string sql = "SELECT " + attribute + " FROM " + collection + " ORDER BY id";
    auto result = const_cast<Database*>(this)->execute(sql);

    std::vector<Value> values;
    values.reserve(result.row_count());
    for (const auto& row : result) {
        values.push_back(row.at(0));
    }

    impl_->logger->debug("Read {} values for {}.{}", values.size(), collection, attribute);
    return values;
}

Value Database::read_scalar_parameter(const std::string& collection,
                                      const std::string& attribute,
                                      const std::string& label) const {
    impl_->logger->debug("Reading scalar parameter: {}.{} for label '{}'", collection, attribute, label);

    if (!impl_->schema) {
        throw std::runtime_error("Cannot read parameter: no schema loaded");
    }

    if (!impl_->schema->is_collection(collection)) {
        throw std::runtime_error("'" + collection + "' is not a valid collection");
    }

    const auto* table_def = impl_->schema->get_table(collection);
    if (!table_def) {
        throw std::runtime_error("Collection not found in schema: " + collection);
    }
    if (!table_def->has_column(attribute)) {
        throw std::runtime_error("Attribute '" + attribute + "' not found in collection '" + collection + "'");
    }

    std::string sql = "SELECT " + attribute + " FROM " + collection + " WHERE label = ?";
    auto result = const_cast<Database*>(this)->execute(sql, {label});

    if (result.empty()) {
        throw std::runtime_error("Element with label '" + label + "' not found in collection '" + collection + "'");
    }

    return result.at(0).at(0);
}

}  // namespace psr
