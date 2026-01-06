#ifndef PSR_SCHEMA_VALIDATOR_H
#define PSR_SCHEMA_VALIDATOR_H

#include "export.h"

#include <string>
#include <vector>

struct sqlite3;

namespace psr {

struct ColumnInfo {
    std::string name;
    std::string type;
    bool not_null;
    bool primary_key;
};

struct ForeignKeyInfo {
    std::string from_column;
    std::string to_table;
    std::string to_column;
    std::string on_update;
    std::string on_delete;
};

struct IndexInfo {
    std::string name;
    bool unique;
    std::vector<std::string> columns;
};

class PSR_API SchemaValidator {
public:
    explicit SchemaValidator(sqlite3* db);

    void validate();

private:
    sqlite3* db_;
    std::vector<std::string> tables_;
    std::vector<std::string> collections_;

    // Table introspection
    std::vector<std::string> get_table_names();
    std::vector<ColumnInfo> get_columns(const std::string& table);
    std::vector<ForeignKeyInfo> get_foreign_keys(const std::string& table);
    std::vector<IndexInfo> get_indexes(const std::string& table);

    // Table classification
    bool is_collection(const std::string& name) const;
    bool is_vector_table(const std::string& name) const;
    bool is_set_table(const std::string& name) const;
    bool is_time_series_table(const std::string& name) const;
    std::string get_parent_collection(const std::string& table) const;

    // Individual validations
    void validate_configuration_exists();
    void validate_collection_names();
    void validate_collection(const std::string& name);
    void validate_vector_table(const std::string& name);
    void validate_set_table(const std::string& name);
    void validate_no_duplicate_attributes();
    void validate_foreign_keys();

    // Helper
    void validation_error(const std::string& message);
};

}  // namespace psr

#endif  // PSR_SCHEMA_VALIDATOR_H
