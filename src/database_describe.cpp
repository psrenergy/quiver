#include "database_impl.h"

#include <iostream>

namespace quiver {

void Database::describe() const {
    std::cout << "Database: " << impl_->path << "\n";
    std::cout << "Version: " << current_version() << "\n";

    if (!impl_->schema) {
        std::cout << "\nNo schema loaded.\n";
        return;
    }

    for (const auto& collection : impl_->schema->collection_names()) {
        std::cout << "\nCollection: " << collection << "\n";

        // Scalars
        const auto* table_def = impl_->schema->get_table(collection);
        if (table_def && !table_def->columns.empty()) {
            std::cout << "  Scalars:\n";
            for (const auto& [name, col] : table_def->columns) {
                std::cout << "    - " << name << " (" << data_type_to_string(col.type) << ")";
                if (col.primary_key) {
                    std::cout << " PRIMARY KEY";
                }
                if (col.not_null && !col.primary_key) {
                    std::cout << " NOT NULL";
                }
                std::cout << "\n";
            }
        }

        // Vectors
        auto prefix_vec = collection + "_vector_";
        for (const auto& table_name : impl_->schema->table_names()) {
            if (!impl_->schema->is_vector_table(table_name))
                continue;
            if (impl_->schema->get_parent_collection(table_name) != collection)
                continue;

            auto group_name = table_name.substr(prefix_vec.size());
            const auto* vec_table = impl_->schema->get_table(table_name);
            if (!vec_table)
                continue;

            std::cout << "  Vectors:\n";
            std::cout << "    - " << group_name << ": ";
            bool first = true;
            for (const auto& [col_name, col] : vec_table->columns) {
                if (col_name == "id" || col_name == "vector_index")
                    continue;
                if (!first)
                    std::cout << ", ";
                std::cout << col_name << "(" << data_type_to_string(col.type) << ")";
                first = false;
            }
            std::cout << "\n";
        }

        // Sets
        auto prefix_set = collection + "_set_";
        for (const auto& table_name : impl_->schema->table_names()) {
            if (!impl_->schema->is_set_table(table_name))
                continue;
            if (impl_->schema->get_parent_collection(table_name) != collection)
                continue;

            auto group_name = table_name.substr(prefix_set.size());
            const auto* set_table = impl_->schema->get_table(table_name);
            if (!set_table)
                continue;

            std::cout << "  Sets:\n";
            std::cout << "    - " << group_name << ": ";
            bool first = true;
            for (const auto& [col_name, col] : set_table->columns) {
                if (col_name == "id")
                    continue;
                if (!first)
                    std::cout << ", ";
                std::cout << col_name << "(" << data_type_to_string(col.type) << ")";
                first = false;
            }
            std::cout << "\n";
        }
    }
}

void Database::export_csv(const std::string& table, const std::string& path) {}

void Database::import_csv(const std::string& table, const std::string& path) {}

}  // namespace quiver
