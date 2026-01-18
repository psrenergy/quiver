#ifndef PSR_SCHEMA_H
#define PSR_SCHEMA_H

#include "data_type.h"
#include "export.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace psr {

struct ColumnDefinition {
    std::string name;
    DataType type;
    bool not_null;
    bool primary_key;
};

struct ForeignKey {
    std::string from_column;
    std::string to_table;
    std::string to_column;
    std::string on_update;
    std::string on_delete;
};

struct Index {
    std::string name;
    bool unique;
    std::vector<std::string> columns;
};

struct TableDefinition {
    std::string name;
    std::map<std::string, ColumnDefinition> columns;
    std::vector<ForeignKey> foreign_keys;
    std::vector<Index> indexes;

    std::optional<DataType> get_column_type(const std::string& column) const;
    bool has_column(const std::string& column) const;
    const ColumnDefinition* get_column(const std::string& column) const;
};

class PSR_API Schema {
public:
    // Factory: loads schema from database
    static Schema from_database(sqlite3* db);

    // Table lookup
    const TableDefinition* get_table(const std::string& name) const;
    bool has_table(const std::string& name) const;

    // Column type lookup (throws if table/column not found)
    DataType get_column_type(const std::string& table, const std::string& column) const;

    // Vector/Set table naming convention
    static std::string vector_table_name(const std::string& collection, const std::string& group);
    static std::string set_table_name(const std::string& collection, const std::string& group);

    // Table classification
    bool is_collection(const std::string& table) const;
    bool is_vector_table(const std::string& table) const;
    bool is_set_table(const std::string& table) const;
    bool is_time_series_table(const std::string& table) const;
    std::string get_parent_collection(const std::string& table) const;

    // Find table for attribute (throws if not found)
    std::string find_vector_table(const std::string& collection, const std::string& attribute) const;
    std::string find_set_table(const std::string& collection, const std::string& attribute) const;

    // All tables/collections
    std::vector<std::string> table_names() const;
    std::vector<std::string> collection_names() const;

private:
    Schema() = default;
    std::map<std::string, TableDefinition> tables_;

    void load_from_database(sqlite3* db);
    static std::vector<ColumnDefinition> query_columns(sqlite3* db, const std::string& table);
    static std::vector<ForeignKey> query_foreign_keys(sqlite3* db, const std::string& table);
    static std::vector<Index> query_indexes(sqlite3* db, const std::string& table);
};

}  // namespace psr

#endif  // PSR_SCHEMA_H
