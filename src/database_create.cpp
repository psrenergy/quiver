#include "database_impl.h"

namespace quiver {

int64_t Database::create_element(const std::string& collection, const Element& element) {
    impl_->logger->debug("Creating element in collection: {}", collection);
    impl_->require_collection(collection, "create_element");

    const auto& scalars = element.scalars();
    if (scalars.empty()) {
        throw std::runtime_error("Cannot create_element: element must have at least one scalar attribute");
    }

    // Resolve and validate every scalar and array before the INSERT: TransactionGuard no-ops inside
    // a caller-owned transaction or a dry run, so a throw after it would leave the element behind.
    auto resolved = impl_->resolve_scalar_fk_labels(collection, scalars, *this);
    for (const auto& [name, value] : resolved) {
        impl_->type_validator->validate_scalar("create_element", collection, name, value);
    }
    auto groups = impl_->prepare_group_data("create_element", collection, element.arrays(), false, *this);

    Impl::TransactionGuard txn(*impl_);

    // Build INSERT SQL for main collection table
    auto sql = "INSERT INTO " + collection + " (";
    std::string placeholders;
    std::vector<Value> parameters;

    auto first = true;
    for (const auto& [name, value] : resolved) {
        if (!first) {
            sql += ", ";
            placeholders += ", ";
        }
        sql += name;
        placeholders += "?";
        parameters.push_back(value);
        first = false;
    }
    sql += ") VALUES (" + placeholders + ")";

    execute(sql, parameters);
    const auto element_id = sqlite3_last_insert_rowid(impl_->db);
    impl_->logger->debug("Inserted element with id: {}", element_id);

    // prepare_group_data already dropped empty arrays; everything left was validated above.
    impl_->insert_group_data(groups, element_id, false, *this);

    txn.commit();
    impl_->logger->info("Created element {} in {}", element_id, collection);
    return element_id;
}

}  // namespace quiver
