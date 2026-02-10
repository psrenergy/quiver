#include "database_impl.h"
#include "database_internal.h"

namespace quiver {

ScalarMetadata Database::get_scalar_metadata(const std::string& collection, const std::string& attribute) const {
    if (!impl_->schema) {
        throw std::runtime_error("Cannot get scalar metadata: no schema loaded");
    }

    const auto* table_def = impl_->schema->get_table(collection);
    if (!table_def) {
        throw std::runtime_error("Collection not found in schema: " + collection);
    }

    const auto* col = table_def->get_column(attribute);
    if (!col) {
        throw std::runtime_error("Scalar attribute '" + attribute + "' not found in collection '" + collection + "'");
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
    if (!impl_->schema) {
        throw std::runtime_error("Cannot get vector metadata: no schema loaded");
    }

    // Find the vector table for this group
    auto vector_table = Schema::vector_table_name(collection, group_name);
    const auto* table_def = impl_->schema->get_table(vector_table);

    if (!table_def) {
        throw std::runtime_error("Vector group '" + group_name + "' not found for collection '" + collection + "'");
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
    if (!impl_->schema) {
        throw std::runtime_error("Cannot get set metadata: no schema loaded");
    }

    // Find the set table for this group
    auto set_table = Schema::set_table_name(collection, group_name);
    const auto* table_def = impl_->schema->get_table(set_table);

    if (!table_def) {
        throw std::runtime_error("Set group '" + group_name + "' not found for collection '" + collection + "'");
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
    if (!impl_->schema) {
        throw std::runtime_error("Cannot list scalar attributes: no schema loaded");
    }

    const auto* table_def = impl_->schema->get_table(collection);
    if (!table_def) {
        throw std::runtime_error("Collection not found in schema: " + collection);
    }

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
    if (!impl_->schema) {
        throw std::runtime_error("Cannot list vector groups: no schema loaded");
    }

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
    if (!impl_->schema) {
        throw std::runtime_error("Cannot list set groups: no schema loaded");
    }

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
