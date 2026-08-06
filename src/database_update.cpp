#include "database_impl.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace quiver {

void Database::update_element(const std::string& collection, int64_t id, const Element& element) {
    impl_->logger->debug("Updating element {} in collection: {}", id, collection);
    impl_->require_collection(collection, "update_element");

    const auto& scalars = element.scalars();
    const auto& arrays = element.arrays();

    if (scalars.empty() && arrays.empty()) {
        throw std::runtime_error("Cannot update_element: element must have at least one attribute to update");
    }

    impl_->require_element(collection, id, *this);

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

void Database::update_element(const std::string& collection, const std::string& label, const Element& element) {
    update_element(collection, impl_->resolve_label(collection, label, "update_element", *this), element);
}

void Database::update_relation(const std::string& collection_from,
                               const std::string& collection_to,
                               const std::string& relation_type,
                               int64_t id,
                               const std::optional<std::string>& target_label) {
    impl_->logger->debug("Updating relation {}.{} for id {}", collection_from, relation_type, id);
    impl_->require_collection(collection_from, "update_relation");

    std::string column = collection_to;
    std::transform(column.begin(), column.end(), column.begin(), [](unsigned char c) { return std::tolower(c); });
    column += "_" + relation_type;

    const auto* table = impl_->schema->get_table(collection_from);

    if (!table->has_column(column)) {
        throw std::runtime_error("Cannot update_relation: relation column '" + column + "' not found in collection '" +
                                 collection_from + "'");
    }

    const ForeignKey* relation = nullptr;
    for (const auto& fk : table->foreign_keys) {
        if (fk.from_column == column) {
            relation = &fk;
            break;
        }
    }
    if (relation == nullptr) {
        throw std::runtime_error("Cannot update_relation: relation column '" + column + "' in collection '" +
                                 collection_from + "' is not a foreign key");
    }
    if (relation->to_table != collection_to) {
        throw std::runtime_error("Cannot update_relation: relation column '" + column + "' in collection '" +
                                 collection_from + "' is a foreign key to collection '" + relation->to_table +
                                 "', not to '" + collection_to + "'");
    }

    Element element;
    if (target_label) {
        element.set(column, *target_label);
    } else {
        element.set_null(column);
    }
    update_element(collection_from, id, element);
}

void Database::update_relation(const std::string& collection_from,
                               const std::string& collection_to,
                               const std::string& relation_type,
                               const std::string& label,
                               const std::optional<std::string>& target_label) {
    update_relation(collection_from,
                    collection_to,
                    relation_type,
                    impl_->resolve_label(collection_from, label, "update_relation", *this),
                    target_label);
}

namespace {

// Transpose row-shaped group data into the column-shaped form insert_rows_into_group_table
// expects. `names` is the union of every row's keys (already validated), so a column that appears
// only in a later row is still written; a cell missing from a row becomes SQL NULL, which keeps
// every column the same length.
std::map<std::string, std::vector<Value>> transpose_group_rows(const std::vector<std::map<std::string, Value>>& rows,
                                                               const std::set<std::string>& names) {
    std::map<std::string, std::vector<Value>> columns;

    for (const auto& col_name : names) {
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

    const auto is_vector = type == GroupTableType::Vector;
    const auto table_name =
        is_vector ? Schema::vector_table_name(collection, group) : Schema::set_table_name(collection, group);
    const auto* table_def = schema->get_table(table_name);
    if (!table_def) {
        // Same wording as get_vector_metadata / get_set_metadata for the same condition (Pattern 2).
        throw std::runtime_error(std::string(is_vector ? "Vector" : "Set") + " group not found: '" + group +
                                 "' in collection '" + collection + "'");
    }
    require_element(collection, id, db);

    // Validate the union of every row's keys, not just rows[0]: a column named only in a later row
    // must still be written, and an unknown one must still be rejected (update_time_series_group
    // validates every row too). Validating here rather than over the transposed map is also what
    // catches a named-but-empty column list - {"typo": []} transposes to nothing, so the checks
    // below used to be skipped entirely while the DELETE still fired.
    std::set<std::string> names;
    for (const auto& row : rows) {
        for (const auto& [col_name, _] : row) {
            names.insert(col_name);
        }
    }
    for (const auto& col_name : names) {
        if (col_name == "id" || (is_vector && col_name == "vector_index")) {
            // Both are derived (the element id and the row's position), and emitting one here
            // would duplicate it in the INSERT column list - SQLite keeps the first occurrence,
            // so the caller's value would vanish silently.
            throw std::runtime_error(std::string("Cannot ") + caller + ": column '" + col_name +
                                     "' is managed by the group table, not a value column");
        }
        if (!table_def->has_column(col_name)) {
            throw std::runtime_error(std::string("Cannot ") + caller + ": column '" + col_name +
                                     "' not found in group '" + group + "' for collection '" + collection + "'");
        }
    }

    // Resolve FK labels before any writes, so a failed lookup cannot leave a cleared group.
    auto columns = transpose_group_rows(rows, names);
    for (auto& [col_name, values] : columns) {
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

void Database::update_vector_group(const std::string& collection,
                                   const std::string& group,
                                   const std::string& label,
                                   const std::vector<std::map<std::string, Value>>& rows) {
    update_vector_group(collection, group, impl_->resolve_label(collection, label, "update_vector_group", *this), rows);
}

void Database::update_set_group(const std::string& collection,
                                const std::string& group,
                                int64_t id,
                                const std::vector<std::map<std::string, Value>>& rows) {
    impl_->logger->debug("Updating set {}.{} for id {} with {} rows", collection, group, id, rows.size());
    impl_->update_group_rows("update_set_group", collection, group, GroupTableType::Set, id, rows, *this);
    impl_->logger->info("Updated set {}.{} for id {} with {} rows", collection, group, id, rows.size());
}

void Database::update_set_group(const std::string& collection,
                                const std::string& group,
                                const std::string& label,
                                const std::vector<std::map<std::string, Value>>& rows) {
    update_set_group(collection, group, impl_->resolve_label(collection, label, "update_set_group", *this), rows);
}

}  // namespace quiver
