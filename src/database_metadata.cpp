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
    impl_->require_collection(collection, "get_vector_metadata");

    // Find the vector table for this group
    auto vector_table = Schema::vector_table_name(collection, group_name);
    const auto* table_def = impl_->schema->get_table(vector_table);

    if (!table_def) {
        throw std::runtime_error("Vector group not found: '" + group_name + "' in collection '" + collection + "'");
    }

    GroupMetadata metadata;
    metadata.group_name = group_name;

    // Add all data columns in declaration order (skip id and vector_index)
    for (const auto& col_name : table_def->column_order) {
        if (col_name == "id" || col_name == "vector_index") {
            continue;
        }

        metadata.value_columns.push_back(internal::scalar_metadata_with_fk(*table_def, col_name));
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

    // Add all data columns in declaration order (skip id)
    for (const auto& col_name : table_def->column_order) {
        if (col_name == "id") {
            continue;
        }

        metadata.value_columns.push_back(internal::scalar_metadata_with_fk(*table_def, col_name));
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
    impl_->require_schema();

    std::vector<GroupMetadata> result;
    for (const auto& group_name : impl_->schema->group_names(collection, GroupTableType::Vector)) {
        result.push_back(get_vector_metadata(collection, group_name));
    }
    return result;
}

std::vector<GroupMetadata> Database::list_set_groups(const std::string& collection) const {
    impl_->require_schema();

    std::vector<GroupMetadata> result;
    for (const auto& group_name : impl_->schema->group_names(collection, GroupTableType::Set)) {
        result.push_back(get_set_metadata(collection, group_name));
    }
    return result;
}

// -- UI metadata (Phase 3) ---------------------------------------------------------------------
//
// Two-source validation, in this order (D-36 + Pitfall 4): the schema/column check can throw
// (Pattern 2), the sidecar lookup never does (D-41 -- Impl::ui_config is a disengaged
// std::optional on every database outside tests/schemas/ui/, and find_attribute/find_vocabulary
// are UB to call on a disengaged config since both take `const UIConfigSet&`).

UIMetadata Database::get_attribute_ui_metadata(const std::string& collection, const std::string& attribute) const {
    impl_->require_collection(collection, "get_attribute_ui_metadata");

    const auto* table_def = impl_->schema->get_table(collection);
    if (!table_def->get_column(attribute)) {
        throw std::runtime_error("Scalar attribute not found: '" + attribute + "' in collection '" + collection +
                                 "'");
    }

    impl_->require_ui_config();
    if (!impl_->ui_config) {
        return UIMetadata{};
    }
    const auto* meta = find_attribute(*impl_->ui_config, collection, attribute);
    return meta ? *meta : UIMetadata{};
}

std::vector<std::string> Database::list_ui_vocabularies() const {
    impl_->require_ui_config();

    std::vector<std::string> names;
    if (impl_->ui_config) {
        // std::map's key order is already lexicographic, so this iteration order IS sorted order
        // for free -- relied on deliberately; revisit if UIConfigSet::vocabularies ever changes
        // storage type.
        for (const auto& [name, entries] : impl_->ui_config->vocabularies) {
            names.push_back(name);
        }
    }
    return names;
}

std::vector<UIEnumEntry> Database::get_ui_vocabulary(const std::string& name) const {
    impl_->require_ui_config();

    // A disengaged config and an engaged config lacking `name` collapse to the same outcome for
    // the caller -- there is no sidecar-shaped reason to distinguish them, only a name-shaped one.
    const std::vector<UIEnumEntry>* entries = impl_->ui_config ? find_vocabulary(*impl_->ui_config, name) : nullptr;
    if (!entries) {
        throw std::runtime_error("Vocabulary not found: '" + name + "'");
    }
    return *entries;
}

}  // namespace quiver
