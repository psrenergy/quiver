#ifndef QUIVER_DATABASE_H
#define QUIVER_DATABASE_H

#include "export.h"
#include "quiver/attribute_metadata.h"
#include "quiver/element.h"
#include "quiver/options.h"
#include "quiver/result.h"

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace quiver {

class QUIVER_API Database {
public:
    explicit Database(const std::string& path, const DatabaseOptions& options = {});
    ~Database();

    // Non-copyable
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Movable
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;

    static Database from_migrations(const std::string& db_path,
                                    const std::string& migrations_path,
                                    const DatabaseOptions& options = {});

    static Database
    from_schema(const std::string& db_path, const std::string& schema_path, const DatabaseOptions& options = {});
    bool is_healthy() const;

    int64_t current_version() const;

    // Create operations
    int64_t create_element(const std::string& collection, const Element& element);

    // Update operations
    void update_element(const std::string& collection, int64_t id, const Element& element);

    // Delete operations
    void delete_element(const std::string& collection, int64_t id);
    void delete_element(const std::string& collection, const std::string& label);

    // Read scalar attributes (all elements). One entry per element, aligned with read_element_ids;
    // a SQL NULL is std::nullopt (positional — never dropped).
    std::vector<std::optional<int64_t>> read_scalar_integers(const std::string& collection,
                                                             const std::string& attribute);
    std::vector<std::optional<double>> read_scalar_floats(const std::string& collection, const std::string& attribute);
    std::vector<std::optional<std::string>> read_scalar_strings(const std::string& collection,
                                                                const std::string& attribute);

    // Read scalar attributes (by element ID)
    std::optional<int64_t>
    read_scalar_integer_by_id(const std::string& collection, const std::string& attribute, int64_t id);
    std::optional<double>
    read_scalar_float_by_id(const std::string& collection, const std::string& attribute, int64_t id);
    std::optional<std::string>
    read_scalar_string_by_id(const std::string& collection, const std::string& attribute, int64_t id);

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

    // Read a whole vector/set group (by element ID) - returns rows keyed by value column,
    // positionally aligned with SQL NULL cells preserved as nullptr Values
    std::vector<std::map<std::string, Value>>
    read_vector_group_by_id(const std::string& collection, const std::string& group, int64_t id);
    std::vector<std::map<std::string, Value>>
    read_set_group_by_id(const std::string& collection, const std::string& group, int64_t id);

    // Update a whole vector/set group (by element ID) - replaces all rows for the element.
    // The write counterpart of read_vector_group_by_id / read_set_group_by_id, and the
    // group-scoped alternative to passing arrays through update_element: a column name alone
    // is ambiguous when two groups of a collection share it (legal for foreign-key columns),
    // whereas (collection, group) names exactly one table. An empty row list clears the group.
    // Columns are taken from the union of every row's keys and validated against the group; a
    // cell missing from a row is SQL NULL. Throws if the element does not exist ("Element not
    // found"), if a column is unknown, or if a column is "id"/"vector_index" - both are derived
    // (the element and the row's position), so accepting one would silently discard the value.
    void update_vector_group(const std::string& collection,
                             const std::string& group,
                             int64_t id,
                             const std::vector<std::map<std::string, Value>>& rows);
    void update_set_group(const std::string& collection,
                          const std::string& group,
                          int64_t id,
                          const std::vector<std::map<std::string, Value>>& rows);

    // Read element Ids
    std::vector<int64_t> read_element_ids(const std::string& collection);

    // Current number of elements in a collection.
    int64_t number_of_elements(const std::string& collection) const;

    // Attribute metadata queries
    ScalarMetadata get_scalar_metadata(const std::string& collection, const std::string& attribute) const;
    GroupMetadata get_vector_metadata(const std::string& collection, const std::string& group_name) const;
    GroupMetadata get_set_metadata(const std::string& collection, const std::string& group_name) const;

    // List attributes/groups - returns full metadata
    std::vector<ScalarMetadata> list_scalar_attributes(const std::string& collection) const;
    std::vector<GroupMetadata> list_vector_groups(const std::string& collection) const;
    std::vector<GroupMetadata> list_set_groups(const std::string& collection) const;
    std::vector<GroupMetadata> list_time_series_groups(const std::string& collection) const;

    // Time series metadata
    GroupMetadata get_time_series_metadata(const std::string& collection, const std::string& group_name) const;

    // Read time series group - returns rows with date_time and value columns
    std::vector<std::map<std::string, Value>>
    read_time_series_group(const std::string& collection, const std::string& group, int64_t id);

    // Read time series row - returns one value per element for a specific attribute at a given date_time
    // Uses "last non-null value at or before date_time" lookup semantics
    // Returns nullptr Value for elements with no matching data
    std::vector<Value> read_time_series_row(const std::string& collection,
                                            const std::string& group,
                                            const std::string& attribute,
                                            const std::string& date_time);

    // Update time series group - replaces all rows for element
    void update_time_series_group(const std::string& collection,
                                  const std::string& group,
                                  int64_t id,
                                  const std::vector<std::map<std::string, Value>>& rows);

    // Add (or upsert) a single time series row. Inserts a new row identified by id +
    // every dimension column from the schema PK; if a row with the same PK already
    // exists, value columns are overwritten. Validation matches update_time_series_group:
    // every dimension column must be present, unknown columns or type mismatches throw
    // "Cannot upsert_time_series_row: ..." (canonical Pattern 1). Participates in the
    // existing nest-aware TransactionGuard.
    void upsert_time_series_row(const std::string& collection,
                                const std::string& group,
                                int64_t id,
                                const std::map<std::string, Value>& row);

    // Time series files - singleton table storing file paths for external time series data
    bool has_time_series_files(const std::string& collection) const;
    std::vector<std::string> list_time_series_files_columns(const std::string& collection) const;
    std::map<std::string, std::optional<std::string>> read_time_series_files(const std::string& collection);
    void update_time_series_files(const std::string& collection,
                                  const std::map<std::string, std::optional<std::string>>& paths);

    const std::string& path() const;

    // Schema inspection — human-readable text reports.
    // describe(): whole-database overview (every collection, element counts, attribute/group names).
    std::string describe() const;
    // describe_collection(): one collection's structure (scalars, vector/set/time-series groups).
    std::string describe_collection(const std::string& collection) const;
    // summarize_collection(): one collection's value statistics (null/non-null counts, low-cardinality
    // integer value distributions, per-group empty/non-empty counts).
    std::string summarize_collection(const std::string& collection) const;

    // CSV operations
    void export_csv(const std::string& collection,
                    const std::string& group,
                    const std::string& path,
                    const CSVOptions& options = default_csv_options());
    void import_csv(const std::string& collection,
                    const std::string& group,
                    const std::string& path,
                    const CSVOptions& options = default_csv_options());

    // Query methods - execute SQL and return first row's first column
    std::optional<std::string> query_string(const std::string& sql, const std::vector<Value>& parameters = {});
    std::optional<int64_t> query_integer(const std::string& sql, const std::vector<Value>& parameters = {});
    std::optional<double> query_float(const std::string& sql, const std::vector<Value>& parameters = {});

    // Transaction control
    void begin_transaction();
    void commit();
    void rollback();
    bool in_transaction() const;

    // Dry runs - everything between begin_dry_run() and end_dry_run() happens inside a single
    // transaction that is always rolled back. While one is active, begin_transaction/commit/
    // rollback are absorbed (no-ops) so callers that manage their own transactions compose
    // instead of erroring on a nested BEGIN. A nested rollback is therefore not partial: the
    // dry run undoes everything at the end regardless. in_transaction() still reports true.
    void begin_dry_run();
    void end_dry_run();
    bool in_dry_run() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // Internal helper for executing raw SQL (for migrations)
    void execute_raw(const std::string& sql);

    // Internal method for parameterized queries
    Result execute(const std::string& sql, const std::vector<Value>& parameters = {});

    // Internal methods
    void set_version(int64_t version);
    void migrate_up(const std::string& migration_path);
    void apply_schema(const std::string& schema_path);
};

}  // namespace quiver

#endif  // QUIVER_DATABASE_H
