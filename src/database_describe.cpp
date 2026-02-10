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

void Database::export_csv(const std::string& table,
                          const std::string& path,
                          const DateFormatMap& date_format_map,
                          const EnumMap& enum_map) {
    impl_->logger->debug("Exporting table {} to CSV at path '{}'", table, path);
    impl_->require_collection(table, "export to CSV");

    const auto* table_def = impl_->schema->get_table(table);

    // Build export columns (label first, skip id if table has label)
    bool has_label = table_def->has_column("label");
    std::vector<std::string> columns;
    if (has_label) {
        columns.push_back("label");
    }
    for (const auto& [col_name, col] : table_def->columns) {
        if (col_name == "id" && has_label)
            continue;
        if (col_name == "label")
            continue;
        columns.push_back(col_name);
    }

    // Build id->label lookup for FK columns whose target table has a label
    FkLabelMap fk_labels;
    for (const auto& fk : table_def->foreign_keys) {
        const auto* target = impl_->schema->get_table(fk.to_table);
        if (!target || !target->has_column("label"))
            continue;
        auto& lookup = fk_labels[fk.from_column];
        for (auto& row : execute("SELECT id, label FROM " + fk.to_table)) {
            lookup[*row.get_integer(0)] = *row.get_string(1);
        }
    }

    // Query all data
    std::string sql = "SELECT ";
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0)
            sql += ", ";
        sql += columns[i];
    }
    sql += " FROM " + table;
    auto data = execute(sql);

    quiver::write_csv(path, columns, data, fk_labels, date_format_map, enum_map);
}

void Database::import_csv(const std::string& table, const std::string& path) {
    impl_->logger->debug("Importing table {} from CSV at path '{}'", table, path);
    impl_->require_collection(table, "import from CSV");
    // TODO: implement
}

}  // namespace quiver
