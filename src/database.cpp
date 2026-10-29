#include "database_impl.h"
#include "quiver/result.h"
#include "ui_metadata.h"
#include "utils/file.h"
#include "utils/string.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sqlite3.h>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <filesystem>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

std::atomic<uint64_t> g_logger_counter{0};
std::once_flag sqlite3_init_flag;

void ensure_sqlite3_initialized() {
    std::call_once(sqlite3_init_flag, []() { sqlite3_initialize(); });
}

// query_* runs SQL that may come from an untrusted Lua script, so it may only read. SQLite's authorizer refuses
// writes, PRAGMA, ATTACH, DETACH and transaction control while the statement compiles; VACUUM never reaches the
// authorizer, so sqlite3_stmt_readonly catches it after compilation (see Impl::execute).
struct ReadOnlyAuthorizer {
    explicit ReadOnlyAuthorizer(sqlite3* db) : db(db) {
        sqlite3_set_authorizer(db, allow_reads, nullptr);
    }
    ~ReadOnlyAuthorizer() {
        sqlite3_set_authorizer(db, nullptr, nullptr);
    }
    ReadOnlyAuthorizer(const ReadOnlyAuthorizer&) = delete;
    ReadOnlyAuthorizer& operator=(const ReadOnlyAuthorizer&) = delete;

    static int allow_reads(void*, int action, const char*, const char*, const char*, const char*) {
        switch (action) {
        case SQLITE_SELECT:
        case SQLITE_READ:
        case SQLITE_FUNCTION:
        case SQLITE_RECURSIVE:
            return SQLITE_OK;
        default:
            return SQLITE_DENY;
        }
    }

    sqlite3* db;
};

// The numbered subdirectories of a migrations directory, ascending by version. A name counts only
// when it parses whole as a positive integer; anything else is skipped, and a path that is not a
// directory yields none.
std::vector<std::pair<int64_t, std::filesystem::path>> list_migrations(const std::string& path) {
    namespace fs = std::filesystem;
    std::vector<std::pair<int64_t, fs::path>> migrations;
    if (!fs::is_directory(path)) {
        return migrations;
    }
    for (const auto& entry : fs::directory_iterator(path)) {
        if (!entry.is_directory()) {
            continue;
        }
        const auto name = entry.path().filename().string();
        const auto* end = name.data() + name.size();
        int64_t version = 0;
        const auto [parsed, error] = std::from_chars(name.data(), end, version);
        if (error == std::errc{} && parsed == end && version > 0) {
            migrations.emplace_back(version, entry.path());
        }
    }
    std::sort(migrations.begin(), migrations.end());
    return migrations;
}

constexpr auto kNotReadOnly =
    "Cannot query: SQL must be read-only; writes, PRAGMA, ATTACH, VACUUM and transaction control are not allowed";

spdlog::level::level_enum to_spdlog_level(quiver::LogLevel level) {
    switch (level) {
    case quiver::LogLevel::Debug:
        return spdlog::level::debug;
    case quiver::LogLevel::Info:
        return spdlog::level::info;
    case quiver::LogLevel::Warn:
        return spdlog::level::warn;
    case quiver::LogLevel::Error:
        return spdlog::level::err;
    case quiver::LogLevel::Off:
        return spdlog::level::off;
    default:
        return spdlog::level::info;
    }
}

std::shared_ptr<spdlog::logger> create_database_logger(
    const std::string& db_path,
    quiver::LogLevel console_level,
    bool read_only
) {
    namespace fs = std::filesystem;

    // Generate unique logger name for multiple Database instances
    auto id = g_logger_counter.fetch_add(1);
    auto logger_name = "quiver_database_" + std::to_string(id);

    // Create console sink (thread-safe)
    auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    console_sink->set_level(to_spdlog_level(console_level));

    // No file logging in memory, nor for a read-only handle, which writes nothing to disk: the file
    // sink would create or truncate quiver_database.log next to the database, wiping the trail of
    // any writer on it.
    if (db_path == ":memory:" || read_only) {
        const auto logger = std::make_shared<spdlog::logger>(logger_name, console_sink);
        logger->set_level(spdlog::level::debug);
        if (db_path == ":memory:") {
            logger->warn("Database is in-memory only; no file logging will be performed.");
        }
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
            const auto log_file_path = (db_dir / "quiver_database.log").string();
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

namespace quiver {

Database::Database(const std::string& path, const DatabaseOptions& options) : impl_(std::make_unique<Impl>()) {
    impl_->path = path;
    impl_->read_only = options.read_only;
    impl_->logger = create_database_logger(path, options.console_level, options.read_only);

    impl_->logger->debug("Opening database: {}", path);

    ensure_sqlite3_initialized();

    auto flags = options.read_only ? SQLITE_OPEN_READONLY : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
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

Database::~Database() = default;

Database::Database(Database&& other) noexcept = default;
Database& Database::operator=(Database&& other) noexcept = default;

bool Database::is_healthy() const {
    return impl_ && impl_->db != nullptr;
}

Result Database::Impl::execute(const std::string& sql, const std::vector<Value>& parameters, bool read_only) const {
    sqlite3_stmt* raw_stmt = nullptr;
    auto rc = SQLITE_OK;
    {
        std::optional<ReadOnlyAuthorizer> authorizer;
        if (read_only) {
            authorizer.emplace(db);
        }
        rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &raw_stmt, nullptr);
    }
    if (rc == SQLITE_AUTH) {
        throw std::runtime_error(kNotReadOnly);
    }
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
    }
    StmtPtr stmt(raw_stmt, sqlite3_finalize);
    if (read_only && !sqlite3_stmt_readonly(stmt.get())) {
        throw std::runtime_error(kNotReadOnly);
    }

    // Reject a parameter-count mismatch loudly: too few would bind NULL to the trailing
    // placeholder, too many would silently ignore the extras.
    const auto expected_parameters = static_cast<size_t>(sqlite3_bind_parameter_count(stmt.get()));
    if (expected_parameters != parameters.size()) {
        throw std::runtime_error(
            "Failed to execute statement: expected " + std::to_string(expected_parameters) +
            " bound parameter(s) but got " + std::to_string(parameters.size())
        );
    }

    // Bind parameters
    for (size_t i = 0; i < parameters.size(); ++i) {
        const auto idx = static_cast<int>(i + 1);
        const auto& parameter = parameters[i];

        std::visit(
            [&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::nullptr_t>) {
                    sqlite3_bind_null(stmt.get(), idx);
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    sqlite3_bind_int64(stmt.get(), idx, arg);
                } else if constexpr (std::is_same_v<T, double>) {
                    sqlite3_bind_double(stmt.get(), idx, arg);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    auto trimmed = string::trim(arg);
                    sqlite3_bind_text(
                        stmt.get(),
                        idx,
                        trimmed.c_str(),
                        static_cast<int>(trimmed.size()),
                        SQLITE_TRANSIENT
                    );  // NOLINT(performance-no-int-to-ptr) SQLite macro
                }
            },
            parameter
        );
    }

    // Get column info
    std::vector<std::string> columns;
    const auto col_count = sqlite3_column_count(stmt.get());
    columns.reserve(col_count);
    for (int i = 0; i < col_count; ++i) {
        const char* name = sqlite3_column_name(stmt.get(), i);
        columns.emplace_back(name ? name : "");
    }

    // Execute and fetch results
    std::vector<Row> rows;
    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        std::vector<Value> values;
        values.reserve(col_count);

        for (int i = 0; i < col_count; ++i) {
            auto type = sqlite3_column_type(stmt.get(), i);
            switch (type) {
            case SQLITE_INTEGER:
                values.emplace_back(sqlite3_column_int64(stmt.get(), i));
                break;
            case SQLITE_FLOAT:
                values.emplace_back(sqlite3_column_double(stmt.get(), i));
                break;
            case SQLITE_TEXT: {
                const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), i));
                values.emplace_back(std::string(text ? text : ""));
                break;
            }
            case SQLITE_NULL:
                values.emplace_back(nullptr);
                break;
            case SQLITE_BLOB:
                throw std::runtime_error("Failed to execute statement: BLOB type not supported");
            default:
                throw std::runtime_error("Failed to execute statement: unknown column type");
                break;
            }
        }
        rows.emplace_back(std::move(values));
    }

    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(db)));
    }

    return {std::move(columns), std::move(rows)};
}

void Database::Impl::execute_raw(const std::string& sql, const char* what) const {
    char* err_msg = nullptr;
    const auto rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error(std::string("Failed to ") + what + ": " + error);
    }
}

int64_t Database::current_version() const {
    return *impl_->execute("PRAGMA user_version")[0].get_integer(0);
}

const std::string& Database::path() const {
    return impl_->path;
}

Database Database::from_migrations(
    const std::string& db_path,
    const std::string& migrations_path,
    const DatabaseOptions& options
) {
    namespace fs = std::filesystem;
    if (!fs::exists(migrations_path)) {
        throw std::runtime_error("Migrations path not found: " + migrations_path);
    }
    if (!fs::is_directory(migrations_path)) {
        throw std::runtime_error("Cannot from_migrations: path is not a directory: " + migrations_path);
    }
    auto db = Database(db_path, options);
    if (options.read_only) {
        // Nothing can be migrated, so the database must already be where the migrations lead; the
        // ui/ sidecar is written for that version. The read-only open never creates a missing file.
        const auto migrations = list_migrations(migrations_path);
        const auto current = db.current_version();
        if (!migrations.empty() && migrations.back().first > current) {
            throw std::runtime_error(
                "Cannot from_migrations: database is at version " + std::to_string(current) + " but migrations reach " +
                std::to_string(migrations.back().first) + "; a read-only open cannot apply them"
            );
        }
    } else {
        db.migrate_up(migrations_path, "from_migrations");
    }
    db.impl_->ui_metadata = load_ui_metadata(migrations_path, *db.impl_->logger);
    return db;
}

void Database::validate_migrations(const std::string& migrations_path) {
    namespace fs = std::filesystem;
    if (!fs::exists(migrations_path)) {
        throw std::runtime_error("Cannot validate_migrations: migrations path not found: " + migrations_path);
    }
    if (!fs::is_directory(migrations_path)) {
        throw std::runtime_error("Cannot validate_migrations: path is not a directory: " + migrations_path);
    }
    if (list_migrations(migrations_path).empty()) {
        throw std::runtime_error("Cannot validate_migrations: no migrations found in " + migrations_path);
    }

    Database db(":memory:", {.console_level = LogLevel::Off});
    db.migrate_up(migrations_path, "validate_migrations");
    db.migrate_down(migrations_path);

    const auto leftovers = Schema::from_database(db.impl_->db).table_names();
    if (!leftovers.empty()) {
        std::string names;
        for (const auto& table : leftovers) {
            names += (names.empty() ? "" : ", ") + table;
        }
        throw std::runtime_error("Failed to validate_migrations: down migrations left tables behind: " + names);
    }
}

Database Database::from_schema(
    const std::string& db_path,
    const std::string& schema_path,
    const DatabaseOptions& options
) {
    namespace fs = std::filesystem;
    if (options.read_only) {
        throw std::runtime_error("Cannot from_schema: read_only mode (use Database constructor to open existing)");
    }
    if (!fs::exists(schema_path)) {
        throw std::runtime_error("Schema file not found: " + schema_path);
    }
    if (db_path != ":memory:") {
        fs::remove(db_path);
    }
    auto db = Database(db_path, options);
    db.apply_schema(schema_path);
    return db;
}

void Database::set_version(int64_t version) {
    impl_->execute_raw("PRAGMA user_version = " + std::to_string(version) + ";", "set user_version");
    impl_->logger->debug("Set database version to {}", version);
}

void Database::begin_transaction() {
    if (impl_->dry_run) {
        return;  // absorbed: the dry run already owns the transaction
    }
    if (!sqlite3_get_autocommit(impl_->db)) {
        throw std::runtime_error("Cannot begin_transaction: transaction already active");
    }
    impl_->begin_transaction();
    impl_->logger->debug("User transaction started");
}

bool Database::in_transaction() const {
    return !sqlite3_get_autocommit(impl_->db);
}

void Database::commit() {
    if (impl_->dry_run) {
        return;  // absorbed: end_dry_run decides, and it always rolls back
    }
    if (sqlite3_get_autocommit(impl_->db)) {
        throw std::runtime_error("Cannot commit: no active transaction");
    }
    impl_->commit();
    impl_->logger->debug("User transaction committed");
}

void Database::rollback() {
    if (impl_->dry_run) {
        return;  // absorbed: end_dry_run undoes everything anyway
    }
    if (sqlite3_get_autocommit(impl_->db)) {
        throw std::runtime_error("Cannot rollback: no active transaction");
    }
    impl_->rollback();
    impl_->logger->debug("User transaction rolled back");
}

void Database::begin_dry_run() {
    if (impl_->dry_run) {
        throw std::runtime_error("Cannot begin_dry_run: dry run already active");
    }
    if (!sqlite3_get_autocommit(impl_->db)) {
        throw std::runtime_error("Cannot begin_dry_run: transaction already active");
    }
    impl_->begin_transaction();
    impl_->dry_run = true;
    impl_->logger->debug("Dry run started");
}

void Database::end_dry_run() {
    if (!impl_->dry_run) {
        throw std::runtime_error("Cannot end_dry_run: no active dry run");
    }
    // Guarded: a caller can still end the transaction out from under us with a raw COMMIT
    // through query_*.
    if (!sqlite3_get_autocommit(impl_->db)) {
        impl_->rollback();  // logs a failure instead of throwing, so verify it took effect
        if (!sqlite3_get_autocommit(impl_->db)) {
            // The flag stays set on purpose: begin/commit/rollback keep being absorbed (so nothing
            // can commit what was meant to be discarded) and the caller can retry end_dry_run.
            throw std::runtime_error("Failed to end_dry_run: rollback left the transaction open");
        }
    }
    impl_->dry_run = false;
    impl_->logger->debug("Dry run rolled back");
}

bool Database::in_dry_run() const {
    return impl_->dry_run;
}

void Database::migrate_up(const std::string& migrations_path, const char* operation) {
    const auto migrations = list_migrations(migrations_path);
    if (migrations.empty()) {
        impl_->logger->debug("No migrations found in {}", migrations_path);
        return;
    }

    const auto current = current_version();
    const auto pending =
        std::find_if(migrations.begin(), migrations.end(), [current](const auto& m) { return m.first > current; });

    if (pending == migrations.end()) {
        impl_->logger->debug("Database is up to date at version {}", current);
        return;
    }

    impl_->logger->info(
        "Applying {} pending migration(s) from version {} to {}",
        migrations.end() - pending,
        current,
        migrations.back().first
    );

    for (auto it = pending; it != migrations.end(); ++it) {
        const auto& [version, path] = *it;
        impl_->logger->info("Applying migration {}", version);

        const auto up_sql = utils::read_file(path / "up.sql").value_or("");
        if (up_sql.empty()) {
            throw std::runtime_error(
                std::string("Cannot ") + operation + ": migration " + std::to_string(version) + " has no up.sql file"
            );
        }

        try {
            Impl::TransactionGuard txn(*impl_);
            impl_->execute_raw(up_sql);
            set_version(version);
            txn.commit();
            impl_->logger->info("Migration {} applied successfully", version);
        } catch (const std::exception& e) {
            impl_->logger->error("Migration {} failed: {}", version, e.what());
            throw std::runtime_error(
                std::string("Failed to ") + operation + ": up migration " + std::to_string(version) + ": " + e.what()
            );
        }
    }

    impl_->load_schema_metadata();
    impl_->logger->info("All migrations applied successfully. Database now at version {}", current_version());
}

void Database::migrate_down(const std::string& migrations_path) {
    const auto migrations = list_migrations(migrations_path);
    if (migrations.empty()) {
        impl_->logger->debug("No migrations found in {}", migrations_path);
        return;
    }

    impl_->logger->info("Reverting {} migration(s) from version {} to 0", migrations.size(), migrations.back().first);

    for (auto it = migrations.rbegin(); it != migrations.rend(); ++it) {
        const auto& [version, path] = *it;
        impl_->logger->info("Reverting migration {}", version);

        const auto down_sql = utils::read_file(path / "down.sql").value_or("");
        if (down_sql.empty()) {
            throw std::runtime_error(
                "Cannot validate_migrations: migration " + std::to_string(version) + " has no down.sql file"
            );
        }

        const auto preceding_version = it + 1 == migrations.rend() ? 0 : (it + 1)->first;
        try {
            Impl::TransactionGuard txn(*impl_);
            impl_->execute_raw(down_sql);
            set_version(preceding_version);
            txn.commit();
            impl_->logger->info("Migration {} reverted successfully", version);
        } catch (const std::exception& e) {
            impl_->logger->error("Migration {} failed: {}", version, e.what());
            throw std::runtime_error(
                "Failed to validate_migrations: down migration " + std::to_string(version) + ": " + e.what()
            );
        }
    }
}

void Database::apply_schema(const std::string& schema_path) {
    const auto schema_sql = utils::read_file(schema_path);
    if (!schema_sql) {
        throw std::runtime_error("Failed to from_schema: could not open file: " + schema_path);
    }
    if (schema_sql->empty()) {
        throw std::runtime_error("Cannot from_schema: schema file is empty: " + schema_path);
    }

    impl_->logger->info("Applying schema from: {}", schema_path);

    try {
        Impl::TransactionGuard txn(*impl_);
        impl_->execute_raw(*schema_sql);
        impl_->load_schema_metadata();
        txn.commit();
    } catch (const std::exception& e) {
        impl_->logger->error("Failed to apply schema: {}", e.what());
        throw;
    }

    impl_->logger->info("Schema applied successfully");
}

}  // namespace quiver
