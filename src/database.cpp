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

    auto flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
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
            auto type = sqlite3_column_type(stmt, i);
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
            case SQLITE_NULL:
                values.emplace_back(nullptr);
                break;
            case SQLITE_BLOB:
                throw std::runtime_error("Blob not implemented");
            default:
                throw std::runtime_error("Type not implemented");
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

    // Require schema to be loaded
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

    // Build INSERT SQL for main collection table
    auto sql = "INSERT INTO " + collection + " (";
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

    // Process arrays - route to vector or set tables based on schema
    const auto& arrays = element.arrays();

    // Build a map of set table -> (column_name -> array values)
    std::map<std::string, std::map<std::string, const std::vector<Value>*>> set_table_columns;
    // Build a map of vector table -> (column_name -> array values)
    std::map<std::string, std::map<std::string, const std::vector<Value>*>> vector_table_columns;

    for (const auto& [array_name, values] : arrays) {
        if (values.empty()) {
            throw std::runtime_error("Empty array not allowed for '" + array_name + "'");
        }

        // Check if this is a single-column vector table (Collection_vector_arrayname)
        auto vector_table = Schema::vector_table_name(collection, array_name);
        if (impl_->schema->has_table(vector_table)) {
            vector_table_columns[vector_table][array_name] = &values;
            continue;
        }

        // Check if this array_name is a column in any vector table for the collection
        bool found_vector_table = false;
        for (const auto& table_name : impl_->schema->table_names()) {
            if (!impl_->schema->is_vector_table(table_name))
                continue;
            if (impl_->schema->get_parent_collection(table_name) != collection)
                continue;

            const auto* table_def = impl_->schema->get_table(table_name);
            if (table_def && table_def->has_column(array_name)) {
                vector_table_columns[table_name][array_name] = &values;
                found_vector_table = true;
                break;
            }
        }

        if (found_vector_table) {
            continue;
        }

        // Check if this array_name is a column in any set table
        bool found_set_table = false;
        for (const auto& table_name : impl_->schema->table_names()) {
            if (!impl_->schema->is_set_table(table_name))
                continue;
            if (impl_->schema->get_parent_collection(table_name) != collection)
                continue;

            const auto* table_def = impl_->schema->get_table(table_name);
            if (table_def && table_def->has_column(array_name)) {
                set_table_columns[table_name][array_name] = &values;
                found_set_table = true;
                break;
            }
        }

        if (!found_set_table) {
            throw std::runtime_error("Array '" + array_name +
                                     "' does not match any vector or set table for collection '" + collection + "'");
        }
    }

    // Insert vector table data - zip arrays together into rows with vector_index
    for (const auto& [vector_table, columns] : vector_table_columns) {
        const auto* table_def = impl_->schema->get_table(vector_table);
        if (!table_def) {
            throw std::runtime_error("Vector table not found: " + vector_table);
        }

        // Verify all arrays have same length
        size_t num_rows = 0;
        for (const auto& [col_name, values_ptr] : columns) {
            impl_->type_validator->validate_array(vector_table, col_name, *values_ptr);
            if (num_rows == 0) {
                num_rows = values_ptr->size();
            } else if (values_ptr->size() != num_rows) {
                throw std::runtime_error("Vector columns in table '" + vector_table +
                                         "' must have the same length, but got different lengths for columns");
            }
        }

        // Insert each row with vector_index
        for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
            int64_t vector_index = static_cast<int64_t>(row_idx + 1);
            auto vec_sql = "INSERT INTO " + vector_table + " (id, vector_index";
            std::string vec_placeholders = "?, ?";
            std::vector<Value> vec_params = {element_id, vector_index};

            for (const auto& [col_name, values_ptr] : columns) {
                vec_sql += ", " + col_name;
                vec_placeholders += ", ?";
                vec_params.push_back((*values_ptr)[row_idx]);
            }

            vec_sql += ") VALUES (" + vec_placeholders + ")";
            execute(vec_sql, vec_params);
        }
        impl_->logger->debug("Inserted {} vector rows into {}", num_rows, vector_table);
    }

    // Insert set table data - zip arrays together into rows
    for (const auto& [set_table, columns] : set_table_columns) {
        const auto* table_def = impl_->schema->get_table(set_table);
        if (!table_def) {
            throw std::runtime_error("Set table not found: " + set_table);
        }

        // Verify all arrays have same length
        size_t num_rows = 0;
        for (const auto& [col_name, values_ptr] : columns) {
            if (num_rows == 0) {
                num_rows = values_ptr->size();
            } else if (values_ptr->size() != num_rows) {
                throw std::runtime_error("Set columns in table '" + set_table +
                                         "' must have the same length, but got different lengths for columns");
            }
        }

        // Insert each row
        for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
            auto set_sql = "INSERT INTO " + set_table + " (id";
            std::string set_placeholders = "?";
            std::vector<Value> set_params = {element_id};

            for (const auto& [col_name, values_ptr] : columns) {
                set_sql += ", " + col_name;
                set_placeholders += ", ?";

                Value val = (*values_ptr)[row_idx];

                // Check if this column is a FK and value is a string (label) that needs resolution
                for (const auto& fk : table_def->foreign_keys) {
                    if (fk.from_column == col_name && std::holds_alternative<std::string>(val)) {
                        const std::string& label = std::get<std::string>(val);
                        // Look up the ID by label using direct SQL query
                        auto lookup_sql = "SELECT id FROM " + fk.to_table + " WHERE label = ?";
                        auto lookup_result = execute(lookup_sql, {label});
                        if (lookup_result.empty() || !lookup_result[0].get_int(0)) {
                            throw std::runtime_error("Failed to resolve label '" + label + "' to ID in table '" +
                                                     fk.to_table + "'");
                        }
                        val = lookup_result[0].get_int(0).value();
                        break;
                    }
                }

                set_params.push_back(val);
            }
            set_sql += ") VALUES (" + set_placeholders + ")";
            execute(set_sql, set_params);
        }
        impl_->logger->debug("Inserted {} set rows for table {}", num_rows, set_table);
    }

    impl_->logger->info("Created element {} in {}", element_id, collection);
    return element_id;
}

std::vector<int64_t> Database::read_scalar_ints(const std::string& collection, const std::string& attribute) {
    auto sql = "SELECT " + attribute + " FROM " + collection;
    auto result = execute(sql);

    std::vector<int64_t> values;
    values.reserve(result.row_count());
    for (size_t i = 0; i < result.row_count(); ++i) {
        auto val = result[i].get_int(0);
        if (val) {
            values.push_back(*val);
        }
    }
    return values;
}

std::vector<double> Database::read_scalar_doubles(const std::string& collection, const std::string& attribute) {
    auto sql = "SELECT " + attribute + " FROM " + collection;
    auto result = execute(sql);

    std::vector<double> values;
    values.reserve(result.row_count());
    for (size_t i = 0; i < result.row_count(); ++i) {
        auto val = result[i].get_double(0);
        if (val) {
            values.push_back(*val);
        }
    }
    return values;
}

std::vector<std::string> Database::read_scalar_strings(const std::string& collection, const std::string& attribute) {
    auto sql = "SELECT " + attribute + " FROM " + collection;
    auto result = execute(sql);

    std::vector<std::string> values;
    values.reserve(result.row_count());
    for (size_t i = 0; i < result.row_count(); ++i) {
        auto val = result[i].get_string(0);
        if (val) {
            values.push_back(*val);
        }
    }
    return values;
}

std::vector<std::vector<int64_t>> Database::read_vector_ints(const std::string& collection,
                                                             const std::string& attribute) {
    auto vector_table = impl_->schema->find_vector_table(collection, attribute);
    auto sql = "SELECT id, " + attribute + " FROM " + vector_table + " ORDER BY id, vector_index";
    auto result = execute(sql);

    std::vector<std::vector<int64_t>> vectors;
    int64_t current_id = -1;

    for (size_t i = 0; i < result.row_count(); ++i) {
        auto id = result[i].get_int(0);
        auto val = result[i].get_int(1);

        if (!id)
            continue;

        if (*id != current_id) {
            vectors.emplace_back();
            current_id = *id;
        }

        if (val) {
            vectors.back().push_back(*val);
        }
    }
    return vectors;
}

std::vector<std::vector<double>> Database::read_vector_doubles(const std::string& collection,
                                                               const std::string& attribute) {
    auto vector_table = impl_->schema->find_vector_table(collection, attribute);
    auto sql = "SELECT id, " + attribute + " FROM " + vector_table + " ORDER BY id, vector_index";
    auto result = execute(sql);

    std::vector<std::vector<double>> vectors;
    int64_t current_id = -1;

    for (size_t i = 0; i < result.row_count(); ++i) {
        auto id = result[i].get_int(0);
        auto val = result[i].get_double(1);

        if (!id)
            continue;

        if (*id != current_id) {
            vectors.emplace_back();
            current_id = *id;
        }

        if (val) {
            vectors.back().push_back(*val);
        }
    }
    return vectors;
}

std::vector<std::vector<std::string>> Database::read_vector_strings(const std::string& collection,
                                                                    const std::string& attribute) {
    auto vector_table = impl_->schema->find_vector_table(collection, attribute);
    auto sql = "SELECT id, " + attribute + " FROM " + vector_table + " ORDER BY id, vector_index";
    auto result = execute(sql);

    std::vector<std::vector<std::string>> vectors;
    int64_t current_id = -1;

    for (size_t i = 0; i < result.row_count(); ++i) {
        auto id = result[i].get_int(0);
        auto val = result[i].get_string(1);

        if (!id)
            continue;

        if (*id != current_id) {
            vectors.emplace_back();
            current_id = *id;
        }

        if (val) {
            vectors.back().push_back(*val);
        }
    }
    return vectors;
}

std::vector<std::vector<int64_t>> Database::read_set_ints(const std::string& collection, const std::string& attribute) {
    auto set_table = impl_->schema->find_set_table(collection, attribute);
    auto sql = "SELECT id, " + attribute + " FROM " + set_table + " ORDER BY id";
    auto result = execute(sql);

    std::vector<std::vector<int64_t>> sets;
    int64_t current_id = -1;

    for (size_t i = 0; i < result.row_count(); ++i) {
        auto id = result[i].get_int(0);
        auto val = result[i].get_int(1);

        if (!id)
            continue;

        if (*id != current_id) {
            sets.emplace_back();
            current_id = *id;
        }

        if (val) {
            sets.back().push_back(*val);
        }
    }
    return sets;
}

std::vector<std::vector<double>> Database::read_set_doubles(const std::string& collection,
                                                            const std::string& attribute) {
    auto set_table = impl_->schema->find_set_table(collection, attribute);
    auto sql = "SELECT id, " + attribute + " FROM " + set_table + " ORDER BY id";
    auto result = execute(sql);

    std::vector<std::vector<double>> sets;
    int64_t current_id = -1;

    for (size_t i = 0; i < result.row_count(); ++i) {
        auto id = result[i].get_int(0);
        auto val = result[i].get_double(1);

        if (!id)
            continue;

        if (*id != current_id) {
            sets.emplace_back();
            current_id = *id;
        }

        if (val) {
            sets.back().push_back(*val);
        }
    }
    return sets;
}

std::vector<std::vector<std::string>> Database::read_set_strings(const std::string& collection,
                                                                 const std::string& attribute) {
    auto set_table = impl_->schema->find_set_table(collection, attribute);
    auto sql = "SELECT id, " + attribute + " FROM " + set_table + " ORDER BY id";
    auto result = execute(sql);

    std::vector<std::vector<std::string>> sets;
    int64_t current_id = -1;

    for (size_t i = 0; i < result.row_count(); ++i) {
        auto id = result[i].get_int(0);
        auto val = result[i].get_string(1);

        if (!id)
            continue;

        if (*id != current_id) {
            sets.emplace_back();
            current_id = *id;
        }

        if (val) {
            sets.back().push_back(*val);
        }
    }
    return sets;
}

}  // namespace psr
