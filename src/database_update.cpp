#include "database_impl.h"

#include <map>

namespace quiver {

void Database::update_element(const std::string& collection, int64_t id, const Element& element) {
    impl_->logger->debug("Updating element {} in collection: {}", id, collection);
    impl_->require_collection(collection, "update_element");

    const auto& scalars = element.scalars();
    const auto& arrays = element.arrays();

    if (scalars.empty() && arrays.empty()) {
        throw std::runtime_error("Cannot update_element: element must have at least one attribute to update");
    }

    // Pre-resolve pass: resolve all FK labels before any writes
    auto resolved = impl_->resolve_element_fk_labels(collection, element, *this);

    Impl::TransactionGuard txn(*impl_);

    // Update scalars if present
    if (!resolved.scalars.empty()) {
        // Validate scalar types
        for (const auto& [name, value] : resolved.scalars) {
            impl_->type_validator->validate_scalar(collection, name, value);
        }

        // Build UPDATE SQL
        auto sql = "UPDATE " + collection + " SET ";
        std::vector<Value> params;

        auto first = true;
        for (const auto& [name, value] : resolved.scalars) {
            if (!first) {
                sql += ", ";
            }
            sql += name + " = ?";
            params.push_back(value);
            first = false;
        }
        sql += " WHERE id = ?";
        params.emplace_back(id);

        execute(sql, params);
    }

    // Route arrays to their tables (grouped by table name)
    std::map<std::string, std::map<std::string, const std::vector<Value>*>> vector_table_columns;
    std::map<std::string, std::map<std::string, const std::vector<Value>*>> set_table_columns;
    std::map<std::string, std::map<std::string, const std::vector<Value>*>> time_series_table_columns;

    for (const auto& [attr_name, values] : resolved.arrays) {
        auto matches = impl_->schema->find_all_tables_for_column(collection, attr_name);
        if (matches.empty()) {
            throw std::runtime_error("Cannot update_element: attribute '" + attr_name +
                                     "' is not a vector, set, or time series attribute in collection '" + collection +
                                     "'");
        }

        for (const auto& match : matches) {
            switch (match.type) {
            case GroupTableType::Vector:
                vector_table_columns[match.table_name][attr_name] = &values;
                break;
            case GroupTableType::Set:
                set_table_columns[match.table_name][attr_name] = &values;
                break;
            case GroupTableType::TimeSeries:
                time_series_table_columns[match.table_name][attr_name] = &values;
                break;
            default:
                throw std::runtime_error("Cannot update_element: unknown group table type " +
                                         std::to_string(static_cast<int>(match.type)));
            }
        }
    }

    // Update vector tables
    for (const auto& [table, columns] : vector_table_columns) {
        execute("DELETE FROM " + table + " WHERE id = ?", {id});

        size_t num_rows = 0;
        for (const auto& [col_name, values_ptr] : columns) {
            if (num_rows == 0) {
                num_rows = values_ptr->size();
            } else if (values_ptr->size() != num_rows) {
                throw std::runtime_error("Cannot update_element: vector columns in table '" + table +
                                         "' must have the same length");
            }
        }

        for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
            auto vector_index = static_cast<int64_t>(row_idx + 1);
            auto sql = "INSERT INTO " + table + " (id, vector_index";
            std::string placeholders = "?, ?";
            std::vector<Value> params = {id, vector_index};
            for (const auto& [col_name, values_ptr] : columns) {
                sql += ", " + col_name;
                placeholders += ", ?";
                params.push_back((*values_ptr)[row_idx]);
            }
            sql += ") VALUES (" + placeholders + ")";
            execute(sql, params);
        }
        impl_->logger->debug("Updated vector table {} for id {} with {} rows", table, id, num_rows);
    }

    // Update set tables
    for (const auto& [table, columns] : set_table_columns) {
        execute("DELETE FROM " + table + " WHERE id = ?", {id});

        size_t num_rows = 0;
        for (const auto& [col_name, values_ptr] : columns) {
            if (num_rows == 0) {
                num_rows = values_ptr->size();
            } else if (values_ptr->size() != num_rows) {
                throw std::runtime_error("Cannot update_element: set columns in table '" + table +
                                         "' must have the same length");
            }
        }

        for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
            auto sql = "INSERT INTO " + table + " (id";
            std::string placeholders = "?";
            std::vector<Value> params = {id};
            for (const auto& [col_name, values_ptr] : columns) {
                sql += ", " + col_name;
                placeholders += ", ?";
                params.push_back((*values_ptr)[row_idx]);
            }
            sql += ") VALUES (" + placeholders + ")";
            execute(sql, params);
        }
        impl_->logger->debug("Updated set table {} for id {} with {} rows", table, id, num_rows);
    }

    // Update time series tables
    for (const auto& [table, columns] : time_series_table_columns) {
        execute("DELETE FROM " + table + " WHERE id = ?", {id});

        size_t num_rows = 0;
        for (const auto& [col_name, values_ptr] : columns) {
            if (num_rows == 0) {
                num_rows = values_ptr->size();
            } else if (values_ptr->size() != num_rows) {
                throw std::runtime_error("Cannot update_element: time series columns in table '" + table +
                                         "' must have the same length");
            }
        }

        for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
            auto sql = "INSERT INTO " + table + " (id";
            std::string placeholders = "?";
            std::vector<Value> params = {id};
            for (const auto& [col_name, values_ptr] : columns) {
                sql += ", " + col_name;
                placeholders += ", ?";
                params.push_back((*values_ptr)[row_idx]);
            }
            sql += ") VALUES (" + placeholders + ")";
            execute(sql, params);
        }
        impl_->logger->debug("Updated time series table {} for id {} with {} rows", table, id, num_rows);
    }

    txn.commit();
    impl_->logger->info("Updated element {} in {}", id, collection);
}

}  // namespace quiver
