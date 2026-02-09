#include "database_impl.h"

#include <map>

namespace quiver {

void Database::update_element(const std::string& collection, int64_t id, const Element& element) {
    impl_->logger->debug("Updating element {} in collection: {}", id, collection);
    impl_->require_collection(collection, "update element");

    const auto& scalars = element.scalars();
    const auto& arrays = element.arrays();

    if (scalars.empty() && arrays.empty()) {
        throw std::runtime_error("Element must have at least one attribute to update");
    }

    Impl::TransactionGuard txn(*impl_);

    // Update scalars if present
    if (!scalars.empty()) {
        // Validate scalar types
        for (const auto& [name, value] : scalars) {
            impl_->type_validator->validate_scalar(collection, name, value);
        }

        // Build UPDATE SQL
        auto sql = "UPDATE " + collection + " SET ";
        std::vector<Value> params;

        auto first = true;
        for (const auto& [name, value] : scalars) {
            if (!first) {
                sql += ", ";
            }
            sql += name + " = ?";
            params.push_back(value);
            first = false;
        }
        sql += " WHERE id = ?";
        params.push_back(id);

        execute(sql, params);
    }

    // Route arrays to their tables (grouped by table name)
    std::map<std::string, std::map<std::string, const std::vector<Value>*>> vector_table_columns;
    std::map<std::string, std::map<std::string, const std::vector<Value>*>> set_table_columns;
    std::map<std::string, std::map<std::string, const std::vector<Value>*>> time_series_table_columns;

    for (const auto& [attr_name, values] : arrays) {
        auto match = impl_->schema->find_table_for_column(collection, attr_name);
        if (!match) {
            throw std::runtime_error("Attribute '" + attr_name +
                                     "' is not a vector, set, or time series attribute in collection '" + collection +
                                     "'");
        }

        switch (match->type) {
        case GroupTableType::Vector:
            vector_table_columns[match->table_name][attr_name] = &values;
            break;
        case GroupTableType::Set:
            set_table_columns[match->table_name][attr_name] = &values;
            break;
        case GroupTableType::TimeSeries:
            time_series_table_columns[match->table_name][attr_name] = &values;
            break;
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
                throw std::runtime_error("Vector columns in table '" + table + "' must have the same length");
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
                throw std::runtime_error("Set columns in table '" + table + "' must have the same length");
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
                throw std::runtime_error("Time series columns in table '" + table + "' must have the same length");
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

void Database::update_scalar_integer(const std::string& collection,
                                     const std::string& attribute,
                                     int64_t id,
                                     int64_t value) {
    impl_->logger->debug("Updating {}.{} for id {} to {}", collection, attribute, id, value);
    impl_->require_collection(collection, "update scalar");
    impl_->type_validator->validate_scalar(collection, attribute, value);

    auto sql = "UPDATE " + collection + " SET " + attribute + " = ? WHERE id = ?";
    execute(sql, {value, id});

    impl_->logger->info("Updated {}.{} for id {} to {}", collection, attribute, id, value);
}

void Database::update_scalar_float(const std::string& collection,
                                   const std::string& attribute,
                                   int64_t id,
                                   double value) {
    impl_->logger->debug("Updating {}.{} for id {} to {}", collection, attribute, id, value);
    impl_->require_collection(collection, "update scalar");
    impl_->type_validator->validate_scalar(collection, attribute, value);

    auto sql = "UPDATE " + collection + " SET " + attribute + " = ? WHERE id = ?";
    execute(sql, {value, id});

    impl_->logger->info("Updated {}.{} for id {} to {}", collection, attribute, id, value);
}

void Database::update_scalar_string(const std::string& collection,
                                    const std::string& attribute,
                                    int64_t id,
                                    const std::string& value) {
    impl_->logger->debug("Updating {}.{} for id {} to '{}'", collection, attribute, id, value);
    impl_->require_collection(collection, "update scalar");
    impl_->type_validator->validate_scalar(collection, attribute, value);

    auto sql = "UPDATE " + collection + " SET " + attribute + " = ? WHERE id = ?";
    execute(sql, {value, id});

    impl_->logger->info("Updated {}.{} for id {} to '{}'", collection, attribute, id, value);
}

void Database::update_vector_integers(const std::string& collection,
                                      const std::string& attribute,
                                      int64_t id,
                                      const std::vector<int64_t>& values) {
    impl_->logger->debug("Updating vector {}.{} for id {} with {} values", collection, attribute, id, values.size());
    impl_->require_schema("update vector");

    auto vector_table = impl_->schema->find_vector_table(collection, attribute);

    Impl::TransactionGuard txn(*impl_);

    auto delete_sql = "DELETE FROM " + vector_table + " WHERE id = ?";
    execute(delete_sql, {id});

    for (size_t i = 0; i < values.size(); ++i) {
        auto insert_sql = "INSERT INTO " + vector_table + " (id, vector_index, " + attribute + ") VALUES (?, ?, ?)";
        int64_t vector_index = static_cast<int64_t>(i + 1);
        execute(insert_sql, {id, vector_index, values[i]});
    }

    txn.commit();
    impl_->logger->info("Updated vector {}.{} for id {} with {} values", collection, attribute, id, values.size());
}

void Database::update_vector_floats(const std::string& collection,
                                    const std::string& attribute,
                                    int64_t id,
                                    const std::vector<double>& values) {
    impl_->logger->debug("Updating vector {}.{} for id {} with {} values", collection, attribute, id, values.size());
    impl_->require_schema("update vector");

    auto vector_table = impl_->schema->find_vector_table(collection, attribute);

    Impl::TransactionGuard txn(*impl_);

    auto delete_sql = "DELETE FROM " + vector_table + " WHERE id = ?";
    execute(delete_sql, {id});

    for (size_t i = 0; i < values.size(); ++i) {
        auto insert_sql = "INSERT INTO " + vector_table + " (id, vector_index, " + attribute + ") VALUES (?, ?, ?)";
        int64_t vector_index = static_cast<int64_t>(i + 1);
        execute(insert_sql, {id, vector_index, values[i]});
    }

    txn.commit();
    impl_->logger->info("Updated vector {}.{} for id {} with {} values", collection, attribute, id, values.size());
}

void Database::update_vector_strings(const std::string& collection,
                                     const std::string& attribute,
                                     int64_t id,
                                     const std::vector<std::string>& values) {
    impl_->logger->debug("Updating vector {}.{} for id {} with {} values", collection, attribute, id, values.size());
    impl_->require_schema("update vector");

    auto vector_table = impl_->schema->find_vector_table(collection, attribute);

    Impl::TransactionGuard txn(*impl_);

    auto delete_sql = "DELETE FROM " + vector_table + " WHERE id = ?";
    execute(delete_sql, {id});

    for (size_t i = 0; i < values.size(); ++i) {
        auto insert_sql = "INSERT INTO " + vector_table + " (id, vector_index, " + attribute + ") VALUES (?, ?, ?)";
        int64_t vector_index = static_cast<int64_t>(i + 1);
        execute(insert_sql, {id, vector_index, values[i]});
    }

    txn.commit();
    impl_->logger->info("Updated vector {}.{} for id {} with {} values", collection, attribute, id, values.size());
}

void Database::update_set_integers(const std::string& collection,
                                   const std::string& attribute,
                                   int64_t id,
                                   const std::vector<int64_t>& values) {
    impl_->logger->debug("Updating set {}.{} for id {} with {} values", collection, attribute, id, values.size());
    impl_->require_schema("update set");

    auto set_table = impl_->schema->find_set_table(collection, attribute);

    Impl::TransactionGuard txn(*impl_);

    auto delete_sql = "DELETE FROM " + set_table + " WHERE id = ?";
    execute(delete_sql, {id});

    for (const auto& value : values) {
        auto insert_sql = "INSERT INTO " + set_table + " (id, " + attribute + ") VALUES (?, ?)";
        execute(insert_sql, {id, value});
    }

    txn.commit();
    impl_->logger->info("Updated set {}.{} for id {} with {} values", collection, attribute, id, values.size());
}

void Database::update_set_floats(const std::string& collection,
                                 const std::string& attribute,
                                 int64_t id,
                                 const std::vector<double>& values) {
    impl_->logger->debug("Updating set {}.{} for id {} with {} values", collection, attribute, id, values.size());
    impl_->require_schema("update set");

    auto set_table = impl_->schema->find_set_table(collection, attribute);

    Impl::TransactionGuard txn(*impl_);

    auto delete_sql = "DELETE FROM " + set_table + " WHERE id = ?";
    execute(delete_sql, {id});

    for (const auto& value : values) {
        auto insert_sql = "INSERT INTO " + set_table + " (id, " + attribute + ") VALUES (?, ?)";
        execute(insert_sql, {id, value});
    }

    txn.commit();
    impl_->logger->info("Updated set {}.{} for id {} with {} values", collection, attribute, id, values.size());
}

void Database::update_set_strings(const std::string& collection,
                                  const std::string& attribute,
                                  int64_t id,
                                  const std::vector<std::string>& values) {
    impl_->logger->debug("Updating set {}.{} for id {} with {} values", collection, attribute, id, values.size());
    impl_->require_schema("update set");

    auto set_table = impl_->schema->find_set_table(collection, attribute);

    Impl::TransactionGuard txn(*impl_);

    auto delete_sql = "DELETE FROM " + set_table + " WHERE id = ?";
    execute(delete_sql, {id});

    for (const auto& value : values) {
        auto insert_sql = "INSERT INTO " + set_table + " (id, " + attribute + ") VALUES (?, ?)";
        execute(insert_sql, {id, value});
    }

    txn.commit();
    impl_->logger->info("Updated set {}.{} for id {} with {} values", collection, attribute, id, values.size());
}

}  // namespace quiver
