#include "database_impl.h"

#include <iostream>
#include <utility>
#include <vector>

namespace quiver {

namespace {

// Print a group's value columns in declaration order; time series dimension
// columns are bracketed, vector tables hide their structural vector_index.
void print_group_columns(std::ostream& out, const TableDefinition& table, GroupTableType type) {
    bool first = true;
    for (const auto& col_name : table.column_order) {
        if (col_name == "id" || (type == GroupTableType::Vector && col_name == "vector_index"))
            continue;
        const auto& col = table.columns.at(col_name);
        if (!first)
            out << ", ";
        if (type == GroupTableType::TimeSeries && is_date_time_column(col_name)) {
            out << "[" << col_name << "]";
        } else {
            out << col_name << "(" << data_type_to_string(col.type) << ")";
        }
        first = false;
    }
    out << "\n";
}

std::string group_table_name(const std::string& collection, const std::string& group, GroupTableType type) {
    switch (type) {
    case GroupTableType::Vector:
        return Schema::vector_table_name(collection, group);
    case GroupTableType::Set:
        return Schema::set_table_name(collection, group);
    case GroupTableType::TimeSeries:
        return Schema::time_series_table_name(collection, group);
    default:
        return "";
    }
}

}  // namespace

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

        // Group sections - one block per group table type
        const std::pair<const char*, GroupTableType> sections[] = {
            {"  Vectors:", GroupTableType::Vector},
            {"  Sets:", GroupTableType::Set},
            {"  Time Series:", GroupTableType::TimeSeries},
        };
        for (const auto& [header, type] : sections) {
            auto groups = impl_->schema->group_names(collection, type);
            if (groups.empty())
                continue;
            out << header << "\n";
            for (const auto& group_name : groups) {
                const auto* table = impl_->schema->get_table(group_table_name(collection, group_name, type));
                out << "    - " << group_name << ": ";
                print_group_columns(out, *table, type);
            }
        }
    }
}

}  // namespace quiver
