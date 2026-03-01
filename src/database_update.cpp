#include "database_impl.h"

namespace quiver {

void Database::update_element(const std::string& collection, int64_t id, const Element& element) {
    impl_->logger->debug("Updating element {} in collection: {}", id, collection);
    impl_->require_collection(collection, "update_element");

    const auto& scalars = element.scalars();
    const auto& arrays = element.arrays();

    if (scalars.empty() && arrays.empty()) {
        throw std::runtime_error("Cannot update_element: element must have at least one attribute to update");
    }

    // Pre-resolve pass: resolve all FK labels before any writes
    auto resolved = impl_->resolve_element_fk_labels(collection, element, *this);

    Impl::TransactionGuard txn(*impl_);

    // Update scalars if present
    if (!resolved.scalars.empty()) {
        // Validate scalar types
        for (const auto& [name, value] : resolved.scalars) {
            impl_->type_validator->validate_scalar(collection, name, value);
        }

        // Build UPDATE SQL
        auto sql = "UPDATE " + collection + " SET ";
        std::vector<Value> params;

        auto first = true;
        for (const auto& [name, value] : resolved.scalars) {
            if (!first) {
                sql += ", ";
            }
            sql += name + " = ?";
            params.push_back(value);
            first = false;
        }
        sql += " WHERE id = ?";
        params.emplace_back(id);

        execute(sql, params);
    }

    // Delegate group insertion to shared helper (delete_existing=true for updates)
    impl_->insert_group_data("update_element", collection, id, resolved.arrays, true, *this);

    txn.commit();
    impl_->logger->info("Updated element {} in {}", id, collection);
}

}  // namespace quiver
