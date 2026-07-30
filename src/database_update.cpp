#include "database_impl.h"

namespace quiver {

void Database::update_element(const std::string& collection, int64_t id, const Element& element) {
    impl_->logger->debug("Updating element {} in collection: {}", id, collection);
    impl_->require_collection(collection, "update_element");

    const auto& scalars = element.scalars();
    const auto& arrays = element.arrays();

    if (scalars.empty() && arrays.empty()) {
        throw std::runtime_error("Cannot update_element: element must have at least one attribute to update");
    }

    if (execute("SELECT 1 FROM " + collection + " WHERE id = ?", {id}).empty()) {
        throw std::runtime_error("Element not found: " + std::to_string(id) + " in collection '" + collection + "'");
    }

    // Pre-resolve pass: resolve all FK labels before any writes
    auto resolved = impl_->resolve_element_fk_labels(collection, element, *this);

    Impl::TransactionGuard txn(*impl_);

    // Update scalars if present
    if (!resolved.scalars.empty()) {
        // Validate scalar types
        for (const auto& [name, value] : resolved.scalars) {
            impl_->type_validator->validate_scalar("update_element", collection, name, value);
        }

        // Build UPDATE SQL
        auto sql = "UPDATE " + collection + " SET ";
        std::vector<Value> parameters;

        auto first = true;
        for (const auto& [name, value] : resolved.scalars) {
            if (!first) {
                sql += ", ";
            }
            sql += name + " = ?";
            parameters.push_back(value);
            first = false;
        }
        sql += " WHERE id = ?";
        parameters.emplace_back(id);

        execute(sql, parameters);
    }

    // Delegate group insertion to shared helper (delete_existing=true for updates)
    impl_->insert_group_data("update_element", collection, id, resolved.arrays, true, *this);

    txn.commit();
    impl_->logger->info("Updated element {} in {}", id, collection);
}

namespace {

// Transpose row-shaped group data into the column-shaped form insert_rows_into_group_table
// expects. The column set comes from the first row (mirroring update_time_series_group);
// a cell missing from a later row becomes SQL NULL so every column stays the same length.
std::map<std::string, std::vector<Value>> transpose_group_rows(const std::vector<std::map<std::string, Value>>& rows) {
    std::map<std::string, std::vector<Value>> columns;
    if (rows.empty()) {
        return columns;
    }

    for (const auto& [col_name, _] : rows[0]) {
        auto& values = columns[col_name];
        values.reserve(rows.size());
        for (const auto& row : rows) {
            auto it = row.find(col_name);
            values.push_back(it != row.end() ? it->second : Value{nullptr});
        }
    }
    return columns;
}

}  // namespace

void Database::Impl::update_group_rows(const char* caller,
                                       const std::string& collection,
                                       const std::string& group,
                                       GroupTableType type,
                                       int64_t id,
                                       const std::vector<std::map<std::string, Value>>& rows,
                                       Database& db) {
    require_collection(collection, caller);

    const auto table_name = type == GroupTableType::Vector ? Schema::vector_table_name(collection, group)
                                                           : Schema::set_table_name(collection, group);
    const auto* table_def = schema->get_table(table_name);
    if (!table_def) {
        throw std::runtime_error(std::string(group_table_noun(type)) + " group not found: '" + group +
                                 "' in collection '" + collection + "'");
    }

    // Resolve FK labels before any writes, so a failed lookup cannot leave a cleared group.
    auto columns = transpose_group_rows(rows);
    for (auto& [col_name, values] : columns) {
        if (!table_def->has_column(col_name)) {
            throw std::runtime_error(std::string("Cannot ") + caller + ": column '" + col_name +
                                     "' not found in group '" + group + "' for collection '" + collection + "'");
        }
        for (auto& value : values) {
            value = resolve_fk_label(*table_def, col_name, value, db);
        }
    }

    std::map<std::string, const std::vector<Value>*> column_ptrs;
    for (const auto& [col_name, values] : columns) {
        column_ptrs[col_name] = &values;
    }

    TransactionGuard txn(*this);
    insert_rows_into_group_table(caller, table_name, type, column_ptrs, id, true, db);
    txn.commit();
}

void Database::update_vector_group(const std::string& collection,
                                   const std::string& group,
                                   int64_t id,
                                   const std::vector<std::map<std::string, Value>>& rows) {
    impl_->logger->debug("Updating vector {}.{} for id {} with {} rows", collection, group, id, rows.size());
    impl_->update_group_rows("update_vector_group", collection, group, GroupTableType::Vector, id, rows, *this);
    impl_->logger->info("Updated vector {}.{} for id {} with {} rows", collection, group, id, rows.size());
}

void Database::update_set_group(const std::string& collection,
                                const std::string& group,
                                int64_t id,
                                const std::vector<std::map<std::string, Value>>& rows) {
    impl_->logger->debug("Updating set {}.{} for id {} with {} rows", collection, group, id, rows.size());
    impl_->update_group_rows("update_set_group", collection, group, GroupTableType::Set, id, rows, *this);
    impl_->logger->info("Updated set {}.{} for id {} with {} rows", collection, group, id, rows.size());
}

}  // namespace quiver
