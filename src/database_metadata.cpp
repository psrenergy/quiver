#include "database_impl.h"
#include "database_internal.h"

namespace quiver {

ScalarMetadata Database::get_scalar_metadata(const std::string& collection, const std::string& attribute) const {
    impl_->require_collection(collection, "get_scalar_metadata");

    const auto* table_def = impl_->schema->get_table(collection);

    if (!table_def->get_column(attribute)) {
        throw std::runtime_error("Scalar attribute not found: '" + attribute + "' in collection '" + collection + "'");
    }

    return internal::scalar_metadata_with_fk(*table_def, attribute);
}

GroupMetadata Database::get_vector_metadata(const std::string& collection, const std::string& group_name) const {
    const auto& table_def =
        impl_->require_group_table(collection, group_name, GroupTableType::Vector, "get_vector_metadata");

    GroupMetadata metadata;
    metadata.group_name = group_name;

    // Add all data columns in declaration order (skip id and vector_index)
    for (const auto& col_name : table_def.column_order) {
        if (col_name == "id" || col_name == "vector_index") {
            continue;
        }

        metadata.value_columns.push_back(internal::scalar_metadata_with_fk(table_def, col_name));
    }

    return metadata;
}

GroupMetadata Database::get_set_metadata(const std::string& collection, const std::string& group_name) const {
    const auto& table_def = impl_->require_group_table(collection, group_name, GroupTableType::Set, "get_set_metadata");

    GroupMetadata metadata;
    metadata.group_name = group_name;

    // Add all data columns in declaration order (skip id)
    for (const auto& col_name : table_def.column_order) {
        if (col_name == "id") {
            continue;
        }

        metadata.value_columns.push_back(internal::scalar_metadata_with_fk(table_def, col_name));
    }

    return metadata;
}

std::vector<ScalarMetadata> Database::list_scalar_attributes(const std::string& collection) const {
    impl_->require_collection(collection, "list_scalar_attributes");

    const auto* table_def = impl_->schema->get_table(collection);

    std::vector<ScalarMetadata> result;
    for (const auto& col_name : table_def->column_order) {
        result.push_back(internal::scalar_metadata_with_fk(*table_def, col_name));
    }
    return result;
}

std::vector<GroupMetadata> Database::list_vector_groups(const std::string& collection) const {
    impl_->require_collection(collection, "list_vector_groups");

    std::vector<GroupMetadata> result;
    for (const auto& group_name : impl_->schema->group_names(collection, GroupTableType::Vector)) {
        result.push_back(get_vector_metadata(collection, group_name));
    }
    return result;
}

std::vector<GroupMetadata> Database::list_set_groups(const std::string& collection) const {
    impl_->require_collection(collection, "list_set_groups");

    std::vector<GroupMetadata> result;
    for (const auto& group_name : impl_->schema->group_names(collection, GroupTableType::Set)) {
        result.push_back(get_set_metadata(collection, group_name));
    }
    return result;
}

}  // namespace quiver
