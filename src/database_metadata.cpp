#include "database_impl.h"
#include "database_internal.h"

namespace quiver {

ScalarMetadata Database::get_scalar_metadata(const std::string& collection, const std::string& attribute) const {
    impl_->require_collection(collection, "get_scalar_metadata");

    const auto* table_def = impl_->schema->get_table(collection);

    const auto* col = table_def->get_column(attribute);
    if (!col) {
        throw std::runtime_error("Scalar attribute not found: '" + attribute + "' in collection '" + collection + "'");
    }

    auto metadata = internal::scalar_metadata_from_column(*col);

    for (const auto& fk : table_def->foreign_keys) {
        if (fk.from_column == attribute) {
            metadata.is_foreign_key = true;
            metadata.references_collection = fk.to_table;
            metadata.references_column = fk.to_column;
            break;
        }
    }

    return metadata;
}

GroupMetadata Database::get_vector_metadata(const std::string& collection, const std::string& group_name) const {
    impl_->require_collection(collection, "get_vector_metadata");

    // Find the vector table for this group
    auto vector_table = Schema::vector_table_name(collection, group_name);
    const auto* table_def = impl_->schema->get_table(vector_table);

    if (!table_def) {
        throw std::runtime_error("Vector group not found: '" + group_name + "' in collection '" + collection + "'");
    }

    GroupMetadata metadata;
    metadata.group_name = group_name;

    // Add all data columns (skip id and vector_index)
    for (const auto& [col_name, col] : table_def->columns) {
        if (col_name == "id" || col_name == "vector_index") {
            continue;
        }

        metadata.value_columns.push_back(internal::scalar_metadata_from_column(col));
    }

    return metadata;
}

GroupMetadata Database::get_set_metadata(const std::string& collection, const std::string& group_name) const {
    impl_->require_collection(collection, "get_set_metadata");

    // Find the set table for this group
    auto set_table = Schema::set_table_name(collection, group_name);
    const auto* table_def = impl_->schema->get_table(set_table);

    if (!table_def) {
        throw std::runtime_error("Set group not found: '" + group_name + "' in collection '" + collection + "'");
    }

    GroupMetadata metadata;
    metadata.group_name = group_name;

    // Add all data columns (skip id)
    for (const auto& [col_name, col] : table_def->columns) {
        if (col_name == "id") {
            continue;
        }

        metadata.value_columns.push_back(internal::scalar_metadata_from_column(col));
    }

    return metadata;
}

std::vector<ScalarMetadata> Database::list_scalar_attributes(const std::string& collection) const {
    impl_->require_collection(collection, "list_scalar_attributes");

    const auto* table_def = impl_->schema->get_table(collection);

    std::vector<ScalarMetadata> result;
    for (const auto& [col_name, col] : table_def->columns) {
        auto metadata = internal::scalar_metadata_from_column(col);

        for (const auto& fk : table_def->foreign_keys) {
            if (fk.from_column == col_name) {
                metadata.is_foreign_key = true;
                metadata.references_collection = fk.to_table;
                metadata.references_column = fk.to_column;
                break;
            }
        }

        result.push_back(std::move(metadata));
    }
    return result;
}

std::vector<GroupMetadata> Database::list_vector_groups(const std::string& collection) const {
    impl_->require_schema("list_vector_groups");

    std::vector<GroupMetadata> result;
    auto prefix = collection + "_vector_";

    for (const auto& table_name : impl_->schema->table_names()) {
        if (!impl_->schema->is_vector_table(table_name))
            continue;
        if (impl_->schema->get_parent_collection(table_name) != collection)
            continue;

        // Extract group name from table name
        if (table_name.size() > prefix.size() && table_name.substr(0, prefix.size()) == prefix) {
            auto group_name = table_name.substr(prefix.size());
            result.push_back(get_vector_metadata(collection, group_name));
        }
    }
    return result;
}

std::vector<GroupMetadata> Database::list_set_groups(const std::string& collection) const {
    impl_->require_schema("list_set_groups");

    std::vector<GroupMetadata> result;
    auto prefix = collection + "_set_";

    for (const auto& table_name : impl_->schema->table_names()) {
        if (!impl_->schema->is_set_table(table_name))
            continue;
        if (impl_->schema->get_parent_collection(table_name) != collection)
            continue;

        // Extract group name from table name
        if (table_name.size() > prefix.size() && table_name.substr(0, prefix.size()) == prefix) {
            auto group_name = table_name.substr(prefix.size());
            result.push_back(get_set_metadata(collection, group_name));
        }
    }
    return result;
}

}  // namespace quiver
