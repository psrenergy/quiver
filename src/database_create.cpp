#include "database_impl.h"

namespace quiver {

int64_t Database::create_element(const std::string& collection, const Element& element) {
    impl_->logger->debug("Creating element in collection: {}", collection);
    impl_->require_collection(collection, "create_element");

    const auto& scalars = element.scalars();
    if (scalars.empty()) {
        throw std::runtime_error("Cannot create_element: element must have at least one scalar attribute");
    }

    // Pre-resolve pass: resolve all FK labels before any writes
    auto resolved = impl_->resolve_element_fk_labels(collection, element, *this);

    // Validate resolved scalar types
    for (const auto& [name, value] : resolved.scalars) {
        impl_->type_validator->validate_scalar(collection, name, value);
    }

    Impl::TransactionGuard txn(*impl_);

    // Build INSERT SQL for main collection table
    auto sql = "INSERT INTO " + collection + " (";
    std::string placeholders;
    std::vector<Value> params;

    auto first = true;
    for (const auto& [name, value] : resolved.scalars) {
        if (!first) {
            sql += ", ";
            placeholders += ", ";
        }
        sql += name;
        placeholders += "?";
        params.push_back(value);
        first = false;
    }
    sql += ") VALUES (" + placeholders + ")";

    execute(sql, params);
    const auto element_id = sqlite3_last_insert_rowid(impl_->db);
    impl_->logger->debug("Inserted element with id: {}", element_id);

    // Delegate group insertion to shared helper (empty arrays are skipped silently)
    impl_->insert_group_data("create_element", collection, element_id, resolved.arrays, false, *this);

    txn.commit();
    impl_->logger->info("Created element {} in {}", element_id, collection);
    return element_id;
}

}  // namespace quiver
