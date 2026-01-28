#ifndef QUIVER_DATABASE_H
#define QUIVER_DATABASE_H

#include "export.h"
#include "quiver/attribute_metadata.h"
#include "quiver/element.h"
#include "quiver/log_level.h"
#include "quiver/result.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace quiver {

struct QUIVER_API DatabaseOptions {
    bool read_only = false;
    LogLevel console_level = LogLevel::info;
};

class QUIVER_API Database {
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
    void update_element(const std::string& collection, int64_t id, const Element& element);
    void delete_element_by_id(const std::string& collection, int64_t id);

    // Relation operations
    void set_scalar_relation(const std::string& collection,
                             const std::string& attribute,
                             const std::string& from_label,
                             const std::string& to_label);

    std::vector<std::string> read_scalar_relation(const std::string& collection, const std::string& attribute);

    // Read scalar attributes (all elements)
    std::vector<int64_t> read_scalar_integers(const std::string& collection, const std::string& attribute);
    std::vector<double> read_scalar_floats(const std::string& collection, const std::string& attribute);
    std::vector<std::string> read_scalar_strings(const std::string& collection, const std::string& attribute);

    // Read scalar attributes (by element ID)
    std::optional<int64_t>
    read_scalar_integers_by_id(const std::string& collection, const std::string& attribute, int64_t id);
    std::optional<double>
    read_scalar_floats_by_id(const std::string& collection, const std::string& attribute, int64_t id);
    std::optional<std::string>
    read_scalar_strings_by_id(const std::string& collection, const std::string& attribute, int64_t id);

    // Read vector attributes (all elements)
    std::vector<std::vector<int64_t>> read_vector_integers(const std::string& collection, const std::string& attribute);
    std::vector<std::vector<double>> read_vector_floats(const std::string& collection, const std::string& attribute);
    std::vector<std::vector<std::string>> read_vector_strings(const std::string& collection,
                                                              const std::string& attribute);

    // Read vector attributes (by element ID)
    std::vector<int64_t>
    read_vector_integers_by_id(const std::string& collection, const std::string& attribute, int64_t id);
    std::vector<double>
    read_vector_floats_by_id(const std::string& collection, const std::string& attribute, int64_t id);
    std::vector<std::string>
    read_vector_strings_by_id(const std::string& collection, const std::string& attribute, int64_t id);

    // Read set attributes (all elements)
    std::vector<std::vector<int64_t>> read_set_integers(const std::string& collection, const std::string& attribute);
    std::vector<std::vector<double>> read_set_floats(const std::string& collection, const std::string& attribute);
    std::vector<std::vector<std::string>> read_set_strings(const std::string& collection, const std::string& attribute);

    // Read set attributes (by element ID)
    std::vector<int64_t>
    read_set_integers_by_id(const std::string& collection, const std::string& attribute, int64_t id);
    std::vector<double> read_set_floats_by_id(const std::string& collection, const std::string& attribute, int64_t id);
    std::vector<std::string>
    read_set_strings_by_id(const std::string& collection, const std::string& attribute, int64_t id);

    // Read element IDs
    std::vector<int64_t> read_element_ids(const std::string& collection);

    // Attribute metadata queries
    ScalarMetadata get_scalar_metadata(const std::string& collection, const std::string& attribute) const;
    VectorMetadata get_vector_metadata(const std::string& collection, const std::string& group_name) const;
    SetMetadata get_set_metadata(const std::string& collection, const std::string& group_name) const;

    // List attributes/groups - returns full metadata
    std::vector<ScalarMetadata> list_scalar_attributes(const std::string& collection) const;
    std::vector<VectorMetadata> list_vector_groups(const std::string& collection) const;
    std::vector<SetMetadata> list_set_groups(const std::string& collection) const;

    // Update scalar attributes (by element ID)
    void update_scalar_integer(const std::string& collection, const std::string& attribute, int64_t id, int64_t value);
    void update_scalar_float(const std::string& collection, const std::string& attribute, int64_t id, double value);
    void update_scalar_string(const std::string& collection,
                              const std::string& attribute,
                              int64_t id,
                              const std::string& value);

    // Update vector attributes (by element ID) - replaces entire vector
    void update_vector_integers(const std::string& collection,
                                const std::string& attribute,
                                int64_t id,
                                const std::vector<int64_t>& values);
    void update_vector_floats(const std::string& collection,
                              const std::string& attribute,
                              int64_t id,
                              const std::vector<double>& values);
    void update_vector_strings(const std::string& collection,
                               const std::string& attribute,
                               int64_t id,
                               const std::vector<std::string>& values);

    // Update set attributes (by element ID) - replaces entire set
    void update_set_integers(const std::string& collection,
                             const std::string& attribute,
                             int64_t id,
                             const std::vector<int64_t>& values);
    void update_set_floats(const std::string& collection,
                           const std::string& attribute,
                           int64_t id,
                           const std::vector<double>& values);
    void update_set_strings(const std::string& collection,
                            const std::string& attribute,
                            int64_t id,
                            const std::vector<std::string>& values);

    const std::string& path() const;

    // CSV operations
    void export_to_csv(const std::string& table, const std::string& path);
    void import_from_csv(const std::string& table, const std::string& path);

    // Query methods - execute SQL and return first row's first column
    std::optional<std::string> query_string(const std::string& sql);
    std::optional<int64_t> query_integer(const std::string& sql);
    std::optional<double> query_float(const std::string& sql);

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

}  // namespace quiver

#endif  // QUIVER_DATABASE_H
