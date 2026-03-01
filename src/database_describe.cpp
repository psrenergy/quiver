#include "database_impl.h"

#include <iostream>
#include <utility>
#include <vector>

namespace quiver {

void Database::describe(std::ostream& out) const {
    out << "Database: " << impl_->path << "\n";
    out << "Version: " << current_version() << "\n";

    if (!impl_->schema) {
        out << "\nNo schema loaded.\n";
        return;
    }

    for (const auto& collection : impl_->schema->collection_names()) {
        out << "\nCollection: " << collection << "\n";

        // Scalars - use column_order for schema-definition order
        const auto* table_def = impl_->schema->get_table(collection);
        if (table_def && !table_def->column_order.empty()) {
            out << "  Scalars:\n";
            for (const auto& name : table_def->column_order) {
                const auto& col = table_def->columns.at(name);
                out << "    - " << name << " (" << data_type_to_string(col.type) << ")";
                if (col.primary_key) {
                    out << " PRIMARY KEY";
                }
                if (col.not_null && !col.primary_key) {
                    out << " NOT NULL";
                }
                out << "\n";
            }
        }

        // Vectors - collect all matching groups, then print header once
        auto prefix_vec = collection + "_vector_";
        std::vector<std::pair<std::string, const TableDefinition*>> vector_groups;
        for (const auto& table_name : impl_->schema->table_names()) {
            if (!impl_->schema->is_vector_table(table_name))
                continue;
            if (impl_->schema->get_parent_collection(table_name) != collection)
                continue;
            auto group_name = table_name.substr(prefix_vec.size());
            const auto* vec_table = impl_->schema->get_table(table_name);
            if (vec_table)
                vector_groups.emplace_back(group_name, vec_table);
        }
        if (!vector_groups.empty()) {
            out << "  Vectors:\n";
            for (const auto& [group_name, vec_table] : vector_groups) {
                out << "    - " << group_name << ": ";
                bool first = true;
                for (const auto& col_name : vec_table->column_order) {
                    if (col_name == "id" || col_name == "vector_index")
                        continue;
                    const auto& col = vec_table->columns.at(col_name);
                    if (!first)
                        out << ", ";
                    out << col_name << "(" << data_type_to_string(col.type) << ")";
                    first = false;
                }
                out << "\n";
            }
        }

        // Sets - collect all matching groups, then print header once
        auto prefix_set = collection + "_set_";
        std::vector<std::pair<std::string, const TableDefinition*>> set_groups;
        for (const auto& table_name : impl_->schema->table_names()) {
            if (!impl_->schema->is_set_table(table_name))
                continue;
            if (impl_->schema->get_parent_collection(table_name) != collection)
                continue;
            auto group_name = table_name.substr(prefix_set.size());
            const auto* set_table = impl_->schema->get_table(table_name);
            if (set_table)
                set_groups.emplace_back(group_name, set_table);
        }
        if (!set_groups.empty()) {
            out << "  Sets:\n";
            for (const auto& [group_name, set_table] : set_groups) {
                out << "    - " << group_name << ": ";
                bool first = true;
                for (const auto& col_name : set_table->column_order) {
                    if (col_name == "id")
                        continue;
                    const auto& col = set_table->columns.at(col_name);
                    if (!first)
                        out << ", ";
                    out << col_name << "(" << data_type_to_string(col.type) << ")";
                    first = false;
                }
                out << "\n";
            }
        }

        // Time Series - collect all matching groups, then print header once
        auto prefix_ts = collection + "_time_series_";
        std::vector<std::pair<std::string, const TableDefinition*>> ts_groups;
        for (const auto& table_name : impl_->schema->table_names()) {
            if (!impl_->schema->is_time_series_table(table_name))
                continue;
            if (impl_->schema->get_parent_collection(table_name) != collection)
                continue;
            auto group_name = table_name.substr(prefix_ts.size());
            const auto* ts_table = impl_->schema->get_table(table_name);
            if (ts_table)
                ts_groups.emplace_back(group_name, ts_table);
        }
        if (!ts_groups.empty()) {
            out << "  Time Series:\n";
            for (const auto& [group_name, ts_table] : ts_groups) {
                out << "    - " << group_name << ": ";
                bool first = true;
                for (const auto& col_name : ts_table->column_order) {
                    if (col_name == "id")
                        continue;
                    const auto& col = ts_table->columns.at(col_name);
                    if (!first)
                        out << ", ";
                    bool is_dimension = col_name.starts_with("date_");
                    if (is_dimension) {
                        out << "[" << col_name << "]";
                    } else {
                        out << col_name << "(" << data_type_to_string(col.type) << ")";
                    }
                    first = false;
                }
                out << "\n";
            }
        }
    }
}

}  // namespace quiver
