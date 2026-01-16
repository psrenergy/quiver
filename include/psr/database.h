#ifndef PSR_DATABASE_H
#define PSR_DATABASE_H

#include "export.h"
#include "psr/element.h"
#include "psr/log_level.h"
#include "psr/result.h"

#include <memory>
#include <optional>
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

    int64_t current_version() const;

    // Element operations
    int64_t create_element(const std::string& collection, const Element& element);

    // Relation operations
    void set_scalar_relation(const std::string& collection,
                             const std::string& attribute,
                             const std::string& from_label,
                             const std::string& to_label);

    std::vector<std::string> read_scalar_relation(const std::string& collection, const std::string& attribute);

    // Read scalar attributes (all elements)
    std::vector<int64_t> read_scalar_integers(const std::string& collection, const std::string& attribute);
    std::vector<double> read_scalar_doubles(const std::string& collection, const std::string& attribute);
    std::vector<std::string> read_scalar_strings(const std::string& collection, const std::string& attribute);

    // Read scalar attributes (by element ID)
    std::optional<int64_t>
    read_scalar_integers_by_id(const std::string& collection, const std::string& attribute, int64_t id);
    std::optional<double>
    read_scalar_doubles_by_id(const std::string& collection, const std::string& attribute, int64_t id);
    std::optional<std::string>
    read_scalar_strings_by_id(const std::string& collection, const std::string& attribute, int64_t id);

    // Read vector attributes (all elements)
    std::vector<std::vector<int64_t>> read_vector_integers(const std::string& collection, const std::string& attribute);
    std::vector<std::vector<double>> read_vector_doubles(const std::string& collection, const std::string& attribute);
    std::vector<std::vector<std::string>> read_vector_strings(const std::string& collection,
                                                              const std::string& attribute);

    // Read vector attributes (by element ID)
    std::vector<int64_t>
    read_vector_integers_by_id(const std::string& collection, const std::string& attribute, int64_t id);
    std::vector<double>
    read_vector_doubles_by_id(const std::string& collection, const std::string& attribute, int64_t id);
    std::vector<std::string>
    read_vector_strings_by_id(const std::string& collection, const std::string& attribute, int64_t id);

    // Read set attributes (all elements)
    std::vector<std::vector<int64_t>> read_set_integers(const std::string& collection, const std::string& attribute);
    std::vector<std::vector<double>> read_set_doubles(const std::string& collection, const std::string& attribute);
    std::vector<std::vector<std::string>> read_set_strings(const std::string& collection, const std::string& attribute);

    // Read set attributes (by element ID)
    std::vector<int64_t>
    read_set_integers_by_id(const std::string& collection, const std::string& attribute, int64_t id);
    std::vector<double> read_set_doubles_by_id(const std::string& collection, const std::string& attribute, int64_t id);
    std::vector<std::string>
    read_set_strings_by_id(const std::string& collection, const std::string& attribute, int64_t id);

    // Read element IDs
    std::vector<int64_t> read_element_ids(const std::string& collection);

    const std::string& path() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // Internal helper for executing raw SQL (for migrations)
    void execute_raw(const std::string& sql);

    // Internal method for parameterized queries
    Result execute(const std::string& sql, const std::vector<Value>& params = {});

    // Internal methods
    void set_version(int64_t version);
    void migrate_up(const std::string& migration_path);
    void apply_schema(const std::string& schema_path);
    void begin_transaction();
    void commit();
    void rollback();
};

}  // namespace psr

#endif  // PSR_DATABASE_H
