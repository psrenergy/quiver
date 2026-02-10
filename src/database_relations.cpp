#include "database_impl.h"

namespace quiver {

void Database::update_scalar_relation(const std::string& collection,
                                   const std::string& attribute,
                                   const std::string& from_label,
                                   const std::string& to_label) {
    impl_->logger->debug("Setting relation {}.{} from '{}' to '{}'", collection, attribute, from_label, to_label);

    if (!impl_->schema) {
        throw std::runtime_error("Cannot set relation: no schema loaded");
    }

    const auto* table_def = impl_->schema->get_table(collection);
    if (!table_def) {
        throw std::runtime_error("Collection not found in schema: " + collection);
    }

    // Find the foreign key with the given attribute name
    std::string to_table;
    for (const auto& fk : table_def->foreign_keys) {
        if (fk.from_column == attribute) {
            to_table = fk.to_table;
            break;
        }
    }

    if (to_table.empty()) {
        throw std::runtime_error("Attribute '" + attribute + "' is not a foreign key in collection '" + collection +
                                 "'");
    }

    // Look up the target ID by label
    auto lookup_sql = "SELECT id FROM " + to_table + " WHERE label = ?";
    auto lookup_result = execute(lookup_sql, {to_label});
    if (lookup_result.empty() || !lookup_result[0].get_integer(0)) {
        throw std::runtime_error("Target element with label '" + to_label + "' not found in '" + to_table + "'");
    }
    auto to_id = lookup_result[0].get_integer(0).value();

    // Update the source element
    auto update_sql = "UPDATE " + collection + " SET " + attribute + " = ? WHERE label = ?";
    execute(update_sql, {to_id, from_label});

    impl_->logger->info(
        "Set relation {}.{} for '{}' to '{}' (id: {})", collection, attribute, from_label, to_label, to_id);
}

std::vector<std::string> Database::read_scalar_relation(const std::string& collection, const std::string& attribute) {
    if (!impl_->schema) {
        throw std::runtime_error("Cannot read relation: no schema loaded");
    }

    const auto* table_def = impl_->schema->get_table(collection);
    if (!table_def) {
        throw std::runtime_error("Collection not found in schema: " + collection);
    }

    // Find the foreign key with the given attribute name
    std::string to_table;
    for (const auto& fk : table_def->foreign_keys) {
        if (fk.from_column == attribute) {
            to_table = fk.to_table;
            break;
        }
    }

    if (to_table.empty()) {
        throw std::runtime_error("Attribute '" + attribute + "' is not a foreign key in collection '" + collection +
                                 "'");
    }

    // LEFT JOIN to get target labels (NULL for unset relations)
    auto sql = "SELECT t.label FROM " + collection + " c LEFT JOIN " + to_table + " t ON c." + attribute + " = t.id";
    auto result = execute(sql);

    std::vector<std::string> labels;
    labels.reserve(result.row_count());
    for (size_t i = 0; i < result.row_count(); ++i) {
        auto val = result[i].get_string(0);
        labels.push_back(val.value_or(""));
    }
    return labels;
}

}  // namespace quiver
