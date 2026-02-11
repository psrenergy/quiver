#include "database_impl.h"

#include <map>

namespace quiver {

int64_t Database::create_element(const std::string& collection, const Element& element) {
    impl_->logger->debug("Creating element in collection: {}", collection);
    impl_->require_collection(collection, "create_element");

    const auto& scalars = element.scalars();
    if (scalars.empty()) {
        throw std::runtime_error("Cannot create_element: element must have at least one scalar attribute");
    }

    // Validate scalar types
    for (const auto& [name, value] : scalars) {
        impl_->type_validator->validate_scalar(collection, name, value);
    }

    Impl::TransactionGuard txn(*impl_);

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
    // Build a map of time series table -> (column_name -> array values)
    std::map<std::string, std::map<std::string, const std::vector<Value>*>> time_series_table_columns;

    for (const auto& [array_name, values] : arrays) {
        if (values.empty()) {
            throw std::runtime_error("Cannot create_element: empty array not allowed for '" + array_name + "'");
        }

        auto match = impl_->schema->find_table_for_column(collection, array_name);
        if (!match) {
            throw std::runtime_error("Cannot create_element: array '" + array_name +
                                     "' does not match any vector, set, or time series table in collection '" +
                                     collection + "'");
        }

        switch (match->type) {
        case GroupTableType::Vector:
            vector_table_columns[match->table_name][array_name] = &values;
            break;
        case GroupTableType::Set:
            set_table_columns[match->table_name][array_name] = &values;
            break;
        case GroupTableType::TimeSeries:
            time_series_table_columns[match->table_name][array_name] = &values;
            break;
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
                throw std::runtime_error("Cannot create_element: vector columns in table '" + vector_table +
                                         "' must have the same length");
            }
        }

        // Insert each row with vector_index
        for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
            auto vector_index = static_cast<int64_t>(row_idx + 1);
            auto vector_sql = "INSERT INTO " + vector_table + " (id, vector_index";
            std::string vec_placeholders = "?, ?";
            std::vector<Value> vec_params = {element_id, vector_index};

            for (const auto& [col_name, values_ptr] : columns) {
                vector_sql += ", " + col_name;
                vec_placeholders += ", ?";
                vec_params.push_back((*values_ptr)[row_idx]);
            }

            vector_sql += ") VALUES (" + vec_placeholders + ")";
            execute(vector_sql, vec_params);
        }
        impl_->logger->debug("Inserted {} vector rows into {}", num_rows, vector_table);
    }

    // Insert set table data - zip arrays together into rows
    for (const auto& [set_table, columns] : set_table_columns) {
        const auto* table_def = impl_->schema->get_table(set_table);
        if (!table_def) {
            throw std::runtime_error("Set table not found: " + set_table);
        }

        // Verify all arrays have same length and valid types
        size_t num_rows = 0;
        for (const auto& [col_name, values_ptr] : columns) {
            impl_->type_validator->validate_array(set_table, col_name, *values_ptr);
            if (num_rows == 0) {
                num_rows = values_ptr->size();
            } else if (values_ptr->size() != num_rows) {
                throw std::runtime_error("Cannot create_element: set columns in table '" + set_table +
                                         "' must have the same length");
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

                auto val = (*values_ptr)[row_idx];

                // Check if this column is a FK and value is a string (label) that needs resolution
                for (const auto& fk : table_def->foreign_keys) {
                    if (fk.from_column == col_name && std::holds_alternative<std::string>(val)) {
                        const std::string& label = std::get<std::string>(val);
                        // Look up the ID by label using direct SQL query
                        auto lookup_sql = "SELECT id FROM " + fk.to_table + " WHERE label = ?";
                        auto lookup_result = execute(lookup_sql, {label});
                        if (lookup_result.empty() || !lookup_result[0].get_integer(0)) {
                            throw std::runtime_error("Failed to resolve label '" + label + "' to ID in table '" +
                                                     fk.to_table + "'");
                        }
                        val = lookup_result[0].get_integer(0).value(); // NOLINT(bugprone-unchecked-optional-access) checked on line 156
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

    // Insert time series table data - zip arrays together into rows (no vector_index)
    for (const auto& [ts_table, columns] : time_series_table_columns) {
        const auto* table_def = impl_->schema->get_table(ts_table);
        if (!table_def) {
            throw std::runtime_error("Time series table not found: " + ts_table);
        }

        // Verify all arrays have same length and valid types
        size_t num_rows = 0;
        for (const auto& [col_name, values_ptr] : columns) {
            impl_->type_validator->validate_array(ts_table, col_name, *values_ptr);
            if (num_rows == 0) {
                num_rows = values_ptr->size();
            } else if (values_ptr->size() != num_rows) {
                throw std::runtime_error("Cannot create_element: time series columns in table '" + ts_table +
                                         "' must have the same length");
            }
        }

        // Insert each row
        for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
            auto ts_sql = "INSERT INTO " + ts_table + " (id";
            std::string ts_placeholders = "?";
            std::vector<Value> ts_params = {element_id};

            for (const auto& [col_name, values_ptr] : columns) {
                ts_sql += ", " + col_name;
                ts_placeholders += ", ?";
                ts_params.push_back((*values_ptr)[row_idx]);
            }

            ts_sql += ") VALUES (" + ts_placeholders + ")";
            execute(ts_sql, ts_params);
        }
        impl_->logger->debug("Inserted {} time series rows into {}", num_rows, ts_table);
    }

    txn.commit();
    impl_->logger->info("Created element {} in {}", element_id, collection);
    return element_id;
}

}  // namespace quiver
