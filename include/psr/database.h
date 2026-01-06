#ifndef PSR_DATABASE_H
#define PSR_DATABASE_H

#include "export.h"
#include "psr/log_level.h"
#include "psr/result.h"

#include <memory>
#include <string>
#include <vector>

namespace psr {

struct PSR_API DatabaseOptions {
    bool read_only = false;
    LogLevel console_level = LogLevel::info;
};

class PSR_API Database {
public:
    explicit Database(const std::string& path, const DatabaseOptions& options = DatabaseOptions());
    ~Database();

    // Non-copyable
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Movable
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;

    static Database from_migrations(const std::string& db_path,
                                    const std::string& migrations_path,
                                    const DatabaseOptions& options = DatabaseOptions());

    static Database from_schema(const std::string& db_path,
                                const std::string& schema_path,
                                const DatabaseOptions& options = DatabaseOptions());
    bool is_healthy() const;

    Result execute(const std::string& sql, const std::vector<Value>& params = {});

    int64_t current_version() const;
    void set_version(int64_t version);

    void migrate_up(const std::string& migration_path);
    void apply_schema(const std::string& schema_path);

    // Transaction management
    void begin_transaction();
    void commit();
    void rollback();

    const std::string& path() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // Internal helper for executing raw SQL (for migrations)
    void execute_raw(const std::string& sql);
};

}  // namespace psr

#endif  // PSR_DATABASE_H
