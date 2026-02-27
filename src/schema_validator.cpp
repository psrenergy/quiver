#include "quiver/schema_validator.h"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace quiver {

SchemaValidator::SchemaValidator(const Schema& schema) : schema_(schema) {}

void SchemaValidator::validation_error(const std::string& message) {
    throw std::runtime_error("Schema validation error: " + message);
}

void SchemaValidator::validate() {
    // Identify collections
    collections_.clear();
    for (const auto& name : schema_.table_names()) {
        if (schema_.is_collection(name)) {
            collections_.push_back(name);
        }
    }

    // Run all validations
    validate_configuration_exists();
    validate_collection_names();

    for (const auto& name : schema_.table_names()) {
        if (schema_.is_collection(name)) {
            validate_collection(name);
        } else if (schema_.is_vector_table(name)) {
            validate_vector_table(name);
        } else if (schema_.is_set_table(name)) {
            validate_set_table(name);
        } else if (schema_.is_time_series_files_table(name)) {
            validate_time_series_files_table(name);
        }
        // Time series tables have minimal validation (just file paths)
    }

    validate_no_duplicate_attributes();
    validate_foreign_keys();
}

void SchemaValidator::validate_configuration_exists() {
    if (!schema_.has_table("Configuration")) {
        validation_error("Schema must have a 'Configuration' table");
    }
}

void SchemaValidator::validate_collection_names() {
    for (const auto& name : schema_.table_names()) {
        // Skip special tables
        if (schema_.is_vector_table(name) || schema_.is_set_table(name) || schema_.is_time_series_table(name) ||
            schema_.is_time_series_files_table(name)) {
            continue;
        }

        // Check for invalid collection names (underscores not allowed except for special tables)
        if (name.find('_') != std::string::npos && name != "Configuration") {
            validation_error("Invalid collection name '" + name + "': collection names cannot contain underscores");
        }
    }
}

void SchemaValidator::validate_collection(const std::string& name) {
    const auto* table = schema_.get_table(name);
    if (!table) {
        return;
    }

    // Must have 'id' column as primary key
    const auto* id_column = table->get_column("id");
    if (!id_column || !id_column->primary_key) {
        validation_error("Collection '" + name + "' must have 'id' as primary key");
    }

    // Must have 'label' column with TEXT type and NOT NULL constraint
    const auto* label_column = table->get_column("label");
    if (!label_column) {
        validation_error("Collection '" + name + "' must have a 'label' column");
    }
    if (label_column->type != DataType::Text) {
        validation_error("Collection '" + name + "' label column must be TEXT type");
    }
    if (!label_column->not_null) {
        validation_error("Collection '" + name + "' label column must have NOT NULL constraint");
    }

    // Check for UNIQUE constraint on label
    auto label_unique = false;
    for (const auto& idx : table->indexes) {
        if (idx.unique && idx.columns.size() == 1 && idx.columns[0] == "label") {
            label_unique = true;
            break;
        }
    }
    if (!label_unique) {
        validation_error("Collection '" + name + "' label column must have UNIQUE constraint");
    }
}

void SchemaValidator::validate_vector_table(const std::string& name) {
    const auto* table = schema_.get_table(name);
    if (!table) {
        return;
    }

    // Get parent collection
    auto parent = schema_.get_parent_collection(name);
    if (std::find(collections_.begin(), collections_.end(), parent) == collections_.end()) {
        validation_error("Vector table '" + name + "' references non-existent collection '" + parent + "'");
    }

    // Must have 'id' column
    const auto* id_col = table->get_column("id");
    if (!id_col) {
        validation_error("Vector table '" + name + "' must have 'id' column");
    }

    // id should NOT be primary key alone (composite PK required)
    if (id_col && id_col->primary_key) {
        auto pk_count = 0;
        for (const auto& [_, col] : table->columns) {
            if (col.primary_key) {
                pk_count++;
            }
        }
        if (pk_count == 1) {
            validation_error("Vector table '" + name +
                             "' must have composite primary key (id, vector_index), not just 'id'");
        }
    }

    // Must have 'vector_index' column
    if (!table->has_column("vector_index")) {
        validation_error("Vector table '" + name + "' must have 'vector_index' column");
    }

    // Must have FK to parent with ON DELETE CASCADE ON UPDATE CASCADE
    auto has_parent_fk = false;
    for (const auto& fk : table->foreign_keys) {
        if (fk.from_column == "id" && fk.to_table == parent) {
            has_parent_fk = true;
            if (fk.on_delete != "CASCADE" || fk.on_update != "CASCADE") {
                validation_error("Vector table '" + name +
                                 "' FK to parent must use ON DELETE CASCADE ON UPDATE CASCADE");
            }
            break;
        }
    }
    if (!has_parent_fk) {
        validation_error("Vector table '" + name + "' must have foreign key to parent collection '" + parent + "'");
    }
}

void SchemaValidator::validate_set_table(const std::string& name) {
    const auto* table = schema_.get_table(name);
    if (!table) {
        return;
    }

    // Get FK column names
    std::set<std::string> fk_columns;
    for (const auto& fk : table->foreign_keys) {
        fk_columns.insert(fk.from_column);
    }

    // Must have at least one UNIQUE constraint
    auto has_unique = false;
    for (const auto& idx : table->indexes) {
        if (idx.unique) {
            has_unique = true;
            break;
        }
    }
    if (!has_unique) {
        validation_error("Set table '" + name + "' must have a UNIQUE constraint");
    }

    // Check that all non-id, non-FK columns are covered by unique constraints
    std::set<std::string> unique_columns;
    for (const auto& idx : table->indexes) {
        if (idx.unique) {
            for (const auto& col : idx.columns) {
                unique_columns.insert(col);
            }
        }
    }

    for (const auto& [col_name, _] : table->columns) {
        // Skip id and FK columns
        if (col_name == "id" || fk_columns.count(col_name) > 0) {
            continue;
        }
        // Non-FK columns should be in unique constraint
        if (unique_columns.find(col_name) == unique_columns.end()) {
            validation_error("Set table '" + name + "' column '" + col_name + "' must be part of a UNIQUE constraint");
        }
    }
}

void SchemaValidator::validate_time_series_files_table(const std::string& name) {
    const auto* table = schema_.get_table(name);
    if (!table) {
        return;
    }

    // Get parent collection
    auto parent = schema_.get_time_series_files_parent_collection(name);
    if (std::find(collections_.begin(), collections_.end(), parent) == collections_.end()) {
        validation_error("Time series files table '" + name + "' references non-existent collection '" + parent + "'");
    }

    // All columns should be TEXT type (for file paths)
    for (const auto& [col_name, col] : table->columns) {
        if (col.type != DataType::Text) {
            validation_error("Time series files table '" + name + "' column '" + col_name +
                             "' must be TEXT type (for file paths)");
        }
    }
}

void SchemaValidator::validate_no_duplicate_attributes() {
    // For each collection, gather all attribute names from scalar columns and all group tables
    // (vector, set, time series) and reject any duplicates.
    for (const auto& collection : collections_) {
        if (collection == "Configuration") {
            continue;
        }

        const auto* col_table = schema_.get_table(collection);
        if (!col_table) {
            continue;
        }

        std::set<std::string> attributes;

        // Add scalar attributes (excluding id and label)
        for (const auto& [col_name, _] : col_table->columns) {
            if (col_name != "id" && col_name != "label") {
                attributes.insert(col_name);
            }
        }

        // Check all group tables (vector, set, time series) for duplicates
        for (const auto& table_name : schema_.table_names()) {
            const auto is_vector = schema_.is_vector_table(table_name);
            const auto is_set = schema_.is_set_table(table_name);
            const auto is_ts = schema_.is_time_series_table(table_name);

            if (!is_vector && !is_set && !is_ts) {
                continue;
            }
            if (schema_.get_parent_collection(table_name) != collection) {
                continue;
            }

            const auto* group_table = schema_.get_table(table_name);
            if (!group_table) {
                continue;
            }

            // Get FK column names
            std::set<std::string> fk_cols;
            for (const auto& fk : group_table->foreign_keys) {
                fk_cols.insert(fk.from_column);
            }

            for (const auto& [col_name, _] : group_table->columns) {
                // Skip structural columns common to all group types
                if (col_name == "id" || fk_cols.count(col_name) > 0) {
                    continue;
                }

                // Skip type-specific structural columns
                if (is_vector && col_name == "vector_index") {
                    continue;
                }
                if (is_ts && col_name.starts_with("date_")) {
                    continue;
                }

                if (attributes.count(col_name) > 0) {
                    validation_error("Duplicate attribute '" + col_name + "' found in table '" + table_name +
                                     "' (already defined in collection '" + collection + "')");
                }
                attributes.insert(col_name);
            }
        }
    }
}

void SchemaValidator::validate_foreign_keys() {
    for (const auto& table_name : schema_.table_names()) {
        const auto* table = schema_.get_table(table_name);
        if (!table) {
            continue;
        }

        for (const auto& fk : table->foreign_keys) {
            // Find the column for this FK
            const auto* col = table->get_column(fk.from_column);
            if (!col) {
                continue;
            }

            // Rule: FK columns with ON DELETE SET NULL cannot have NOT NULL constraint
            if (fk.on_delete == "SET NULL" && col->not_null) {
                validation_error("Foreign key column '" + fk.from_column + "' in table '" + table_name +
                                 "' has ON DELETE SET NULL but NOT NULL constraint");
            }

            // Rule: Valid FK action combinations
            // For vector table parent FK: ON DELETE CASCADE ON UPDATE CASCADE
            // For relation FKs: ON DELETE SET NULL ON UPDATE CASCADE
            if (schema_.is_vector_table(table_name) && fk.from_column == "id") {
                // Parent FK must be CASCADE/CASCADE
                if (fk.on_delete != "CASCADE" || fk.on_update != "CASCADE") {
                    validation_error("Vector table '" + table_name +
                                     "' parent FK must use ON DELETE CASCADE ON UPDATE CASCADE");
                }
            } else if (!schema_.is_time_series_table(table_name)) {
                // Relation FK - check for valid combinations
                // Must be ON UPDATE CASCADE
                if (fk.on_update != "CASCADE") {
                    validation_error("Foreign key '" + fk.from_column + "' in table '" + table_name +
                                     "' must use ON UPDATE CASCADE");
                }
                // ON DELETE must be SET NULL or CASCADE
                if (fk.on_delete != "SET NULL" && fk.on_delete != "CASCADE") {
                    validation_error("Foreign key '" + fk.from_column + "' in table '" + table_name +
                                     "' must use ON DELETE SET NULL or ON DELETE CASCADE");
                }
            }

            // Rule: FK column names should follow pattern <collection>_id or <collection>_<relation>
            // Skip vector table id column and set table columns
            if (!schema_.is_vector_table(table_name) || fk.from_column != "id") {
                if (!schema_.is_set_table(table_name) && !schema_.is_time_series_table(table_name)) {
                    // Check if FK column name ends with _id or matches target_relation pattern
                    auto target = fk.to_table;
                    auto target_lower = target;
                    std::transform(target_lower.begin(), target_lower.end(), target_lower.begin(), ::tolower);
                    auto col_lower = fk.from_column;
                    std::transform(col_lower.begin(), col_lower.end(), col_lower.begin(), ::tolower);

                    // Expected pattern: <target>_id or <target>_<something>
                    auto valid_name = (col_lower == target_lower + "_id") ||
                                      (col_lower.starts_with(target_lower + "_")) ||
                                      (col_lower.find("_id") != std::string::npos);

                    if (!valid_name) {
                        validation_error("Foreign key column '" + fk.from_column + "' in table '" + table_name +
                                         "' should follow naming pattern '<collection>_id'");
                    }
                }
            }
        }
    }
}

}  // namespace quiver
